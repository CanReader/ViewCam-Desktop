#!/usr/bin/env python3
"""Fake ViewCam phone — stands in for the mobile app so the Desktop client can
be tested end-to-end without real hardware.

Implements the Desktop-facing half of design/CONNECTIVITY_PROTOCOL.md:
  * UDP broadcast beacon  "VIEWCAM|<name>|<tcpPort>|<deviceId>"  every 1.5 s
  * TCP server on 8080: on accept, send one HELLO frame, then stream VIDEO
    frames (MJPEG) built from a sample JPEG at ~24 fps.

Usage:
  python3 fake_phone.py [--image PATH] [--name NAME] [--fps N] [--no-stream]

Pure stdlib (no Pillow needed). Ctrl-C to stop.
"""
import argparse
import json
import os
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time
import uuid

# ── Protocol constants (mirror Desktop/include/core/Constants.h) ──
STREAM_PORT = 8080
BEACON_PORT = 8081
BEACON_INTERVAL = 1.5
MAGIC = b"VCAM"
HEADER_SIZE = 24
FMT_MJPEG, FMT_H264, FMT_HELLO, FMT_HEARTBEAT = 0, 1, 2, 3
TYPE_VIDEO, TYPE_CONTROL = 0, 1

DEVICE_ID = str(uuid.uuid4())


def jpeg_size(data: bytes):
    """Return (width, height) by scanning JPEG SOF markers; (0,0) on failure."""
    i = 2  # skip SOI
    n = len(data)
    while i + 9 < n:
        if data[i] != 0xFF:
            i += 1
            continue
        marker = data[i + 1]
        # SOF0..SOF15 (except DHT=C4, JPG=C8, DAC=CC) carry dimensions
        if 0xC0 <= marker <= 0xCF and marker not in (0xC4, 0xC8, 0xCC):
            height = struct.unpack(">H", data[i + 5:i + 7])[0]
            width = struct.unpack(">H", data[i + 7:i + 9])[0]
            return width, height
        seg_len = struct.unpack(">H", data[i + 2:i + 4])[0]
        i += 2 + seg_len
    return 0, 0


