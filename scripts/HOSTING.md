# ViewCam update hosting layout (viewcam.tech)

Two hostnames, two cache policies. Manifests are mutable + short-cached; artifacts
are immutable + content-addressed by version path and never overwritten.

## Paths

```
updates.viewcam.tech/
  stable/manifest.json        # channel manifest (this is what the client GETs)
  beta/manifest.json

dl.viewcam.tech/
  <ver>/ViewCam-<ver>-linux-x86_64.zip      # Linux: versioned-swap package
  <ver>/ViewCam-<ver>-win-x64-Setup.exe     # Windows: signed Inno Setup installer
```

The **apply unit differs by platform** (verify is identical):

- **Linux** ships a `.zip` full-folder package. The client verifies it, extracts
  it into `versions/<ver>`, and `vc-updater` repoints `current` (versioned-swap).
- **Windows** ships a signed `Setup.exe` (Inno Setup). The client verifies its
  Ed25519 signature the SAME way, then launches it; the installer self-elevates
  (UAC), upgrades the Program Files install in place, and re-registers the system
  virtual camera DLLs. No per-user versioned-swap on Windows.

- Client embeds `VIEWCAM_UPDATE_MANIFEST_BASE=https://updates.viewcam.tech` and
  `VIEWCAM_UPDATE_CHANNEL` (default `stable`), and GETs
  `https://updates.viewcam.tech/<channel>/manifest.json`.
- Each manifest `platforms.<key>.url` is an absolute `https://dl.viewcam.tech/<ver>/…`
  link (release.sh writes these from `VIEWCAM_BASE_URL`, default
  `https://dl.viewcam.tech/<ver>/`).
- Manifest key → filename:
  `linux-x86_64` → `ViewCam-<ver>-linux-x86_64.zip`,
  `windows-x86_64` → `ViewCam-<ver>-win-x64-Setup.exe`.

## Cache headers

| Object | Content-Type | Cache-Control |
| --- | --- | --- |
| `<channel>/manifest.json` | `application/json` | `public, max-age=300, must-revalidate` (5 min) |
| `<ver>/*.zip` | `application/zip` | `public, max-age=31536000, immutable` (1 yr) |
| `<ver>/*-Setup.exe` | `application/octet-stream` | `public, max-age=31536000, immutable` (1 yr) |

Artifacts are immutable: a published `<ver>/<zip>` is **never** overwritten. Ship a
fix as a new version. The manifest is the only thing that changes to point clients
at it, so it must be short-cached (and ideally served behind a CDN that honors the
5-minute TTL). Optionally set `ETag`/`Last-Modified` on the manifest so
`must-revalidate` is cheap.

## Publish steps (per release)

1. Build + deploy `bin/`, then produce the per-platform artifact:
   - **Linux** (versioned-swap package):
     ```
     cmake --build build-release --target ViewCam vc-updater
     cmake --build build-release --target viewcam_package    # -> dist/ViewCam-<ver>-linux-x86_64.zip
     ```
   - **Windows** (signed installer, on a Windows host with Inno Setup 6):
     ```
     cmake --build build-release --target ViewCam            # deploy bin\
     iscc installer\viewcam_setup.iss                        # -> installer\output\ViewCamStudio-<ver>-Setup.exe
     copy installer\output\ViewCamStudio-<ver>-Setup.exe dist\ViewCam-<ver>-win-x64-Setup.exe
     ```
     The Setup.exe is what the in-app updater downloads, verifies, and launches.
     (Code-sign the Setup.exe with your Authenticode cert before signing it for
     the manifest, so SmartScreen and the UAC dialog show a trusted publisher.)
2. Sign + emit manifest (run once per platform host; merges into one manifest.json):
   ```
   scripts/release.sh <ver> stable
   ```
   On Linux it finds + signs the `.zip`; on Windows (Git Bash/MSYS2) it finds +
   signs the `…-win-x64-Setup.exe`. The Ed25519 signing is identical for both.
3. Upload the **artifacts first** to `dl.viewcam.tech/<ver>/` (immutable headers).
   Verify each is fetchable before touching the manifest.
4. Upload `dist/manifest.json` **last** to `updates.viewcam.tech/<channel>/manifest.json`
   (short cache). Flipping the manifest is the atomic "go live" — once it points at
   the new version, clients begin downloading on their next background check.
5. For staged rollout, publish the manifest with `rollout < 1.0` (e.g. `0.1`),
   watch, then re-upload with `rollout: 1.0`. Only the manifest changes; artifacts
   stay put.

Ordering matters: artifacts before manifest. A manifest pointing at a not-yet-uploaded
zip would make clients fail the download/verify step until the upload lands.
```
