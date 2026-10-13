# RK3588 Playback Fix

[中文说明](README.zh-CN.md) · [Downloads](https://github.com/sw1313/rk3588-playback-fix/releases/latest)

The LSPosed module restores a selectable HW+ default in Yamby on any chipset, while its generic mpv decoder override is restricted to RK3588. A separate Magisk module prevents problematic content frame-rate switching observed on an H96Max V58 running RK3588 Android 12.

## Downloads and installation

| Asset | Version | Purpose |
| --- | --- | --- |
| `RK3588-Mpv-Fix-0.3.2.apk` | 0.3.2 | Yamby HW+ defaults on any chipset; optional RK3588 white-edge correction |
| `rk3588-frameguard-0.1.0.zip` | 0.1.0 | Magisk module to keep refresh-rate requests at 60Hz |

Install the APK, enable it in LSPosed, and select Yamby as its scope. Open the module's settings once, then force-stop and reopen Yamby. In **Settings → MPV Player → Default decoding mode**, choose **HW+**. The adapter was tested with Yamby **2.1.0.11 (20010012)** and LSPosed **2.2.1**. Future Yamby releases may need a new adapter.

Yamby's settings adapter has no chipset restriction. The APK requires Android 8.0 or later and a working LSPosed installation. Actual HW+ playback still depends on the device's MediaCodec implementation and supported formats. Cross-chipset scope is enabled in code; physical-device playback has only been tested on RK3588. Yamby never receives the generic forced decoder override, including when its settings adapter cannot be installed.

Install the ZIP using Magisk. Its service runs after boot completes. It requires **RK3588 and Android API 32**. Each module can be used independently.

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

Version 0.3.2 adds **Yamby HW+ top gray/white edge correction** in the module's settings. It is **off by default** and only installs on RK3588. Enable it only for the reproduced four-pixel top edge, then force-stop and reopen Yamby.

The tested H.264 and HEVC decoders report `crop-top=4` and a 1920×1080 active image inside a 1920×1104 hardware buffer. The AImageReader rendering path exposes the padding at the top. The module applies a four-pixel RGB sampling compensation for 1080p H.264/HEVC MediaCodec playback, supporting both the padded texture and the 1080p intermediate texture produced by debanding. It retains the correction across player recreation and shader-list changes, while preserving existing shaders. Other codecs, resolutions and software decoding are excluded when the decoder properties become available. Rendering with `vo=gpu` and Anime4K/debanding was tested; other rendering paths remain unverified.

This is a rendering workaround, not a vendor-driver patch. It does not zoom the image or change its aspect ratio. If earlier preprocessing has already discarded edge rows, it cannot reconstruct them. On/off comparison reproduced and removed the edge; the user confirmed playback re-entry remained clear and both the H.264 and HEVC test videos were clear after switching between them.

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
./gradlew -p lsposed :app:assembleDebug :app:lintDebug
python3 tools/package_magisk.py
```

On Windows use `gradlew.bat` and `python`. The APK is generated under `lsposed/app/build/outputs/apk/debug/`; the ZIP is generated under `dist/`. The official Xposed API 82 is a `compileOnly` dependency and is not bundled into the APK. The published APK is a debug-signed build; signing keys are not included.

## Validation and privacy

- APK assembly and Android Lint passed.
- Version 0.3.1 passed eight isolated hook-scope scenarios using the actual entry-point code and mocked Android/Xposed boundaries, including non-RK Yamby routing, RK-only generic hooks, unknown adapters, software decoding and renderer preservation. These checks do not simulate physical non-RK decoding or firmware. The updated module also loaded successfully on the RK3588 test device.
- Yamby's HW+ default was retained across an application restart, and the previously green video rendered correctly.
- Magisk installation and Android shell syntax checks passed. Isolated settings-store checks covered backup, repeated application, manual changes, unset values, pause/resume, and uninstall restoration.
- Automatic application following a full device reboot still needs verification. Other players and firmware versions have not been verified.

The modules have no Internet permission or telemetry. Decoder diagnostics use `RKPlaybackFix` and record codec/render options rather than media URLs. This repository and its release assets exclude device logs, screenshots, network addresses, local user paths, and private signing keys.

Gradle wrapper files originate from Gradle and carry their Apache-2.0 notices; the license text is included in `third_party/Apache-2.0.txt`. The Xposed API is provided by [rovo89/XposedBridge](https://github.com/rovo89/XposedBridge), under Apache-2.0, and is supplied by the installed framework at runtime. Neither Yamby nor mpv binaries are distributed here.
