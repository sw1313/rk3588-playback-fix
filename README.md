# RK3588 Playback Fix

[**Current downloads / 当前下载**](DOWNLOADS.md) · [Download table](https://github.com/sw1313/rk3588-playback-fix/blob/main/DOWNLOADS.md) · [中文说明](README.zh-CN.md)

**[Project release v0.6.3 — all four installers](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.3)**

Every public Release is a complete project snapshot with Decode Stack, Native
Mali Global, the Yamby/mpv plugin and Frame Guard attached together. The project
version identifies the snapshot; each package retains its own component version.
GitHub **Latest** points to the newest complete project release. System components
remain experimental and restricted to the firmware hashes in their contracts.

| Component | Current version | Purpose |
| --- | --- | --- |
| [Decode Stack](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.3) | **0.6.3 — experimental** | Full-precision AV1 AFBC output, Codec2 1.14.16 and cumulative decode updates with feature/whole-stack rollback |
| [Native Mali Global](magisk-mali-global/README.md) | **0.2.0 — experimental** | Global ARM32/ARM64 native Mali g18p0 userspace graphics paths |
| [Yamby/mpv LSPosed plugin](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.3) | **0.3.3** | Default HW+ setting and optional RK3588 edge correction; this is the plugin version, not the LSPosed framework version |
| [Frame Guard](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.3) | **0.1.0** | Optional fixed-refresh workaround for the reproduced HDMI blackouts |

## Decode Stack 0.6.3

Decode Stack **0.6.3** adds guarded lossless AV1 AFBC display output, preserving
the source bit depth, HDR and signaled film grain. The final binary rendered
all 3597 frames of the local 60-second 4K60 HDR source with zero player drops;
all 3597 render callbacks arrived with no gap over 25ms. A same-route prototype
also completed the 314-second source; that is distinct from final-binary validation.
See [actual playback, exact pixels and limits](docs/decode-av1-063.zh-CN.md).

## Choose and install

Use the exact filenames and links in [DOWNLOADS.md](DOWNLOADS.md). The Mali,
LSPosed and Frame Guard packages are unchanged; they do not need a new version
number when Decode Stack changes. Old releases remain available for rollback
and source history. The old system collection contains Decode Stack **0.3.2**,
not the current decoder package.

For a system decoder upgrade, install only the unified Decode Stack ZIP in
Magisk and reboot. Keep the superseded standalone MPP/RGA/Codec2/libyuv modules
disabled. The installer validates dependencies before disabling overlapping
modules. [Switches, dependency handling and rollback](magisk-decode/README.md)
remain available; changes take effect after reboot.

Decode Stack and global Mali accept only the firmware hashes in their
contracts. Testing targets an H96Max V58 with RK3588, Android 12/API32 and the
original 5.10.66 kernel. These packages do not flash boot, replace the kernel,
modify Yamby files or add LSPosed features. Global Mali's installer downloads
about 69 MiB of SHA256-pinned upstream libraries; closed Mali binaries are not
included in the public ZIP. The retired app-scoped Mali experiment must remain
disabled when using the global module.

## Decode Stack 0.5.1 and known limits

0.5.1 repairs the reproduced legacy AVC/HEVC 8-bit buffer-output stop/drain
failure through a guarded SDK14/API32 backend route. It adds a `codec2_buffer`
switch and retains the backend and whole-module rollback controls. Surface,
P010, ten-bit samples, resolution and bitrate are preserved. Unknown/ten-bit
buffer profiles and AV1/VP9 retain their prior selection; this is not a fix
for every copy-back client. Read the [0.5.1 comparison and validation](docs/decode-buffer-051.zh-CN.md).

The AnTuTu 12.0.2 full editing pipeline still has a premature-EOS/completion
problem. It was also reproduced with the original decode stack under controlled
source-callback scheduling pressure. An activity finishing does not prove all
450 source frames were preserved; a 449-frame result was observed. No global
EOS suppression, frame duplication or score workaround is shipped. See the
[complete editing investigation](docs/antutu-editing-eos.zh-CN.md).

The image-effects process also has an unresolved exit-path crash. It reports
test results, logs test exit, then encounters a native SIGSEGV; its crash
reporting thread subsequently aborts on a destroyed mutex. The first-fault
callback is not fully identified; no original-Mali comparison or fix is claimed.
See the [exit-path evidence](docs/antutu-image-effects.zh-CN.md).

0.6.3 local playback, precision, lifecycle and reboot rollback tests passed.
No new manual Yamby acceptance is recorded for 0.6.3; its original libraries and
the LSPosed plugin remain unchanged. Earlier confirmations are historical. Finite sample coverage does not establish universal compatibility.
HDR10+ refers to the standard Codec2 metadata bridge, not native HDMI HDR10+
output. Dolby Vision/LLDV/FEL and kernel-driver upgrades are not included.

## Component documentation

- [Unified decode contents, switches and rollback](magisk-decode/README.md)
- [SDK core port and kernel boundary](docs/decode-sdk-050.zh-CN.md)
- [MPP/libyuv upgrade scope](docs/decode-upgrade-040.zh-CN.md)
- [Native Mali Global installation and validation](magisk-mali-global/README.md)

Historical standalone releases are not the current installation path. Their
firmware-specific experiments and validation remain under `magisk-mpp`,
`magisk-rga` and `magisk-codec2` for reference. Do not stack them on the unified
module.

## HW versus HW+

These names describe Yamby's current mpv settings; other players may use different labels.

| Mode | mpv setting | Frame path |
| --- | --- | --- |
| HW | `mediacodec-copy` | Hardware decoding followed by a copy back to system memory |
| HW+ | `mediacodec` | Hardware decoding through Android's MediaCodec Surface path, avoiding that copy back |
| SW | `no` | Software decoding |

Both HW and HW+ use the hardware video decoder. The direct path generally reduces copying overhead; it does not inherently improve image quality. See the [mpv hardware-decoding documentation](https://mpv.io/manual/stable/#options-hwdec).

On the tested device, affected HEVC Main10 content produced green frames through the copy-back path and displayed correctly with HW+. The precise vendor driver fault has not been established.

The module adds HW+ to Yamby's own default-mode menu and preserves that saved value when Yamby initializes its player. Manual HW/HW+/SW choices remain available. **It does not override `vo`: `gpu` and `gpu-next` remain under the player's control.**

For other mpv applications on **RK3588 only**, select the application in LSPosed and enable the module's hardware-decoding preference. It detects common MPVLib string setters and supported delayed bindings, replacing copy/auto hardware-decoding requests with `mediacodec`. Explicit software decoding is retained. Only Yamby has been tested on the device; fully obfuscated or native-only bindings may need separate adapters. Other chipsets receive no generic mpv hooks. No hooks are applied to Android system processes.

## Optional Yamby white-edge correction

Version 0.3.3 provides **Yamby HW+ adaptive gray/white edge correction** in the module's settings. It is **off by default** and remains RK3588-only. Enable it for reproduced padding at the video edge, then force-stop and reopen Yamby.

The tested decoder reports `crop-top=4` with a 1920×1080 active image in a 1920×1104 hardware buffer. Version 0.3.3 no longer hard-codes that resolution, offset or H.264/HEVC codec list. It listens to MediaCodec crop reports and AImageReader buffer dimensions through Yamby's mpv log callback, and matches them against the current video's dimensions and non-copy hardware-decoding state. Missing, zero, invalid or mismatched crop metadata produces no correction. Switching files and recreating the player discard previous metadata.

The generated RGB shader uses the reported top/left offset and accepts either the original padded texture or an intermediate texture at the active video's dimensions. Its execution condition supports both `gpu` and `gpu-next` semantics. Existing shaders and the player's renderer choice are preserved; shader-list resets reattach the current correction. This relies on Yamby exposing the relevant verbose decoder/render messages; different wrappers or future message formats may require an adapter update.

This is rendering compensation, not a vendor-driver patch. It does not zoom the image or change its aspect ratio. It cannot reconstruct edge pixels discarded by earlier processing. 1080p H.264/HEVC correction was previously confirmed with `gpu`; the user has now confirmed the adaptive correction removed the edge with `gpu-next`. 720p/4K and other codecs have metadata/shader tests, which do not establish physical-device playback support. It is intended for the observed RK3588 AImageReader padding fault, not every possible edge artifact.

## Frame Guard

The tested firmware repeatedly requested 30/60Hz mode changes that the Rockchip HWC rejected with `BadConfig`, causing brief whole-screen blackouts. Disabling content mode switching and fixing the requested range at 60Hz resolved the reproduced issue.

Frame Guard applies the following once after boot:

```text
secure match_content_frame_rate = 0
system min_refresh_rate = 60
system peak_refresh_rate = 60
```

It backs up the original values to `/data/adb/rk3588-frameguard/original.conf` before its first application. Magisk's **Action** button toggles restore/apply. Uninstall restores values still owned by the module and preserves subsequent manual changes. **Disabling the module alone does not undo persisted Android settings**; use Action or uninstall to restore them. Fixed 60Hz disables automatic matching to a video's frame rate.

## Build

Use JDK 17 and an Android SDK containing platform 35. Set `ANDROID_HOME` to your SDK directory or configure an untracked `lsposed/local.properties`.

```sh
./gradlew -p lsposed :app:assembleDebug :app:lintDebug :app:testDebugUnitTest
python3 tools/package_magisk.py
```

On Windows use `gradlew.bat` and `python`. The APK is generated under `lsposed/app/build/outputs/apk/debug/`; the ZIP is generated under `dist/`. The official Xposed API 82 is a `compileOnly` dependency and is not bundled into the APK. The published APK is a debug-signed build; signing keys are not included.

## Validation and privacy

- APK assembly, Android Lint and crop geometry unit tests passed. Generated 1080p, 720p and 4K shaders also compiled with a GLSL ES 3.20 validator. Isolated hook checks covered metadata availability, shader preservation, file/player resets, resolution/codec changes, SW exclusion and scope boundaries.
- Version 0.3.1 passed eight isolated hook-scope scenarios using the actual entry-point code and mocked Android/Xposed boundaries, including non-RK Yamby routing, RK-only generic hooks, unknown adapters, software decoding and renderer preservation. These checks do not simulate physical non-RK decoding or firmware. The updated module also loaded successfully on the RK3588 test device.
- Yamby's HW+ default was retained across an application restart, and the previously green video rendered correctly.
- Magisk installation and Android shell syntax checks passed. Isolated settings-store checks covered backup, repeated application, manual changes, unset values, pause/resume, and uninstall restoration.
- Automatic application following a full device reboot still needs verification. Other players and firmware versions have not been verified.

The LSPosed APK and Frame Guard have no Internet permission or telemetry. The separate global Mali installer downloads its pinned upstream libraries. Decoder diagnostics use `RKPlaybackFix` and record codec/render options rather than media URLs. This repository and its release assets exclude device logs, screenshots, network addresses, local user paths, and private signing keys.

Gradle wrapper files originate from Gradle and carry their Apache-2.0 notices; the license text is included in `third_party/Apache-2.0.txt`. The Xposed API is provided by [rovo89/XposedBridge](https://github.com/rovo89/XposedBridge), under Apache-2.0, and is supplied by the installed framework at runtime. Neither Yamby nor mpv binaries are distributed here.
