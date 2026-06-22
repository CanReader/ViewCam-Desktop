#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────────────────
# uninstall.sh — remove a per-user ViewCam install (Linux).
#
# By default removes only the desktop integration (.desktop + icons) and leaves
# the app tree + your settings alone. Pass --purge (or --all) to also delete the
# whole ~/.local/share/ViewCam install root (versions, current, vc-updater,
# update.log). --yes skips the confirmation prompt.
# ──────────────────────────────────────────────────────────────────────────
set -euo pipefail

XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
INSTALL_ROOT="$XDG_DATA_HOME/ViewCam"
APPLICATIONS_DIR="$XDG_DATA_HOME/applications"
ICONS_DIR="$XDG_DATA_HOME/icons/hicolor"
DESKTOP_FILE="$APPLICATIONS_DIR/viewcam.desktop"

PURGE=0
ASSUME_YES=0

info() { printf '\033[36m::\033[0m %s\n' "$*"; }
ok()   { printf '\033[32m✓\033[0m %s\n'  "$*"; }

while [ $# -gt 0 ]; do
    case "$1" in
        --purge|--all) PURGE=1; shift ;;
        -y|--yes)      ASSUME_YES=1; shift ;;
        -h|--help)
            cat <<EOF
Usage: uninstall.sh [--purge] [--yes]
  (default)  remove the .desktop launcher and icons only.
  --purge    also delete the install tree at $INSTALL_ROOT.
  --yes      do not prompt for confirmation.
EOF
            exit 0 ;;
        *) printf 'unknown argument: %s\n' "$1" >&2; exit 1 ;;
    esac
done

# ── Desktop integration (always) ───────────────────────────────────────────
[ -f "$DESKTOP_FILE" ] && { rm -f "$DESKTOP_FILE"; ok "Removed $DESKTOP_FILE"; } \
                       || info "No launcher at $DESKTOP_FILE"

for icon in "$ICONS_DIR/scalable/apps/viewcam.svg" "$ICONS_DIR/512x512/apps/viewcam.png"; do
    [ -f "$icon" ] && { rm -f "$icon"; ok "Removed $icon"; }
done

# ── Install tree (only with --purge) ───────────────────────────────────────
if [ "$PURGE" = 1 ]; then
    if [ -d "$INSTALL_ROOT" ]; then
        if [ "$ASSUME_YES" != 1 ]; then
            printf '\033[33mDelete the entire install tree %s? [y/N] \033[0m' "$INSTALL_ROOT"
            read -r reply
            case "$reply" in [yY]|[yY][eE][sS]) ;; *) info "Kept install tree."; PURGE=0 ;; esac
        fi
    else
        info "No install tree at $INSTALL_ROOT"
        PURGE=0
    fi
fi
if [ "$PURGE" = 1 ]; then
    rm -rf "$INSTALL_ROOT"
    ok "Removed $INSTALL_ROOT"
fi

# ── Refresh caches (best-effort) ───────────────────────────────────────────
command -v update-desktop-database >/dev/null 2>&1 && \
    update-desktop-database "$APPLICATIONS_DIR" >/dev/null 2>&1 || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && \
    gtk-update-icon-cache -q -t -f "$ICONS_DIR" >/dev/null 2>&1 || true

echo
ok "ViewCam uninstalled."
[ "$PURGE" != 1 ] && echo "(App tree + settings kept under $INSTALL_ROOT. Re-run with --purge to remove them.)"
