#!/usr/bin/env bash
# release.sh — sign ViewCam release artifact(s) + emit/merge dist/manifest.json.
#
# Pairs with the `viewcam_package` CMake target (Linux .zip) and the Inno Setup
# build (Windows Setup.exe). Run it AFTER building the artifact, once per
# platform you have an artifact for. It MERGES into an existing manifest.json so
# you can run it on Linux (for the .zip) and on a Windows host (for the .exe)
# against the same manifest — each run fills only the platform entries for the
# artifacts it finds, preserving the others.
#
# PER-PLATFORM ARTIFACT (the "apply" unit differs by OS, the verify is identical):
#   linux-x86_64    ViewCam-<ver>-linux-x86_64.zip   (versioned-swap package)
#   windows-x86_64  ViewCam-<ver>-win-x64-Setup.exe  (Inno Setup installer)
#
# WHAT IT SIGNS: the raw artifact BYTES (sign the file, not the manifest), with
# Ed25519, matching the verify-only client — SAME scheme for .zip and .exe:
#     openssl pkeyutl -sign -inkey <priv> -rawin -in <artifact> -out <sig.bin>
#   then base64(sig.bin) -> manifest "signature".
# Client recomputes SHA-256 (integrity) + verifies Ed25519 (authenticity) over
# the downloaded artifact against the embedded public key before using it
# (Linux: extract the zip; Windows: launch the verified Setup.exe).
#
# USAGE:
#   scripts/release.sh [VERSION] [CHANNEL]
#     VERSION   release version (default: read from Desktop/CMakeLists.txt project())
#     CHANNEL   stable | beta   (default: stable)
#
# ENV OVERRIDES:
#   VIEWCAM_SIGN_KEY   private key path   (default: release/update-signing-key.pem)
#   VIEWCAM_DIST_DIR   dir with the zips  (default: dist/)
#   VIEWCAM_BASE_URL   artifact base URL  (default: https://dl.viewcam.tech/<ver>/)
#   VIEWCAM_NOTES_URL  release-notes URL  (default: https://viewcam.tech/releases/<ver>)
#   VIEWCAM_MIN_VER    min_version        (default: <ver>)
#   VIEWCAM_ROLLOUT    0..1 staged ratio  (default: 1.0)
#   VIEWCAM_PUB_DATE   ISO-8601 UTC       (default: now)
#
# WINDOWS: build the Setup.exe first on a Windows host with Inno Setup 6:
#     iscc installer\viewcam_setup.iss
#   then copy the produced ViewCamStudio-<ver>-Setup.exe to
#   dist/ViewCam-<ver>-win-x64-Setup.exe and run this script under Git Bash /
#   MSYS2 (provides bash + openssl + base64 + sha256sum). The signing logic is
#   identical to the .zip; only the artifact it finds differs.
#
# Never prints the private key. Fails loudly if the key is missing.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"   # Desktop/

die() { echo "release.sh: ERROR: $*" >&2; exit 1; }
info() { echo "release.sh: $*" >&2; }

# ── Resolve version (arg, else parse CMakeLists.txt project() VERSION) ─────────
VERSION="${1:-}"
if [ -z "${VERSION}" ]; then
    VERSION="$(grep -oE 'VERSION[[:space:]]+[0-9]+\.[0-9]+\.[0-9]+' "${ROOT_DIR}/CMakeLists.txt" \
                | head -n1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' || true)"
    [ -n "${VERSION}" ] || die "could not read VERSION from CMakeLists.txt; pass it explicitly"
fi
CHANNEL="${2:-stable}"

DIST_DIR="${VIEWCAM_DIST_DIR:-${ROOT_DIR}/dist}"
SIGN_KEY="${VIEWCAM_SIGN_KEY:-${ROOT_DIR}/release/update-signing-key.pem}"
BASE_URL="${VIEWCAM_BASE_URL:-https://dl.viewcam.tech/${VERSION}/}"
NOTES_URL="${VIEWCAM_NOTES_URL:-https://viewcam.tech/releases/${VERSION}}"
MIN_VER="${VIEWCAM_MIN_VER:-${VERSION}}"
ROLLOUT="${VIEWCAM_ROLLOUT:-1.0}"
PUB_DATE="${VIEWCAM_PUB_DATE:-$(date -u +%Y-%m-%dT%H:%M:%SZ)}"

# normalize base url to end with a single slash
BASE_URL="${BASE_URL%/}/"

MANIFEST="${DIST_DIR}/manifest.json"

# ── Preconditions ─────────────────────────────────────────────────────────────
[ -f "${SIGN_KEY}" ] || die "private signing key not found: ${SIGN_KEY}
  (set VIEWCAM_SIGN_KEY, or place it at release/update-signing-key.pem — never commit it)"
[ -d "${DIST_DIR}" ] || die "dist dir not found: ${DIST_DIR} (run the viewcam_package target first)"
command -v openssl   >/dev/null || die "openssl not found"
command -v sha256sum >/dev/null || command -v shasum >/dev/null || die "need sha256sum or shasum"
command -v base64    >/dev/null || die "base64 not found"
command -v python3   >/dev/null || die "python3 not found (used for manifest merge)"

