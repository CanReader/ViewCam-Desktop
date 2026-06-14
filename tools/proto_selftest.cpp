// proto_selftest — single-process end-to-end proof of the Desktop connectivity
// path against an in-process "fake phone" that speaks CONNECTIVITY_PROTOCOL.md.
//
// Exercises the REAL Desktop classes: DeviceDiscovery (UDP beacon parse),
// StreamReceiver (24-byte header + HELLO/VIDEO dispatch), FrameDecoder
// (JPEG→QImage) and V4L2LoopbackWriter (virtual camera write + readback).
//
// Runs headless, self-terminates. Exit 0 = PASS.
#include "core/Constants.h"
#include "core/Logger.h"
#include "network/DeviceDiscovery.h"
#include "network/FrameDecoder.h"
#include "network/StreamReceiver.h"
#include "virtualcam/V4L2LoopbackWriter.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>
#include <QtEndian>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>

static QByteArray makeHeader(quint32 payloadLen, quint16 w, quint16 h,
                             vc::FrameFormat fmt, vc::FrameType type) {
  QByteArray hdr(vc::kFrameHeaderSize, 0);
  char *d = hdr.data();
  memcpy(d, vc::kFrameMagic, 4);
  qToLittleEndian<quint32>(payloadLen, d + vc::hdr::kPayloadLen);
  qToLittleEndian<quint64>(0, d + vc::hdr::kTimestamp);
  qToLittleEndian<quint16>(w, d + vc::hdr::kWidth);
  qToLittleEndian<quint16>(h, d + vc::hdr::kHeight);
  d[vc::hdr::kFormat] = static_cast<char>(fmt);
  d[vc::hdr::kType] = static_cast<char>(type);
  return hdr;
}

