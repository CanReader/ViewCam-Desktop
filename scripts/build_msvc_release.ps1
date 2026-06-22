# ViewCam Studio — MSVC Release Build + Installer
# Usage (from project root):  .\scripts\build_msvc_release.ps1 [-SkipBuild] [-SkipInstaller]
param(
    [switch]$SkipBuild,
    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$DistDir     = Join-Path $ProjectRoot "bin"
$BuildDir    = Join-Path $ProjectRoot "build\release-msvc"

# ── Tool paths ──────────────────────────────────────────────────────────────
$CMake   = "D:\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$Ninja   = "D:\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
$IsCC    = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"

if (-not (Test-Path $CMake))  { throw "cmake not found at $CMake"  }
if (-not (Test-Path $IsCC))   { throw "ISCC.exe not found at $IsCC — install Inno Setup 6" }

Write-Host "`n=== ViewCam Studio — MSVC Release Build ===" -ForegroundColor Cyan
Write-Host "Project : $ProjectRoot"
Write-Host "Deploy  : $DistDir"

# ── 1. Configure ─────────────────────────────────────────────────────────────
if (-not $SkipBuild) {
    Write-Host "`n[1/3] Configuring (preset: release-msvc)..." -ForegroundColor Yellow
    Push-Location $ProjectRoot
    & $CMake -S . --preset release-msvc
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed (exit $LASTEXITCODE)" }
    Pop-Location

    # ── 2. Build ──────────────────────────────────────────────────────────────
    Write-Host "`n[2/3] Building Release..." -ForegroundColor Yellow
    # Note: --preset is resolved relative to CWD; use --build + --config instead.
    & $CMake --build $BuildDir --config Release --parallel
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed (exit $LASTEXITCODE)" }
    Write-Host "Build complete. Deploy dir: $DistDir" -ForegroundColor Green
}

# ── 3. Package with Inno Setup ───────────────────────────────────────────────
if (-not $SkipInstaller) {
    Write-Host "`n[3/3] Creating installer..." -ForegroundColor Yellow

    $InstallerScript = Join-Path $ProjectRoot "installer\viewcam_setup.iss"
    $OutputDir       = Join-Path $ProjectRoot "installer\output"
    New-Item -ItemType Directory -Force $OutputDir | Out-Null

    & $IsCC $InstallerScript
    if ($LASTEXITCODE -ne 0) { throw "Inno Setup compilation failed (exit $LASTEXITCODE)" }

    $Installer = Get-ChildItem $OutputDir -Filter "ViewCamStudio-*-Setup.exe" | Select-Object -Last 1
    Write-Host "`nInstaller ready: $($Installer.FullName)" -ForegroundColor Green
}

Write-Host "`nAll done." -ForegroundColor Cyan