sha256_hex() {  # portable sha256 -> bare hex
    if command -v sha256sum >/dev/null; then sha256sum "$1" | cut -d' ' -f1
    else shasum -a 256 "$1" | cut -d' ' -f1; fi
}
file_size() { # portable byte size
    if stat -c%s "$1" >/dev/null 2>&1; then stat -c%s "$1"; else stat -f%z "$1"; fi
}
b64() { base64 "$1" | tr -d '\n'; }   # base64 without line wraps (GNU+BSD)

# Map a release artifact filename to its manifest platform KEY (brief §2 schema).
# Linux ships a .zip (versioned-swap); Windows ships a signed Setup.exe.
manifest_key_for() {
    case "$1" in
        *-linux-x86_64.zip)   echo "linux-x86_64" ;;
        *-win-x64-Setup.exe)  echo "windows-x86_64" ;;
        *-win-x64.exe)        echo "windows-x86_64" ;;  # alt naming, tolerated
        *) echo "" ;;
    esac
}

# ── Collect artifacts for this version (Linux .zip + Windows Setup.exe) ───────
shopt -s nullglob
ARTIFACTS=( "${DIST_DIR}/ViewCam-${VERSION}-linux-x86_64.zip" \
            "${DIST_DIR}/ViewCam-${VERSION}-win-x64-Setup.exe" )
FOUND=()
for a in "${ARTIFACTS[@]}"; do [ -f "$a" ] && FOUND+=( "$a" ); done
[ "${#FOUND[@]}" -gt 0 ] || \
    die "no ViewCam-${VERSION} artifact in ${DIST_DIR}
  (expected the Linux .zip from viewcam_package and/or the Windows Setup.exe from iscc)"

info "version=${VERSION} channel=${CHANNEL} dist=${DIST_DIR}"
info "signing with key: ${SIGN_KEY}  (contents never printed)"

# ── Sign each present platform artifact; build python kwargs for the merge ────
PLAT_ARGS=()
for artifact in "${FOUND[@]}"; do
    fname="$(basename "$artifact")"
    key="$(manifest_key_for "$fname")"
    [ -n "$key" ] || die "cannot map platform for ${fname}"

    sha="$(sha256_hex "$artifact")"
    size="$(file_size "$artifact")"

    sigbin="$(mktemp)"
    # Ed25519 detached signature over the RAW artifact bytes (rawin = no prehash).
    # Identical for a .zip and a Setup.exe — the client verifies the same way.
    openssl pkeyutl -sign -inkey "${SIGN_KEY}" -rawin -in "$artifact" -out "$sigbin" \
        || { rm -f "$sigbin"; die "openssl signing failed for ${fname}"; }
    sig="$(b64 "$sigbin")"
    rm -f "$sigbin"

    url="${BASE_URL}${fname}"
    info "  ${key}: size=${size} sha256=${sha} sig=${#sig}B64chars url=${url}"
    PLAT_ARGS+=( "${key}|${url}|${sha}|${size}|${sig}" )
done

# ── Merge into manifest.json (create or update; preserve other platforms) ─────
PLATS="$(printf '%s\n' "${PLAT_ARGS[@]}")" \
MANIFEST="$MANIFEST" CHANNEL="$CHANNEL" VERSION="$VERSION" MIN_VER="$MIN_VER" \
PUB_DATE="$PUB_DATE" NOTES_URL="$NOTES_URL" ROLLOUT="$ROLLOUT" \
python3 - <<'PY'
import json, os

path = os.environ["MANIFEST"]
m = {}
if os.path.exists(path):
    with open(path) as f:
        try: m = json.load(f)
        except Exception: m = {}

m["channel"]     = os.environ["CHANNEL"]
m["version"]     = os.environ["VERSION"]
m["min_version"] = os.environ["MIN_VER"]
m["pub_date"]    = os.environ["PUB_DATE"]
m["notes_url"]   = os.environ["NOTES_URL"]
m["rollout"]     = float(os.environ["ROLLOUT"])
plats = m.get("platforms")
if not isinstance(plats, dict):
    plats = {}

# Seed clear placeholders so a single-platform run still emits a complete,
# obviously-incomplete schema for the missing platform (filled by its own host).
for k in ("linux-x86_64", "windows-x86_64"):
    plats.setdefault(k, {"url": "", "sha256": "", "size": 0, "signature": ""})

for line in os.environ["PLATS"].splitlines():
    if not line.strip():
        continue
    key, url, sha, size, sig = line.split("|", 4)
    plats[key] = {"url": url, "sha256": sha, "size": int(size), "signature": sig}

m["platforms"] = {k: plats[k] for k in ("linux-x86_64", "windows-x86_64") if k in plats}

with open(path, "w") as f:
    json.dump(m, f, indent=2)
    f.write("\n")
print("wrote " + path)
PY

info "manifest: ${MANIFEST}"
PLACEHOLDER="$(python3 - "$MANIFEST" <<'PY'
import json,sys
m=json.load(open(sys.argv[1]))
miss=[k for k,v in m.get("platforms",{}).items() if not v.get("signature")]
print(",".join(miss))
PY
)"
if [ -n "${PLACEHOLDER}" ]; then
    info "NOTE: placeholder (unsigned) platform(s): ${PLACEHOLDER}"
    info "      run release.sh on that platform's build host to fill it in."
fi
info "done."
