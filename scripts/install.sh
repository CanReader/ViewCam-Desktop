#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────────────────
# install.sh — per-user first-run installer for ViewCam (Linux).
#
# Run from the root of an extracted release zip (the dir holding ViewCam,
# viewcam.sh, vc-updater, lib/, plugins/, qml/, viewcam.rcc, VERSION). It lays
# down the SAME per-user versioned layout the self-updater expects so a fresh
# install and a self-updated install are byte-identical in structure:
#
#   ~/.local/share/ViewCam/                 (install root — matches InstallLayout.cpp)
#     versions/<version>/                   the full app folder (this zip's contents)
#     current            -> versions/<version>   (relative symlink, atomic swap)
#     vc-updater                            the helper, copied to the ROOT
#     update.log                            (written later by vc-updater)
#
# It also installs a .desktop launcher + icon into the per-user XDG dirs, with
# Exec pointing at the STABLE  <root>/current/viewcam.sh  path so it keeps
# working unchanged as `current` is repointed by every future self-update.
#
# Idempotent: re-running with a newer VERSION drops a new versions/<new> dir and
# repoints `current` atomically without disturbing a running instance.
#
# POSIX-ish bash. No root, no sudo, no system dirs touched.
# ──────────────────────────────────────────────────────────────────────────
set -euo pipefail

# ── Resolve the directory this script lives in (= extracted zip root) ──────
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"

# ── Per-user XDG locations (honour overrides; default to the spec paths) ───
XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
INSTALL_ROOT="$XDG_DATA_HOME/ViewCam"            # == InstallLayout::installRoot()
VERSIONS_DIR="$INSTALL_ROOT/versions"            # == InstallLayout::versionsDir()
CURRENT_LINK="$INSTALL_ROOT/current"             # == InstallLayout::currentLink()
APPLICATIONS_DIR="$XDG_DATA_HOME/applications"
ICONS_DIR="$XDG_DATA_HOME/icons/hicolor"
DESKTOP_FILE="$APPLICATIONS_DIR/viewcam.desktop"
DESKTOP_ID="viewcam"

# The app folder content that must land under versions/<ver>/. The icon/install
# tooling that rides along in the zip is harmless inside a version dir but we
# only NEED these to be a runnable app.
REQUIRED=(ViewCam viewcam.sh vc-updater viewcam.rcc)

die()  { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }
info() { printf '\033[36m::\033[0m %s\n' "$*"; }
ok()   { printf '\033[32m✓\033[0m %s\n'  "$*"; }

# ── 1. Determine the version ───────────────────────────────────────────────
# Source of truth, simplest-robust-first:
#   1. explicit  --version X.Y.Z  (lets a caller/CI override)
#   2. a VERSION file shipped in the zip root (written by Package.cmake)
#   3. fall back to refusing — we never guess, a wrong version dir would shadow
#      a real one and confuse the updater's QVersionNumber pruning.
VERSION=""
while [ $# -gt 0 ]; do
    case "$1" in
        --version) VERSION="${2:-}"; shift 2 ;;
        --version=*) VERSION="${1#*=}"; shift ;;
        -h|--help)
            cat <<EOF
Usage: install.sh [--version X.Y.Z]
Installs ViewCam for the current user under $INSTALL_ROOT.
Run it from the root of an extracted ViewCam release .zip.
EOF
            exit 0 ;;
        *) die "unknown argument: $1" ;;
    esac
done

if [ -z "$VERSION" ] && [ -f "$SRC_DIR/VERSION" ]; then
    VERSION="$(tr -d '[:space:]' < "$SRC_DIR/VERSION")"