int main(int argc, char *argv[]) {
  Logger::init(Logger::Level::Info);
  QCoreApplication app(argc, argv);

  const QString sample =
      argc > 1 ? argv[1]
               : "../ViewCam Design System/assets/viewfinder-sample.jpg";

  // Build a JPEG payload exactly as the phone would (sample is a PNG → JPEG).
  QImage img(sample);
  if (img.isNull()) {
    VC_CRITICAL("Cannot load sample image: {}", sample.toStdString());
    return 1;
  }
  QByteArray jpeg;
  {
    QBuffer buf(&jpeg);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPEG", vc::kDefaultJpegQuality);
  }
  const quint16 W = static_cast<quint16>(img.width());
  const quint16 H = static_cast<quint16>(img.height());
  VC_INFO("Sample {}x{} -> JPEG {} bytes", W, H, jpeg.size());

  // Result flags
  bool sawCard = false, sawHello = false, sawStatus = false;
  int helloBattery = -999, statusBattery = -999;
  bool helloCharging = false, statusCharging = true;
  bool sawControlAck = false, ackTorch = false, ackHdrSupported = true;
  QString helloLens;
  int framesDecoded = 0;
  const QString testId = "test-device-001";

  // ── In-process fake phone: TCP server (HELLO + VIDEO loop) ──
  QTcpServer server;
  if (!server.listen(QHostAddress::LocalHost, vc::kStreamPort)) {
    VC_CRITICAL("TCP listen failed: {}", server.errorString().toStdString());
    return 1;
  }
  QObject::connect(&server, &QTcpServer::newConnection, [&]() {
    auto *c = server.nextPendingConnection();
    VC_INFO("[phone] client connected, sending HELLO");
    const QByteArray hello =
        R"({"name":"Test Phone","os":"FakeOS 1.0","maxW":1920,"maxH":1080,)"
        R"("battery":90,"charging":true,"lens":"Back · ƒ1.8"})";
    c->write(makeHeader(hello.size(), 0, 0, vc::FrameFormat::Hello,
                        vc::FrameType::Control));
    c->write(hello);
    auto *t = new QTimer(c);
    QObject::connect(t, &QTimer::timeout, [c, &jpeg, W, H]() {
      c->write(makeHeader(jpeg.size(), W, H, vc::FrameFormat::Mjpeg,
                          vc::FrameType::Video));
      c->write(jpeg);
    });
    t->start(40); // ~25 fps

    // One STATUS frame (format=3/type=1, JSON body) — live battery push.
    QTimer::singleShot(150, c, [c]() {
      const QByteArray status = R"({"battery":77,"charging":false})";
      c->write(makeHeader(status.size(), 0, 0, vc::FrameFormat::Heartbeat,
                          vc::FrameType::Control));
      c->write(status);
      VC_INFO("[phone] sent STATUS battery=77");
    });

    // Phone reads Desktop->Phone CONTROL frames (format=4) off the SAME socket,
    // then acknowledges applied state in a STATUS controls{} (reporting HDR as
    // unsupported, to prove the desktop disables that toggle).
    auto *ctrlBuf = new QByteArray();
    QObject::connect(c, &QTcpSocket::readyRead, c, [c, ctrlBuf]() {
      ctrlBuf->append(c->readAll());
      while (ctrlBuf->size() >= vc::kFrameHeaderSize) {
        const auto *d = reinterpret_cast<const uchar *>(ctrlBuf->constData());
        if (std::memcmp(d, vc::kFrameMagic, 4) != 0) { ctrlBuf->remove(0, 1); continue; }
        const quint32 plen = qFromLittleEndian<quint32>(d + vc::hdr::kPayloadLen);
        const int total = vc::kFrameHeaderSize + static_cast<int>(plen);
        if (ctrlBuf->size() < total) break;
        const auto fmt = static_cast<vc::FrameFormat>(d[vc::hdr::kFormat]);
        const QByteArray body = ctrlBuf->mid(vc::kFrameHeaderSize, plen);
        ctrlBuf->remove(0, total);
        if (fmt != vc::FrameFormat::Control) continue;
        VC_INFO("[phone] got CONTROL: {}", body.constData());
        QJsonObject controls = QJsonDocument::fromJson(body).object();
        controls["hdrSupported"] = false; // phone says HDR unsupported
        const QByteArray ack = QJsonDocument(
            QJsonObject{{"battery", 77}, {"charging", false},
                        {"controls", controls}}).toJson(QJsonDocument::Compact);
        c->write(makeHeader(ack.size(), 0, 0, vc::FrameFormat::Heartbeat,
                            vc::FrameType::Control));
        c->write(ack);
        VC_INFO("[phone] sent STATUS controls ack (hdrSupported=false)");
      }
    });
  });

  // ── Real Desktop client pieces ──
  DeviceDiscovery discovery;
  FrameDecoder decoder;
  StreamReceiver receiver;
  V4L2LoopbackWriter writer;
  const bool vcamOpen = writer.open();
  VC_INFO("Virtual camera open: {} ({})", vcamOpen,
          writer.devicePath());

  QObject::connect(&discovery, &DeviceDiscovery::deviceFound,
                   [&](const QString &id, const QString &name,
                       const QString &host, int port) {
                     VC_INFO("[client] discovered {} '{}' {}:{}",
                             id.toStdString(), name.toStdString(),
                             host.toStdString(), port);
                     if (id == testId)
                       sawCard = true;
                   });
  QObject::connect(&receiver, &StreamReceiver::helloReceived,
                   [&](const QString &name, const QString &os, int, int,
                       int battery, bool charging, const QString &lens) {
                     VC_INFO("[client] HELLO name='{}' os='{}' battery={} "
                             "charging={} lens='{}'", name.toStdString(),
                             os.toStdString(), battery, charging,
                             lens.toStdString());
                     sawHello = true;
                     helloBattery = battery;
                     helloCharging = charging;
                     helloLens = lens;
                     // Desktop -> Phone CONTROL on the same socket.
                     receiver.sendControl(
                         QJsonObject{{"torch", true}, {"hdr", true}});
                   });
  QObject::connect(&receiver, &StreamReceiver::statusReceived,
                   [&](int battery, bool charging) {
                     VC_INFO("[client] STATUS battery={} charging={}", battery,
                             charging);
                     sawStatus = true;
                     statusBattery = battery;
                     statusCharging = charging;
                   });
  QObject::connect(&receiver, &StreamReceiver::controlStateReceived,
                   [&](const QJsonObject &controls) {
                     VC_INFO("[client] CONTROL ack: {}",
                             QJsonDocument(controls)
                                 .toJson(QJsonDocument::Compact)
                                 .constData());
                     sawControlAck = true;
                     ackTorch = controls.value("torch").toBool();
                     ackHdrSupported = controls.value("hdrSupported").toBool(true);
                   });
  QObject::connect(&receiver, &StreamReceiver::frameReceived, &decoder,
                   &FrameDecoder::decode);
  QObject::connect(&decoder, &FrameDecoder::imageReady,
                   [&](const QImage &decoded) {
                     ++framesDecoded;
                     if (vcamOpen)
                       writer.writeFrame(decoded);
                   });

  discovery.start();
  receiver.connectToHost("127.0.0.1", vc::kStreamPort);

  // Send a few UDP beacons to ourselves so DeviceDiscovery parses one.
  auto *beaconSock = new QUdpSocket(&app);
  const QByteArray beacon =
      QStringLiteral("VIEWCAM|Test Phone|%1|%2")
          .arg(vc::kStreamPort)
          .arg(testId)
          .toUtf8();
  auto *beaconTimer = new QTimer(&app);
  QObject::connect(beaconTimer, &QTimer::timeout, [&]() {
    beaconSock->writeDatagram(beacon, QHostAddress::LocalHost, vc::kBeaconPort);
  });
  beaconTimer->start(300);

  // ── Evaluate once enough frames have flowed ──
  auto finish = [&](int code) {
    // best-effort virtual-camera readback (v4l2loopback supports read())
    bool vcamReadback = false;
    if (vcamOpen) {
      int fd = ::open(writer.devicePath().c_str(), O_RDONLY | O_NONBLOCK);
      if (fd >= 0) {
        QByteArray buf(W * H * 2, 0); // YUYV = 2 bytes/px
        for (int i = 0; i < 50 && !vcamReadback; ++i) {
          ssize_t n = ::read(fd, buf.data(), buf.size());
          if (n > 0)
            vcamReadback = true;
          else
            usleep(20000);
        }
        ::close(fd);
      }
    }

    const bool batteryOk = helloBattery == 90 && helloCharging &&
                           statusBattery == 77 && !statusCharging;
    const bool lensOk = helloLens == QString::fromUtf8("Back · ƒ1.8");
    // CONTROL round-trip: sent {torch:true} → phone echoed it + hdrSupported=false
    const bool controlOk = sawControlAck && ackTorch && !ackHdrSupported;

    VC_INFO("──────── RESULT ────────");
    VC_INFO("discovery card parsed : {}", sawCard);
    VC_INFO("HELLO received        : {} (battery={}, charging={})", sawHello,
            helloBattery, helloCharging);
    VC_INFO("STATUS received       : {} (battery={}, charging={})", sawStatus,
            statusBattery, statusCharging);
    VC_INFO("battery/charging ok   : {}", batteryOk);
    VC_INFO("lens parsed ok        : {} ('{}')", lensOk, helloLens.toStdString());
    VC_INFO("CONTROL round-trip ok : {} (torch={}, hdrSupported={})", controlOk,
            ackTorch, ackHdrSupported);
    VC_INFO("frames decoded        : {}", framesDecoded);
    VC_INFO("vcam device opened    : {} ({})", vcamOpen, writer.devicePath());
    VC_INFO("vcam frame read back  : {}", vcamReadback);

    const bool pass = sawCard && sawHello && sawStatus && batteryOk && lensOk &&
                      controlOk && framesDecoded >= 5 && vcamOpen;
    VC_INFO("{}", pass ? "PASS ✓" : "FAIL ✗");
    app.exit(pass ? 0 : (code ? code : 1));
  };

  QTimer::singleShot(2500, [&]() { finish(0); });
  QTimer::singleShot(8000, [&]() {
    VC_CRITICAL("timeout");
    finish(2);
  });

  return app.exec();
}
