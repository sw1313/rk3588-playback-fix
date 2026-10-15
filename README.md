# RK3588 Playback Fix

[中文说明](README.zh-CN.md) · [Downloads](https://github.com/sw1313/rk3588-playback-fix/releases/latest)

The LSPosed module restores a selectable HW+ default in Yamby on any chipset, while its generic mpv decoder override is restricted to RK3588. A separate Magisk module prevents problematic content frame-rate switching observed on an H96Max V58 running RK3588 Android 12.

## Downloads and installation

| Asset | Version | Purpose |
| --- | --- | --- |
| `RK3588-Mpv-Fix-0.3.3.apk` | 0.3.3 | Yamby HW+ defaults on any chipset; optional RK3588 white-edge correction |
| `rk3588-frameguard-0.1.0.zip` | 0.1.0 | Magisk module to keep refresh-rate requests at 60Hz |

Install the APK, enable it in LSPosed, and select Yamby as its scope. Open the module's settings once, then force-stop and reopen Yamby. In **Settings → MPV Player → Default decoding mode**, choose **HW+**. The adapter was tested with Yamby **2.1.0.11 (20010012)** and LSPosed **2.2.1**. Future Yamby releases may need a new adapter.

Yamby's settings adapter has no chipset restriction. The APK requires Android 8.0 or later and a working LSPosed installation. Actual HW+ playback still depends on the device's MediaCodec implementation and supported formats. Cross-chipset scope is enabled in code; physical-device playback has only been tested on RK3588. Yamby never receives the generic forced decoder override, including when its settings adapter cannot be installed.

Install the ZIP using Magisk. Its service runs after boot completes. It requires **RK3588 and Android API 32**. Each module can be used independently.

A separate [experimental MPP userspace upgrade](https://github.com/sw1313/rk3588-playback-fix/releases/tag/mpp-v0.1.1) provides MPP 1.1.0 ARM32/ARM64 libraries for the exact tested Android 12 vendor stack. Version 0.1.1 fixes a false boot-check failure; installation, reboot and active-library checks passed on the physical device. It is independent of Frame Guard and the LSPosed APK. Read its [compatibility, validation and rollback instructions](magisk-mpp/README.md) before installation. Later short MediaCodec tests passed for H.264 and AV1 8/10-bit; the pre-existing HEVC Main10 copy-back failure remains unresolved. These are limited synthetic tests, not full player validation.

A separate [RGA userspace upgrade](magisk-rga/README.md) rebuilds librga 1.10.6 for the verified Android 12 gralloc4/system ABI. Both ABIs passed actual RGA jobs; installed MediaCodec H.264 and AV1 8/10-bit copy-back samples match their baseline output. The existing HEVC Main10 copy-back failure remains unresolved. The module includes strict firmware guards and a verified disable/reboot rollback. See its validation limits before installation.

## System Codec2 compatibility

The [Codec2 compatibility module 0.1.0](magisk-codec2/README.md) adds verified full-precision Surface/P010 paths, AV1 color-range handling and HDR static metadata on the exact tested API32 firmware. All 21 original component entries are retained. Fourteen playback cases, AVC/HEVC encoding, pixel-precision checks, reboot rollback and automatic recovery passed; the user also confirmed normal Yamby HW+ playback. Yamby components and LSPosed 0.3.3 are unchanged. Original Yamby HEVC Main10 **HW copy-back** remains limited by its lack of P010 support; use **HW+**.

Install the existing [MPP 1.1.0 module](https://github.com/sw1313/rk3588-playback-fix/releases/tag/mpp-v0.1.1) and [RGA 1.10.6 module](https://github.com/sw1313/rk3588-playback-fix/releases/tag/rga-v0.1.0), reboot to activate them, then install [Codec2 0.1.0](https://github.com/sw1313/rk3588-playback-fix/releases/tag/codec2-v0.1.0) and reboot. The installer rejects unverified firmware and SELinux Enforcing configurations; it never changes enforcement. These system modules are published as pre-releases because other firmware and all content are not validated.

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

The modules have no Internet permission or telemetry. Decoder diagnostics use `RKPlaybackFix` and record codec/render options rather than media URLs. This repository and its release assets exclude device logs, screenshots, network addresses, local user paths, and private signing keys.

Gradle wrapper files originate from Gradle and carry their Apache-2.0 notices; the license text is included in `third_party/Apache-2.0.txt`. The Xposed API is provided by [rovo89/XposedBridge](https://github.com/rovo89/XposedBridge), under Apache-2.0, and is supplied by the installed framework at runtime. Neither Yamby nor mpv binaries are distributed here.
