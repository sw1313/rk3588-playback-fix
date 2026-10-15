# RK3588 Codec2 compatibility module 0.1.0

Control update 0.1.1 keeps the tested service and component binaries unchanged.
It additionally accepts the verified MPP/VPU compatibility profile and allows
the unified installer to preflight against hash-verified staged dependencies.
See [the unified module](../magisk-decode/README.md) for the recommended install.


[中文说明](README.zh-CN.md) · [Download](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.1.0-codec2)

For the **hash-matched AGTV Android 12 / API32 stack on H96Max V58**. This is a firmware-specific userspace compatibility module, not a universal RK3588 driver update. Android, the kernel, GPU, gralloc HAL, Yamby APK/native components and LSPosed 0.3.3 stay unchanged.

## Behavior

The module retains the original ARM32/ARM64 Codec2 component libraries and the original HIDL service instance. An ARM32 service adapter routes ordinary AVC/HEVC/VP9/AV1 Surface decoding to the candidate component. Explicit P010 copy-back requests use full-precision 10-bit conversion; legacy copy-back clients retain their original component. Selection is fixed when each decoder is created.

The complete original factory registry of 21 components is retained, with four additional candidate names. Installation and boot checks reject missing original entries. Secure decoders, encoders and other formats use the original components; this does not establish new DRM/camera support.

Changes cover AFBC handling, linear 10-bit AV1 Surface output, P010 color-range hints, color initialization and HDR static metadata. Six narrow P010 descriptor/copy patches are generated **on the device** from its hash-matched original files. No firmware library is distributed in the ZIP. No 10-to-8-bit conversion, resolution reduction or bitrate cap is added.

Original Yamby's FFmpeg copy-back path cannot consume P010. This module therefore does **not** promise to repair its HEVC Main10 **HW** mode while retaining the original player and full precision. Use **HW+** for that content.

## Installation and controls

1. Install the verified [MPP 1.1.0](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.1.1-mpp) and [RGA 1.10.6](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.1.0-rga) modules, then reboot so their libraries are active.
2. Install `rk3588-codec2-compat-v0.1.0.zip` in Magisk and reboot.
3. Check `/data/adb/modules/rk3588_codec2_upgrade/status.txt`. Default `quality` mode selects the highest verified compatible precision.

The installer checks exact firmware/library hashes and requires the tested, already-Permissive SELinux configuration. It never changes SELinux. Enforcing configurations and unknown firmware are rejected.

As root, select the ordinary component policy:

```sh
sh /data/adb/modules/rk3588_codec2_upgrade/mode-control.sh compatibility
sh /data/adb/modules/rk3588_codec2_upgrade/mode-control.sh quality
```

Reopen playback after changing mode. Compatibility mode retains the service adapter and P010 platform patches; **full rollback** requires disabling/removing the module in Magisk and rebooting. The Action button toggles the full module's disable flag, effective after reboot.

Dependency or registry failures disable the module and trigger one recovery reboot. The disable flag prevents repeated recovery boots. Both dependency-check and missing-component fault injection recovered to the stock service; full rollback verified 19 file hashes. These checks are not a complete playback watchdog.

## Validation and limits

See [VALIDATION.json](VALIDATION.json) for measurements and history:

- Fourteen Surface playback cases, including pause/resume, seek/flush, EOS and capture comparison: 13 hardware cases and the original software MPEG-4 path. Inputs cover AVC/HEVC 8-bit, HEVC/VP9/AV1 10-bit, AV1 8-bit/full range, HEVC BT.2020/PQ HDR, VP8, MPEG-2, H.263, and dimensions through 4K.
- HEVC P010: 4,147,200 visible samples exactly match the reference; 1,884,604 samples have nonzero low two bits. Adaptive resolution testing preserved 72,118,080 samples across five resolutions in one decoder.
- AVC/HEVC encoding produces decoded pixels identical to the stock baseline. Legacy AVC/AV1 copy-back output matches stock.
- The user confirmed normal Yamby HW+ picture, sound, video edges, pause/seek and re-entry with usual rendering settings on 2026-10-15. The specific gpu/gpu-next choice was not stated; separate confirmation of both renderers is not inferred.

Screenshots cannot establish 10-bit precision; raw samples provide that evidence. Correct HDR metadata does not establish every display's HDR handshake or HDMI output depth. All files/apps, long-duration/thermal behavior, DRM/secure playback, camera/recording, 8K, Dolby Vision and HDR10+ are not fully validated. Unvalidated VPU/VDPP/HAL upgrades remain disabled.

## Build and licensing

Use `tools/build_codec2.py` with NDK29 and a prepared API32 SDK. Upstream Codec2 revision: `1e9d80a10b09c9a6e0bf5cc1dbe4b6d3e66dbc2b`. Source changes are in `patches/codec2-p010.patch`, service code in `service-src/`, and the exact-byte patch generator in `src/patcher.c`. See `UPSTREAM.json` and `licenses/Apache-2.0`.

The service reuses this firmware's private ARM32 HIDL constructor ABI, including its verified 100-byte object size. Hash pinning is mandatory. The complete stock component registry is checked against the private original factory input during the build, then checked again on installation and boot. Firmware binaries are private build/link inputs and never enter the release ZIP. A clean rebuild reproduced the device-tested runtime binaries byte-for-byte.

## Unified stack 0.3.0 HDR bridge

The unified module adds `patches/codec2-hdr-bridge.patch` and the bounded helpers
in `bridge-src/`. HEVC MPP dynamic metadata is reconstructed into the original
T.35 payload and emitted through standard C2 per-frame info, including explicit
empty values. Static SMPTE2086/CTA861.3 metadata is written to compatible gralloc
buffers for the optional original-HWC adapter. These paths are controlled by the
unified module's `hdr_static` and `hdr10plus` selections. They preserve pixels
and do not imply native HDMI HDR10+ support. The old standalone installation
instructions above describe the published 0.1.0 release, not the unified build.