fi
[ -n "$VERSION" ] || die "no version: pass --version X.Y.Z or ship a VERSION file in the zip"
# Basic sanity: a version dir name must not contain a path separator.
case "$VERSION" in */*|"") die "invalid version string: '$VERSION'" ;; esac

# ── 2. Validate we're really sitting on an extracted release ───────────────
for f in "${REQUIRED[@]}"; do
    [ -e "$SRC_DIR/$f" ] || die "missing '$f' in '$SRC_DIR' — run this from an extracted ViewCam release zip"
done

info "Installing ViewCam $VERSION for $(whoami) → $INSTALL_ROOT"

TARGET_VERSION_DIR="$VERSIONS_DIR/$VERSION"

# ── 3. Lay down versions/<version>/ ────────────────────────────────────────
mkdir -p "$VERSIONS_DIR"

# Fresh dir each time so an interrupted prior copy can't leave a half-version.
# Copy into a temp sibling then atomically rename into place.
STAGE="$VERSIONS_DIR/.stage-$VERSION.$$"
rm -rf "$STAGE"
mkdir -p "$STAGE"

# Copy the app folder. Exclude the install tooling + the symlink/log artifacts
# of any nested install; everything else (ViewCam, viewcam.sh, vc-updater,
# viewcam.rcc, lib/, plugins/, qml/, icon, VERSION) goes in. Tooling files left
# in a version dir are harmless (the updater ignores them).
#
# Use a copy that preserves the executable bits and follows no surprises.
( cd "$SRC_DIR" && \
  find . -mindepth 1 -maxdepth 1 \
        ! -name 'install.sh' \
        ! -name 'uninstall.sh' \
        -exec cp -a {} "$STAGE/" \; )

# Force the +x bits the updater also enforces on these two (zip may drop modes).
chmod +x "$STAGE/ViewCam" "$STAGE/vc-updater" "$STAGE/viewcam.sh" 2>/dev/null || true

if [ -d "$TARGET_VERSION_DIR" ]; then
    info "Version $VERSION already present — refreshing it"
    rm -rf "$TARGET_VERSION_DIR"
fi
mv "$STAGE" "$TARGET_VERSION_DIR"
ok "Laid down versions/$VERSION/"

# ── 4. Copy vc-updater to the install ROOT ─────────────────────────────────
# It must live at the root (never inside a version dir) so it can swap the dir
# the app runs from — same invariant as InstallLayout.h / vc_updater_main.cpp.
cp -a "$TARGET_VERSION_DIR/vc-updater" "$INSTALL_ROOT/vc-updater.new.$$"
chmod +x "$INSTALL_ROOT/vc-updater.new.$$"
mv -f "$INSTALL_ROOT/vc-updater.new.$$" "$INSTALL_ROOT/vc-updater"
ok "Installed vc-updater at install root"

# ── 5. Atomically (re)point current -> versions/<version> ──────────────────
# RELATIVE target ("versions/<v>") so the install survives being moved, and a
# temp-symlink + rename() so a reader never sees a half-updated link. This is
# byte-for-byte the convention in InstallLayout::activateVersion().
TMP_LINK="$CURRENT_LINK.new.$$"
rm -f "$TMP_LINK"
ln -s "versions/$VERSION" "$TMP_LINK"
mv -fT "$TMP_LINK" "$CURRENT_LINK"
ok "current -> versions/$VERSION"

# ── 6. Install the icon (best-effort, non-fatal) ───────────────────────────
# Prefer the scalable SVG; also drop a 512px PNG for environments that ignore
# scalable. Both ship in the zip root (Package.cmake stages them as
# viewcam.svg / viewcam.png).
install_icon() {
    local installed=0
    if [ -f "$SRC_DIR/viewcam.svg" ]; then
        mkdir -p "$ICONS_DIR/scalable/apps"
        cp -f "$SRC_DIR/viewcam.svg" "$ICONS_DIR/scalable/apps/viewcam.svg" && installed=1
    fi
    if [ -f "$SRC_DIR/viewcam.png" ]; then
        mkdir -p "$ICONS_DIR/512x512/apps"
        cp -f "$SRC_DIR/viewcam.png" "$ICONS_DIR/512x512/apps/viewcam.png" && installed=1
    fi
    [ "$installed" = 1 ] && ok "Installed app icon" || info "No icon shipped in zip — skipping"
}
install_icon

# ── 7. Install the .desktop launcher ───────────────────────────────────────
# Exec uses the STABLE  <root>/current/viewcam.sh  path. viewcam.sh sets
# LD_LIBRARY_PATH/QT_PLUGIN_PATH/QML_IMPORT_PATH for the bundled Qt — the raw
# ViewCam binary has NO $ORIGIN/lib RPATH, so it must launch via the script.
# Because the path goes through `current`, it keeps resolving to the active
# version after every self-update with no .desktop rewrite.
mkdir -p "$APPLICATIONS_DIR"
cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Type=Application
Version=1.1
Name=ViewCam
GenericName=Virtual Webcam
Comment=Turn your phone into a virtual webcam
Exec=$CURRENT_LINK/viewcam.sh %U
TryExec=$CURRENT_LINK/viewcam.sh
Icon=viewcam
Terminal=false
Categories=AudioVideo;Video;
Keywords=webcam;camera;virtual;phone;stream;
StartupNotify=true
StartupWMClass=ViewCam
X-AppVersion=$VERSION
EOF
chmod 644 "$DESKTOP_FILE"
ok "Installed launcher → $DESKTOP_FILE"

# ── 8. Refresh desktop/icon caches (best-effort, non-fatal) ────────────────
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APPLICATIONS_DIR" >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q -t -f "$ICONS_DIR" >/dev/null 2>&1 || true
fi

# ── Done ───────────────────────────────────────────────────────────────────
echo
ok "ViewCam $VERSION installed."
echo
echo "Launch it from your application menu (search \"ViewCam\"), or run:"
echo "    $CURRENT_LINK/viewcam.sh"
echo
echo "Future updates apply in place under $INSTALL_ROOT (no reinstall needed)."
