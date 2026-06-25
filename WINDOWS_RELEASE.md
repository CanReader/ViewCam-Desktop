# Windows Release & Self-Update — Handoff for Claude (or you) on the Windows PC

This is the checklist for building, signing, and publishing the **Windows** ViewCam
desktop release and wiring it into the self-update system. The **Linux** side is
DONE and live (1.0.1 on viewcam.tech). Full design is in
`../design/UPDATE_SYSTEM_BRIEF.md` (§4a is the platform-split apply); this is the
Windows-specific to-do.

> TL;DR: build `bin\` (MSVC) → `iscc` the installer → **sign the `Setup.exe` with the
> Ed25519 key (which lives on the Linux machine)** → upload the `.exe` to
> `dl.viewcam.tech/1.0.1/` + a manifest with BOTH platforms filled to
> `updates.viewcam.tech/stable/`.

> 🛑 **DO NOT PUBLISH / DEPLOY FROM WINDOWS.** `hosting_deployStaticWebsite`
> **REPLACES the ENTIRE subdomain tree** — it does NOT merge. The live **Linux**
> `.zip` artifacts and the signed `manifest.json` exist ONLY on the Linux machine.
> If you deploy to `dl.viewcam.tech` or `updates.viewcam.tech` from Windows, you
> will **WIPE the live Linux release** (this already happened once). Your job ends
> at producing a tested `Setup.exe`; hand it to the Linux machine, where ALL
> publishing happens with the full tree (Linux zips + Windows exe + merged manifest).
> (The tool does NOT filter binaries — that was a misdiagnosis; the wipe is the
> tree-replace behavior. FYI the manifest is unaffected by a `dl` deploy.)

## Why Windows differs from Linux
ViewCam ships a **system virtual camera** (DirectShow `ViewCamFilter.dll` + Media
Foundation `ViewCamMFSource.dll`, `regsvr32`-registered, admin). A system camera
needs admin + a stable registered path, so Windows does NOT use Linux's per-user
versioned-swap. Instead:
- **Windows self-update = download the new signed `Setup.exe` → verify Ed25519 → run
  it elevated.** Inno Setup upgrades the Program Files install in place (same
  `AppId`), re-registers the cam DLLs, and relaunches.
  (`UpdateChecker::applyWindowsInstaller`, `Q_OS_WIN`.)
- The app creates mutex `Global\ViewCamStudioRunning`; the installer's `AppMutex`
  matches it so `CloseApplications`/`RestartApplications` can close + relaunch the app.

## Status
- ✅ **Linux 1.0.1** live: `dl.viewcam.tech/1.0.1/ViewCam-1.0.1-linux-x86_64.zip` +
  signed manifest. End-to-end self-update verified on a real machine.
- ✅ **Windows 1.0.1 PUBLISHED (2026-06-25):**
  `dl.viewcam.tech/1.0.1/ViewCam-1.0.1-win-x64-Setup.exe` (30,382,038 B, sha
  `14bac0cb…`) is live and **signed** into the manifest's `windows-x86_64` entry;
  both platforms in `updates.viewcam.tech/stable/manifest.json` verified
  (sha + Ed25519). Built on a Windows host, installer via `iscc`, exe pulled to the
  Linux machine and signed there (key never left Linux), manifest deployed last.
- ⏳ **Remaining**: confirm the actual Windows SELF-UPDATE flow on a Windows machine
  (install 1.0.0 → it downloads the Setup.exe → verifies → UAC-elevates → in-place
  upgrade + cam re-register → relaunch as 1.0.1). The artifact is live; this is the
  runtime path (`Q_OS_WIN` / `applyWindowsInstaller`) that still needs a real run.

## Prereqs (Windows)
MSVC build tools, Qt 6 (matching the project), CMake + Ninja, **Inno Setup 6**
(`iscc` on PATH), Git, and Git Bash/MSYS2 (only if signing on Windows).

## Steps

### 1. Pull + build
```
git pull
powershell -ExecutionPolicy Bypass -File scripts\build_msvc_release.ps1
```
Produces `bin\` = `ViewCam.exe` + DLLs + `viewcam.rcc` + `vc-updater.exe` + Qt runtime
(via `windeployqt`) + the cam DLLs.

### 2. VERIFY IT RUNS FROM THE BUNDLE — do not skip
The Linux build once shipped broken because the deploy was missing Qt GL plugins.
`windeployqt` is more complete than the Linux manual deploy, but still verify on a
clean machine (or VM) with **no dev Qt in PATH**:
- `bin\ViewCam.exe` opens a window (Qt Quick renders — no "Failed to create RHI").
- `bin\` contains `platforms\qwindows.dll`, `styles\`, `imageformats\qsvg.dll`,
  `tls\`, the cam DLLs, and `vc-updater.exe`.

### 3. Build the installer
```
iscc installer\viewcam_setup.iss     ->  installer\output\ViewCamStudio-1.0.1-Setup.exe
```
On a clean Windows VM: install → app runs → **the virtual camera appears in
OBS/Zoom/Teams** → uninstall is clean. Then test an **in-place upgrade**: install an
older build, run the new `Setup.exe`, confirm it closes the running app, upgrades,
re-registers the cam, and relaunches.

### 4. Sign the Setup.exe — KEY CONSTRAINT (read this)
The **Ed25519 private key** `release/update-signing-key.pem` is ONLY on the Linux
machine. It is gitignored and NOT in the repo. Do NOT commit it. Choose one:
- **(Recommended) Sign on Linux:** copy `ViewCamStudio-1.0.1-Setup.exe` to the Linux
  machine and do steps 4–6 there (it has the key + can pull the live manifest).
- **Sign on Windows:** securely copy `update-signing-key.pem` over, then in Git Bash:
  ```
  cp installer/output/ViewCamStudio-1.0.1-Setup.exe dist/
  VIEWCAM_MIN_VER=1.0.0 VIEWCAM_NOTES_URL="https://viewcam.tech/releases/1.0.1" \
    VIEWCAM_SIGN_KEY=/path/to/update-signing-key.pem ./scripts/release.sh 1.0.1 stable
  ```
  Signs the `.exe` (Ed25519 over its bytes) and fills `windows-x86_64` in
  `dist/manifest.json`.

### 5. Merge into the LIVE manifest — keep the Linux entry!
`updates.viewcam.tech/stable/manifest.json` already has `linux-x86_64` filled. Seed
the local manifest from the live one BEFORE running release.sh so the merge preserves it:
```
curl -s https://updates.viewcam.tech/stable/manifest.json -o dist/manifest.json
# then run release.sh (step 4) -> it fills windows-x86_64 and keeps linux-x86_64
```
Confirm the result has BOTH platforms filled (non-empty `url/sha256/size/signature`).

### 6. Publish (Hostinger shared hosting, user `u865280119`)
- Upload `ViewCamStudio-1.0.1-Setup.exe` → `dl.viewcam.tech/1.0.1/`
  (the manifest `windows-x86_64.url` must match this exact path/filename).
- Upload the merged `manifest.json` → `updates.viewcam.tech/stable/`.
- **Order: artifact to `dl` FIRST, manifest to `updates` LAST** (atomic go-live).
- Mechanism: Hostinger MCP `hosting_deployStaticWebsite(domain, archivePath)`
  **REPLACES** the subdomain tree — so build a FULL tree (`.htaccess` + all version
  dirs + manifests) and deploy once per subdomain. Or FTP individual files
  (host `89.116.147.26`, user `u865280119`). See `scripts/HOSTING.md`.

### 7. Test Windows self-update end-to-end
Install a Windows **1.0.0**, run it, confirm: it sees 1.0.1 → downloads the
`Setup.exe` → **verifies the Ed25519 signature** → runs it (UAC) → upgrades in place →
cam still works → relaunches as 1.0.1.

## Gotchas
- **The whole `Q_OS_WIN` apply path is UNTESTED** (ShellExecuteW flags, AppMutex
  close, in-place upgrade, cam re-register) — this is the first real Windows run.
  Budget debug time. Flags used: `/SILENT /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS /NORESTART`.
- `windows-x86_64.url` must EXACTLY match the uploaded filename under `dl/1.0.1/`.
- Never overwrite the live Linux manifest entry (step 5).
- Keep the private key on as few machines as possible — prefer signing on Linux.
- `installer/viewcam_setup.iss` `#define AppVersion` is `1.0.1` — bump it for future releases.

## Live reference (current Linux 1.0.1)
- Manifest:  https://updates.viewcam.tech/stable/manifest.json
- Linux zip: https://dl.viewcam.tech/1.0.1/ViewCam-1.0.1-linux-x86_64.zip
- Public key (verify-only): `release/update-public-key.pem` (hex `6f121a3a…cf32bb00`)
