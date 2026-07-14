param(
    [ValidateSet("Release", "RelWithDebInfo")]
    [string]$Configuration = "Release",
    [switch]$SkipBuild,
    [switch]$BuildInstaller
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"
$OutputDir = Join-Path $ProjectRoot "dist"
$StageDir = Join-Path $OutputDir "RailShotTV"
$ExePath = Join-Path $BuildDir "$Configuration\RailShotBroadcaster.exe"
$VirtualCamPath = Join-Path $BuildDir "$Configuration\railshot-virtualcam64.dll"

function Copy-Dlls([string]$SourceDirectory, [string]$Destination) {
    if (-not (Test-Path $SourceDirectory)) {
        return
    }
    Get-ChildItem -Path $SourceDirectory -Filter "*.dll" -File -Recurse |
        ForEach-Object {
            Copy-Item -Path $_.FullName -Destination $Destination -Force
        }
}

Write-Host "=== RailShot TV Release Packaging ===" -ForegroundColor Cyan

if (-not $SkipBuild) {
    $cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
    if (-not $cmake) {
        $cmake = "C:\Program Files\CMake\bin\cmake.exe"
    }
    if (-not (Test-Path $cmake)) {
        throw "CMake was not found."
    }
    & $cmake -S $ProjectRoot -B $BuildDir -G "Visual Studio 17 2022" -A x64
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed."
    }
    & $cmake --build $BuildDir --config $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "Release build failed."
    }
}

if (-not (Test-Path $ExePath)) {
    throw "Broadcaster executable not found: $ExePath"
}
if (-not (Test-Path $VirtualCamPath)) {
    throw "Virtual camera DLL not found: $VirtualCamPath"
}

if (Test-Path $StageDir) {
    Remove-Item -Path $StageDir -Recurse -Force
}
New-Item -ItemType Directory -Path $StageDir -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $StageDir "scripts") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $StageDir "assets\overlays") -Force | Out-Null

Copy-Item $ExePath $StageDir -Force
Copy-Item $VirtualCamPath $StageDir -Force
Copy-Item (Join-Path $ProjectRoot "assets\overlays\scoreboard.html") `
    (Join-Path $StageDir "assets\overlays\scoreboard.html") -Force
Copy-Item (Join-Path $ProjectRoot "scripts\install-virtualcam.bat") `
    (Join-Path $StageDir "scripts\install-virtualcam.bat") -Force
Copy-Item (Join-Path $ProjectRoot "scripts\uninstall-virtualcam.bat") `
    (Join-Path $StageDir "scripts\uninstall-virtualcam.bat") -Force
Copy-Item (Join-Path $ProjectRoot "README.md") $StageDir -Force
Copy-Item (Join-Path $ProjectRoot "THIRD_PARTY_NOTICES.md") $StageDir -Force

$qtRoot = Join-Path $ProjectRoot "vendor\qt\6.8.2\msvc2022_64"
$windeployqt = Join-Path $qtRoot "bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
    $windeployqtCommand = Get-Command windeployqt -ErrorAction SilentlyContinue
    if ($windeployqtCommand) {
        $windeployqt = $windeployqtCommand.Source
    }
}
if (-not (Test-Path $windeployqt)) {
    throw "windeployqt was not found. Run scripts\setup-vendor.ps1 first."
}

& $windeployqt --release --compiler-runtime --no-translations `
    --dir $StageDir (Join-Path $StageDir "RailShotBroadcaster.exe")
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed."
}

# windeployqt cannot locate the compiler runtime outside a VS developer shell.
# Resolve the matching x64 VC runtime explicitly through vswhere.
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsInstall = (& $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath | Select-Object -First 1)
    if (-not $vsInstall) {
        $vsInstall = (& $vswhere -latest -products * -property installationPath |
            Select-Object -First 1)
    }
    if ($vsInstall) {
        $redistVersion = Get-ChildItem -Path (Join-Path $vsInstall "VC\Redist\MSVC") `
            -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^\d' } |
            Sort-Object Name -Descending |
            Select-Object -First 1
        if ($redistVersion) {
            $crtDirectory = Get-ChildItem -Path (Join-Path $redistVersion.FullName "x64") `
                -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue |
                Select-Object -First 1
            if ($crtDirectory) {
                Copy-Dlls $crtDirectory.FullName $StageDir
            }
        }
    }
}

Copy-Dlls (Join-Path $ProjectRoot "vendor\ffmpeg\bin") $StageDir
Copy-Dlls (Join-Path $ProjectRoot "vendor\librtmp\bin") $StageDir
Copy-Dlls (Join-Path $ProjectRoot "vendor\ndi\bin\x64") $StageDir

$requiredRuntimePatterns = @("avcodec-*.dll", "avformat-*.dll", "avutil-*.dll",
                             "swscale-*.dll", "swresample-*.dll")
foreach ($pattern in $requiredRuntimePatterns) {
    if (-not (Get-ChildItem -Path $StageDir -Filter $pattern -File -ErrorAction SilentlyContinue)) {
        throw "Missing packaged FFmpeg runtime: $pattern"
    }
}
foreach ($runtime in @("vcruntime140.dll", "msvcp140.dll")) {
    if (-not (Test-Path (Join-Path $StageDir $runtime))) {
        throw "Missing packaged Microsoft C++ runtime: $runtime"
    }
}

$manifest = Get-ChildItem -Path $StageDir -File -Recurse |
    Sort-Object FullName |
    ForEach-Object {
        $relative = $_.FullName.Substring($StageDir.Length + 1)
        $hash = (Get-FileHash -Path $_.FullName -Algorithm SHA256).Hash
        "$hash  $relative"
    }
Set-Content -Path (Join-Path $StageDir "SHA256SUMS.txt") -Value $manifest -Encoding UTF8

$zipPath = Join-Path $OutputDir "RailShotTV-Windows-x64.zip"
if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}
Compress-Archive -Path (Join-Path $StageDir "*") -DestinationPath $zipPath -CompressionLevel Optimal

if ($BuildInstaller) {
    $iscc = (Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source
    if (-not $iscc) {
        $iscc = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    }
    if (-not (Test-Path $iscc)) {
        throw "Inno Setup 6 was not found. Install it or omit -BuildInstaller."
    }
    & $iscc (Join-Path $ProjectRoot "installer\RailShotTV.iss")
    if ($LASTEXITCODE -ne 0) {
        throw "Installer build failed."
    }
}

Write-Host "Packaged folder: $StageDir" -ForegroundColor Green
Write-Host "Portable ZIP:    $zipPath" -ForegroundColor Green