def load_as_jpeg(path, quality):
    """Return JPEG bytes. The design sample is actually a PNG, and the protocol
    requires MJPEG/JPEG VIDEO payloads, so transcode via ffmpeg when needed."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:2] == b"\xff\xd8":  # already JPEG (SOI)
        return data
    if not shutil.which("ffmpeg"):
        sys.exit(f"{path} is not JPEG and ffmpeg is unavailable to transcode it")
    # ffmpeg -q:v: 2 (best) .. 31 (worst); map ~quality 70 -> ~q 6
    q = max(2, min(31, round((100 - quality) / 100 * 31)))
    out = subprocess.run(
        ["ffmpeg", "-y", "-loglevel", "error", "-i", path,
         "-frames:v", "1", "-f", "mjpeg", "-q:v", str(q), "pipe:1"],
        capture_output=True, check=True).stdout
    print(f"[init] transcoded {os.path.basename(path)} -> JPEG "
          f"({len(out)} bytes, q={q})")
    return out


def make_header(payload_len, width, height, fmt, ftype, ts_us):
    # 24-byte little-endian header (see protocol §4)
    return (MAGIC
            + struct.pack("<I", payload_len)
            + struct.pack("<Q", ts_us)
            + struct.pack("<H", width)
            + struct.pack("<H", height)
            + struct.pack("<B", fmt)
            + struct.pack("<B", ftype)
            + struct.pack("<H", 0))  # reserved


def now_us():
    return int(time.monotonic() * 1_000_000)


def beacon_loop(name, stop):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    msg = f"VIEWCAM|{name}|{STREAM_PORT}|{DEVICE_ID}".encode("utf-8")
    print(f"[beacon] broadcasting '{msg.decode()}' every {BEACON_INTERVAL}s")
    while not stop.is_set():
        try:
            sock.sendto(msg, ("255.255.255.255", BEACON_PORT))
        except OSError as e:
            print(f"[beacon] send failed: {e}")
        stop.wait(BEACON_INTERVAL)
    sock.close()


def serve(args, jpeg, width, height):
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", STREAM_PORT))
    srv.listen(1)
    print(f"[tcp] listening on 0.0.0.0:{STREAM_PORT}")
    while True:
        conn, addr = srv.accept()
        print(f"[tcp] client connected: {addr}")
        try:
            # HELLO immediately on accept. battery == -1 means "unknown" (the
            # desktop must render "—", never the raw -1).
            hello = json.dumps({
                "name": args.name, "os": "FakeOS 1.0",
                "maxW": width, "maxH": height, "deviceId": DEVICE_ID,
                "battery": args.battery, "charging": args.charging,
                "lens": args.lens,
            }).encode("utf-8")
            conn.sendall(make_header(len(hello), 0, 0,
                                     FMT_HELLO, TYPE_CONTROL, now_us()) + hello)
            print(f"[tcp] sent HELLO ({len(hello)} bytes, "
                  f"battery={args.battery} charging={args.charging})")

            # STATUS frame (format=3/type=1 with a JSON body) — periodic live
            # battery/charging push. A zero-length type=3 frame stays a heartbeat.
            batt = [args.battery]
            def send_status():
                # simulate a slow battery drift so live updates are observable
                if batt[0] > 0 and not args.charging:
                    batt[0] = max(1, batt[0] - 1)
                body = json.dumps({"battery": batt[0],
                                   "charging": args.charging,
                                   "lens": args.lens}).encode("utf-8")
                conn.sendall(make_header(len(body), 0, 0, FMT_HEARTBEAT,
                                         TYPE_CONTROL, now_us()) + body)
                print(f"[tcp] sent STATUS (battery={batt[0]} "
                      f"charging={args.charging})")

            next_status = time.monotonic() + 5.0

            if args.no_stream:
                # idle: heartbeats + periodic STATUS, until the client disconnects
                while True:
                    if time.monotonic() >= next_status:
                        send_status()
                        next_status += 5.0
                    else:
                        conn.sendall(make_header(0, 0, 0, FMT_HEARTBEAT,
                                                 TYPE_CONTROL, now_us()))
                    time.sleep(1.0)

            # stream VIDEO frames, interleaving a STATUS push every ~5 s
            period = 1.0 / args.fps
            count = 0
            while True:
                hdr = make_header(len(jpeg), width, height,
                                  FMT_MJPEG, TYPE_VIDEO, now_us())
                conn.sendall(hdr + jpeg)
                count += 1
                if count % args.fps == 0:
                    print(f"[tcp] streamed {count} VIDEO frames "
                          f"({width}x{height}, {len(jpeg)} bytes each)")
                if time.monotonic() >= next_status:
                    send_status()
                    next_status += 5.0
                time.sleep(period)
        except (BrokenPipeError, ConnectionResetError, OSError) as e:
            print(f"[tcp] client gone: {e}")
        finally:
            conn.close()


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    default_img = os.path.normpath(os.path.join(
        here, "..", "..", "ViewCam Design System", "assets",
        "viewfinder-sample.jpg"))

    ap = argparse.ArgumentParser()
    ap.add_argument("--image", default=default_img)
    ap.add_argument("--name", default="Fake Pixel 8")
    ap.add_argument("--fps", type=int, default=24)
    ap.add_argument("--battery", type=int, default=86,
                    help="battery %% in HELLO/STATUS (-1 = unknown -> desktop shows —)")
    ap.add_argument("--charging", action="store_true",
                    help="report the phone as charging")
    ap.add_argument("--lens", default="Back · ƒ1.8",
                    help="active-camera descriptor in HELLO/STATUS (empty -> desktop shows —)")
    ap.add_argument("--quality", type=int, default=70,
                    help="JPEG quality if the source needs transcoding")
    ap.add_argument("--no-stream", action="store_true",
                    help="send HELLO + heartbeats but no video (idle state)")
    ap.add_argument("--duration", type=float, default=0,
                    help="auto-exit after N seconds (0 = run forever)")
    args = ap.parse_args()

    if not os.path.isfile(args.image):
        sys.exit(f"sample image not found: {args.image}")
    jpeg = load_as_jpeg(args.image, args.quality)
    width, height = jpeg_size(jpeg)
    if width == 0:
        width, height = 1280, 720
    print(f"[init] image={args.image} {width}x{height} "
          f"{len(jpeg)} bytes  deviceId={DEVICE_ID}")

    stop = threading.Event()
    threading.Thread(target=beacon_loop, args=(args.name, stop),
                     daemon=True).start()
    threading.Thread(target=serve, args=(args, jpeg, width, height),
                     daemon=True).start()
    try:
        if args.duration > 0:
            time.sleep(args.duration)
            print(f"[exit] duration {args.duration}s elapsed")
        else:
            while True:
                time.sleep(3600)
    except KeyboardInterrupt:
        print("\n[exit] stopping")
    stop.set()


if __name__ == "__main__":
    main()
