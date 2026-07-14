# RailShot TV Windows Broadcaster

RailShot TV is a native Windows live-production application for general broadcasts and sports/billiards venues. It uses an original RailShot interface and implementation; OBS Studio is used only as a behavior checklist.

## Highlights

- Scene collections, layered sources, Studio Mode, Preview/Program, and hotkeys
- Video, display, WGC window/game, image, media, browser, text, color, scoreboard, and NDI sources
- Desktop, microphone/device, media, and per-application audio capture
- Audio mixer, monitoring, sync delay, gain, compressor, gate, and noise suppression
- Crop, keying, mask, color-grade, scale, scroll, sharpness, and render-delay filters
- Cut, fade, fade-to-black, slide, swipe, wipe, luma-wipe, and stinger-style transitions
- NVENC, Quick Sync, AMF, and x264 H.264 encoding with automatic fallback
- RTMP streaming presets, MP4/MKV/MOV recording, replay buffer, remux, ISO recording, and virtual camera
- Phone-friendly HTTP remote on port 8080
- obs-websocket v5-compatible remote on port 4455, with optional challenge authentication
- RailShot scoreboard and simplified Venue Mode

## Install

Run `RailShotTV-Windows-x64-Setup.exe` and leave **Register the RailShot Virtual Camera** selected if you want RailShot to appear as a camera in other applications. Installer elevation is required only for installation and virtual-camera registration.

The portable ZIP can also be extracted anywhere. To register its virtual camera, right-click `scripts\install-virtualcam.bat` and choose **Run as administrator**. Use `scripts\uninstall-virtualcam.bat` before moving or deleting a registered portable installation.

Windows 10 version 2004 or later is recommended for Windows Graphics Capture and per-application audio. The Microsoft Edge WebView2 Runtime is required for browser sources and is included with current Windows 10/11 installations.

## First broadcast

1. Add or select a scene.
2. Add a camera, display, window, game, media, or browser source.
3. Open **Settings → Stream**, choose a service, and enter the stream key.
4. Configure the encoder and recording/replay options under **Settings → Output**.
5. Click **Start Streaming**, **Start Recording**, or both.

Recordings and saved replays default to `%USERPROFILE%\Videos\RailShot\Recordings`.

## Remote control

- Web remote: `http://<computer-ip>:8080`
- WebSocket: `ws://<computer-ip>:4455`

The WebSocket endpoint supports the obs-websocket v5 Hello/Identify flow, common scene/studio/stream/record requests, request batches, and output/scene events. Configure a password under **Settings → Stream** before exposing the port beyond a trusted LAN. Existing RailShot string operations remain supported after authentication.

## Troubleshooting and limitations

- Logs: `%LOCALAPPDATA%\RailShot TV\RailShot TV Broadcaster\logs\railshot.log`
- Browser profile/cache: `%LOCALAPPDATA%\RailShot TV\RailShot TV Broadcaster\webview2-data`
- If Windows Firewall prompts for network access, allow private networks only unless the broadcaster must be controlled from another routed network.
- Browser sources require the Microsoft Edge WebView2 Runtime.
- Game Capture is an inject-free Windows Graphics Capture/BitBlt implementation. Protected, exclusive-fullscreen, and anti-cheat-protected games may not be capturable.
- NDI support is included only when the NDI SDK is present at build/package time.
- For virtual-camera registration errors, rerun the installer or registration script as administrator.

## Developer build

Requirements:

- Windows 10/11 x64
- Visual Studio 2022 with Desktop development with C++
- CMake
- Qt 6.8.2, FFmpeg shared libraries, and librtmp under `vendor/`

Install local dependencies:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\setup-vendor.ps1
```

Build:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Package the portable folder and ZIP:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package-release.ps1
```

To also compile the Inno Setup installer:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package-release.ps1 -BuildInstaller
```

The packaging script runs `windeployqt`, includes the MSVC runtime and media DLLs, validates required FFmpeg runtimes, includes the virtual camera, and writes SHA-256 checksums. Inno Setup 6 is required only for `-BuildInstaller`.

## Project structure

```text
src/capture/     Video, window/game, browser/media, and audio sources
src/core/        Scene model, compositor, mixer, settings, remote command bus
src/encoder/     FFmpeg encoder
src/network/     HTTP and WebSocket remotes
src/output/      RTMP, recording, replay, remux, and virtual camera
src/ui/          RailShot Qt interface
plugins/         DirectShow virtual camera filter
installer/       Inno Setup definition
scripts/         Dependency, packaging, and registration scripts
```
