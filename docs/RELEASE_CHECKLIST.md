# RailShot TV release checklist

Use this checklist for every public Windows build. The packaging steps are automated; runtime tests require a clean Windows 10/11 x64 machine.

## Build and package

- [ ] Update `project(... VERSION x.y.z)` in `CMakeLists.txt`.
- [ ] Review `THIRD_PARTY_NOTICES.md` against the exact dependency binaries.
- [ ] Build and package:

  ```powershell
  powershell -ExecutionPolicy Bypass -File scripts\package-release.ps1 -BuildInstaller
  ```

- [ ] Confirm these artifacts exist:
  - `dist\RailShotTV-Windows-x64.zip`
  - `dist\installer\RailShotTV-Windows-x64-Setup.exe`
  - `dist\RailShotTV\SHA256SUMS.txt`
- [ ] Code-sign the application, virtual-camera DLL, and installer when a signing certificate is available.
- [ ] Scan the installer and ZIP with the organization’s malware-scanning service.

## Clean-machine installation

- [ ] Install without Qt, FFmpeg, Visual Studio, or developer tools present.
- [ ] Confirm the application version in Windows file properties and **Help → About**.
- [ ] Confirm install, upgrade-over-previous-version, repair/reinstall, and uninstall.
- [ ] Confirm optional desktop and Start Menu shortcuts.
- [ ] Confirm the virtual-camera task registers the camera and uninstall removes it.
- [ ] Confirm standard-user application launch does not request elevation.
- [ ] Confirm logs and browser data are written under `%LOCALAPPDATA%`, not `Program Files`.

## Source and production checks

- [ ] Add and resize video-device, display, WGC window, image, media, text, color, and browser sources.
- [ ] Test inject-free game capture against a supported borderless/windowed game.
- [ ] Test desktop, microphone, audio-device, media, and application-audio capture.
- [ ] Verify monitoring, mute, volume, sync delay, and each audio filter.
- [ ] Verify each video filter and transition at 30 and 60 FPS.
- [ ] Verify Studio Mode Preview/Program behavior and scene collection persistence.
- [ ] Verify the RailShot scoreboard, Venue Mode, and production profiles.

## Outputs

- [ ] Stream to a private endpoint for at least 60 minutes at 1080p30.
- [ ] Test the selected hardware encoder and x264 fallback.
- [ ] Record MP4, MKV, and MOV; verify audio/video synchronization.
- [ ] Save a replay buffer clip and remux an MKV recording to MP4.
- [ ] Test ISO recording and available NDI functionality.
- [ ] Consume the RailShot virtual camera from at least two common applications.

## Remote control and security

- [ ] Verify the HTTP remote on port 8080 from another private-LAN device.
- [ ] Verify obs-websocket Hello/Identify, wrong-password rejection, common requests, batches, and events on port 4455.
- [ ] Confirm legacy RailShot WebSocket operations cannot bypass configured authentication.
- [ ] Confirm firewall guidance and remote password documentation are accurate.

## Release

- [ ] Archive the exact source revision, dependency versions, checksums, notices, and symbols.
- [ ] Tag the commit and publish release notes listing changes and known limitations.
- [ ] Upload the signed installer and portable ZIP with SHA-256 checksums.
