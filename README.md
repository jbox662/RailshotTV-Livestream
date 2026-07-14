# RailShot TV Windows Broadcaster

Professional live streaming application for billiard venues.

## Phase 4 Status

Generic production features with optional billiard presets:
- **Production profile** — General vs Sports / Billiards
- **Scoreboard overlay** source + dock panel (+1/−1, names, race-to, game type)
- **Browser source** — URL or local HTML overlay (simple HTML renderer)
- **Web remote** — phone-friendly control page at `http://localhost:8080`
- **Venue Mode** — simplified full-screen scene launcher with optional billiard presets
- **RailShot API client** — stub ready for OAuth + cloud score sync

## Phase 3 Status

Professional production features:
- **Studio Mode** — dual Preview/Program monitors with Transition button
- **Local Recording** — program mix recorded to MP4 (`~/Videos/RailShot/Recordings/`)
- **ISO Recording** — per-camera isolated MP4 files before compositing
- **NDI Integration** — source discovery and receiver stub (full support when SDK is in `vendor/ndi/`)

## Phase 2 Status

Scene management and multi-source production:
- `Source`, `Scene`, `SceneCollection` models with `SceneManager` singleton
- Scenes list + Sources list UI (bottom panels)
- Source types: Video Device, Image (PNG/JPG), Media File (MP4/MP3 with loop)
- Scene compositor with z-order layering and per-source transforms
- Preview drag-to-move transform controls
- 300ms crossfade transitions between scenes
- Audio mixer with per-source volume sliders and mute toggles (logarithmic dB)
- Auto-save scene collections every 60 seconds

## Phase 1 Status

Core engine with:
- DirectShow webcam capture
- WASAPI microphone capture
- OpenGL compositor (1920x1080, dedicated thread)
- FFmpeg H.264 encoder (NVENC / AMF / QSV / libx264 fallback)
- RTMP output via librtmp + FLV muxing
- Multi-threaded pipeline (capture → compositor → encoder → output)

## Prerequisites

- Windows 10/11 (64-bit)
- Visual Studio 2022 with C++ desktop development
- Qt 6.4+ (Widgets, OpenGL, OpenGLWidgets)
- FFmpeg shared libraries (with NVENC/AMF/QSV enabled)
- librtmp

Place dependencies in `/vendor` — see [vendor/README.md](vendor/README.md).

## Build

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:\path\to\qt"
cmake --build build --config Release
```

Deploy Qt DLLs:

```powershell
windeployqt build\Release\RailShotBroadcaster.exe
```

Copy FFmpeg DLLs from `vendor/ffmpeg/bin` next to the executable.

## Phase 1 Testing Gate

Stream 1080p30 webcam to YouTube Live for 60 continuous minutes with less than 0.1% dropped frames.

1. Launch the app
2. Select your webcam
3. Enter your YouTube RTMP URL (`rtmp://a.rtmp.youtube.com/live2/STREAM_KEY`)
4. Click **Start Stream**
5. Monitor the stats bar for FPS, drop rate, and connection status
6. Run for 60 minutes and verify drop rate stays below 0.1%

## Project Structure

```
src/
  capture/     DirectShow + WASAPI capture
  core/        Compositor, StreamController, utilities
  encoder/     FFmpegEncoder
  output/      RtmpOutput
  ui/          MainWindow, PreviewWidget
vendor/        Third-party binaries
```
