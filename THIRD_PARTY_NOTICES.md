# Third-party notices

RailShot TV Broadcaster uses third-party software. The applicable license depends on the exact binaries placed in `vendor/` and included by the release packaging process.

- **Qt 6** — Copyright The Qt Company Ltd. and contributors. Available under commercial terms or the LGPL/GPL terms published at <https://www.qt.io/licensing/>.
- **FFmpeg** — Copyright the FFmpeg developers. Available under the LGPL 2.1-or-later or GPL 2-or-later depending on build configuration. See <https://ffmpeg.org/legal.html>.
- **librtmp / rtmpdump** — Copyright the rtmpdump contributors. Distributed under the LGPL 2.1-or-later.
- **Microsoft WebView2 SDK and Runtime** — Copyright Microsoft Corporation. See the Microsoft Edge WebView2 license terms.
- **DirectShow virtual-camera support** — contains portions derived from code by Lain Bailey and other contributors, licensed under the GNU Lesser General Public License 2.1-or-later. Copyright notices remain in the relevant source files under `plugins/railshot-virtualcam/`.
- **NDI SDK** — optional and distributed only when supplied by the builder, subject to the NDI SDK license.

Release publishers are responsible for reviewing the actual dependency builds, retaining their license texts, and satisfying source/relocation requirements before distribution. This notice does not declare a license for RailShot TV Broadcaster itself.
