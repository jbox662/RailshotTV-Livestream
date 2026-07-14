# RailShot TV — Vendor Dependency Setup
# Run from project root:  powershell -ExecutionPolicy Bypass -File scripts\setup-vendor.ps1

param(
    [string]$QtVersion = "6.8.2",
    [string]$QtArch = "win64_msvc2022_64"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$VendorDir = Join-Path $ProjectRoot "vendor"

Write-Host "=== RailShot TV Vendor Setup ===" -ForegroundColor Cyan
Write-Host "Project: $ProjectRoot"
Write-Host "Vendor:  $VendorDir"
Write-Host ""

function Ensure-Dir($path) {
    if (-not (Test-Path $path)) {
        New-Item -ItemType Directory -Path $path -Force | Out-Null
    }
}

# ---------------------------------------------------------------------------
# 1. FFmpeg (prebuilt shared + dev files from BtbN)
# ---------------------------------------------------------------------------
$FfmpegDir = Join-Path $VendorDir "ffmpeg"
if (Test-Path (Join-Path $FfmpegDir "include\libavcodec\avcodec.h")) {
    Write-Host "[FFmpeg] Already installed at vendor/ffmpeg" -ForegroundColor Green
} else {
    Write-Host "[FFmpeg] Downloading prebuilt Windows x64 shared build..." -ForegroundColor Yellow
    $zipUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip"
    $zipPath = Join-Path $env:TEMP "ffmpeg-win64-gpl-shared.zip"

    Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath -UseBasicParsing
    $extractDir = Join-Path $env:TEMP "ffmpeg-extract"
    if (Test-Path $extractDir) { Remove-Item $extractDir -Recurse -Force }
    Expand-Archive -Path $zipPath -DestinationPath $extractDir

    $inner = Get-ChildItem $extractDir -Directory | Select-Object -First 1
    if (Test-Path $FfmpegDir) { Remove-Item $FfmpegDir -Recurse -Force }
    Move-Item $inner.FullName $FfmpegDir
    Remove-Item $zipPath -Force -ErrorAction SilentlyContinue
    Remove-Item $extractDir -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "[FFmpeg] Installed to vendor/ffmpeg" -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# 2. librtmp (build from rtmpdump source)
# ---------------------------------------------------------------------------
$LibrtmpDir = Join-Path $VendorDir "librtmp"
$LibrtmpSrc = Join-Path $VendorDir "rtmpdump-src"
$LibrtmpHeader = Join-Path $LibrtmpDir "include\rtmp.h"
$LibrtmpLib = Join-Path $LibrtmpDir "lib\rtmp.lib"

if ((Test-Path $LibrtmpHeader) -and (Test-Path $LibrtmpLib)) {
    Write-Host "[librtmp] Already installed at vendor/librtmp" -ForegroundColor Green
} else {
    Write-Host "[librtmp] Fetching rtmpdump source..." -ForegroundColor Yellow
    if (-not (Test-Path $LibrtmpSrc)) {
        git clone --depth 1 https://github.com/ShiftMediaProject/rtmpdump.git $LibrtmpSrc
    }

    Ensure-Dir (Join-Path $LibrtmpDir "include")
    Ensure-Dir (Join-Path $LibrtmpDir "lib")

  Copy-Item (Join-Path $LibrtmpSrc "librtmp\*.h") (Join-Path $LibrtmpDir "include") -Force

    # Find MSVC developer environment
    $VsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $VsWhere)) {
        throw "vswhere not found. Install Visual Studio 2022 with C++ workload first."
    }
    $VsPath = & $VsWhere -latest -property installationPath
    $Vcvars = Join-Path $VsPath "VC\Auxiliary\Build\vcvars64.bat"

    $BuildDir = Join-Path $LibrtmpSrc "build"
    Ensure-Dir $BuildDir

    $CompatHeader = Join-Path $VendorDir "librtmp-compat.h"
    $buildScript = @"
call "$Vcvars" >nul
cd /d "$BuildDir"
cl /nologo /c /O2 /DNO_CRYPTO /DWIN32 /D_WIN32 /FI"$CompatHeader" /I"$LibrtmpSrc\librtmp" ^
   "$LibrtmpSrc\librtmp\amf.c" ^
   "$LibrtmpSrc\librtmp\hashswf.c" ^
   "$LibrtmpSrc\librtmp\log.c" ^
   "$LibrtmpSrc\librtmp\parseurl.c" ^
   "$LibrtmpSrc\librtmp\rtmp.c"
lib /nologo /OUT:"$LibrtmpLib" amf.obj hashswf.obj log.obj parseurl.obj rtmp.obj ws2_32.lib winmm.lib
"@

    $batPath = Join-Path $BuildDir "build-librtmp.bat"
    Set-Content -Path $batPath -Value $buildScript -Encoding ASCII
    cmd /c $batPath

    if (-not (Test-Path $LibrtmpLib)) {
        throw "librtmp build failed. Check that VS 2022 C++ tools are installed."
    }
    Write-Host "[librtmp] Built and installed to vendor/librtmp" -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# 3. Qt 6 (via aqtinstall — requires internet, ~2 GB)
# ---------------------------------------------------------------------------
$QtDir = Join-Path $VendorDir "qt"
$QtInstallDir = Join-Path $QtDir "$QtVersion\msvc2022_64"
$QtConfig = Join-Path $QtInstallDir "bin\qmake.exe"

if (Test-Path $QtConfig) {
    Write-Host "[Qt] Already installed at $QtInstallDir" -ForegroundColor Green
} else {
    Write-Host "[Qt] Installing Qt $QtVersion ($QtArch) via aqtinstall..." -ForegroundColor Yellow
    Write-Host "       This downloads ~2 GB and may take several minutes." -ForegroundColor DarkYellow

    python -m pip install aqtinstall --quiet
    python -m aqt install-qt windows desktop $QtVersion $QtArch -O $QtDir -m qtshadertools

    if (-not (Test-Path $QtConfig)) {
        throw "Qt install failed. Check internet connection and try again."
    }
    Write-Host "[Qt] Installed to vendor/qt" -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Vendor setup complete ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:"
Write-Host "  1. Install CMake 4.4 (add to PATH)"
Write-Host "  2. Configure:"
Write-Host "     cmake -B build -G `"Visual Studio 17 2022`" -A x64"
Write-Host "  3. Build:"
Write-Host "     cmake --build build --config Release"
Write-Host "  4. Deploy Qt DLLs:"
Write-Host "     vendor\qt\$QtVersion\msvc2022_64\bin\windeployqt.exe build\Release\RailShotBroadcaster.exe"
Write-Host "  5. Copy FFmpeg DLLs:"
Write-Host "     copy vendor\ffmpeg\bin\*.dll build\Release\"
