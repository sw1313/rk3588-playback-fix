# RK3588 RGA Upgrade

Rebuilds official **librga 1.10.6_[3]** for the verified **AGTV Android 12 / API 32** vendor stack, replacing its **1.9.2_[1]** RGA userspace libraries through a Magisk overlay. Includes ARM64 and ARM32 libraries. RGA performs image copying, scaling and color conversion; this is an image-processing library upgrade, not a replacement VPU decoder or kernel.

The official Android NDK prebuilt cannot directly replace this firmware's library: it lacks Android `RockchipRga`/`Singleton` symbols required by existing consumers and uses a different C++ runtime. This module builds the complete Android gralloc4 configuration from the official source, using Android 12 VNDK C++ headers and the existing system `libc++.so` ABI. Upstream RGA source is unmodified. No vendor link-input binary is redistributed.

## Compatibility

Tested on H96Max V58 / RK3588, AGTV Android 12, kernel 5.10.66, RGA multicore kernel driver **1.2.15**, with MPP 1.1.0 enabled. Installation checks platform/API/ABI, exact hashes of the tested Codec2, gralloc, camera, JPEG and platform libraries, baseline RGA hashes, payload checksums and imported RGA symbols. Unknown firmware is rejected before activation.

The existing TV Codec2 build string includes a December 2023 customization date, but its Git hash also appears in an SDK manifest release. Neither establishes its exact Codec2 source revision or proves which fixes it contains. It is retained. Android 13/14 Codec2 binaries and their private framework ABI are not copied into Android 12. There is no kernel, GPU, gralloc HAL, refresh-rate, codec-rank, clock or persistent-property modification. See the [decoder upgrade scope](../docs/decoder-upgrade-scope.zh-CN.md) for the current investigation and activation requirements.

## Installation and rollback

1. Install `rk3588-rga-upgrade-0.1.0-rga1.10.6.zip` from Magisk on the running device.
2. Both ABIs are checked against the real hardware before installation succeeds. Reboot to activate.
3. Read `/data/adb/modules/rk3588_rga_upgrade/status.txt` for active-library/Codec2 load checks.
4. Magisk **Action** toggles the module's `disable` flag. Reboot after either toggle. Disabling or removing the module restores the original libraries after reboot.

If a boot load check fails, the module disables itself for rollback on the next reboot; the current boot's library mounts remain until reboot. This is a load check, not a complete playback watchdog. Restoration of both original library hashes has been verified through a real disable/reboot cycle, followed by successful re-enabling.

## Physical-device validation

ARM32 and ARM64 passed the following before installation, and active-library checks passed after reboot:

- All 11 distinct RGA symbols imported by the inspected Codec2, external/virtual camera, JPEG and legacy camera consumers resolve. ABI checks do not constitute a camera functional test.
- Real dma-buf NV12 copies at 320x240, 1280x720, 1920x1080 with 1936-pixel stride, and 3840x2160 match the source exactly in the active area.
- NV12-to-RGBA color conversion differs by at most one channel value from the tested BT.601 limited-range reference, matching the stock library's result.
- GraphicBuffer-backed 640x360-to-320x180 scaling produced zero sampled mismatches.
- A separate probe compiled against older public RGA headers passed copy, conversion and GraphicBuffer tests with both the stock and new library.
- Existing vendor Codec2 libraries load with the candidate RGA in both ABIs.

MediaCodec tests use small synthetic **640x360, 12-frame** MP4 files and explicit `c2.rk.*` hardware decoders. The active Codec2 service was verified to map the upgraded **ARM32** RGA library:

| Input | Before and after RGA upgrade |
| --- | --- |
| H.264 8-bit | 12 frames, EOS, nonempty output; bytes identical |
| AV1 8-bit | 12 frames, EOS, nonempty output; bytes identical |
| AV1 10-bit | 12 frames, EOS, nonempty output; bytes identical |
| HEVC Main10 | Existing no-Surface/copy-back path returns all-zero output before and after; unresolved |

The HEVC result is a failed copy-back test, not evidence that every HEVC Surface playback fails. After the final re-enable/reboot, the user confirmed **HEVC 10-bit playback works in Yamby/HW+**. Real AV1 content was unavailable for user testing; its evidence remains the synthetic tests above. The copy-back failure remains a limitation of this firmware's codec path; setting `codec2_fbc_disable=1` did not repair the probe, and the diagnostic property was restored. The existing HW+ Surface workaround remains applicable. The module does not claim to fix all green-screen problems or increase AV1 decoding throughput. Long-form playback, HDR output, AFBC playback, seeks, resolution transitions and thermal performance need separate application tests. RGA 4K tests are not 4K MediaCodec playback tests.

## Rebuild

Use Python 3, CMake, Ninja and a complete Windows Android NDK 29. Obtain `librga_1.10.6_[3].tar.gz` from the official [source share](https://console.zbox.filez.com/l/fuGojC), using the access code published in the [upstream README](https://github.com/airockchip/librga). Source SHA256:

```text
dae723cfc3cf4d076f21e7f5c609647408f86ac612a94ceb889163959eeb2f0b
```

Provide private `vendor64/` and `vendor32/` link-input directories from the verified device. Each needs `libc++.so`, `libui.so`, `libutils.so`, `libcutils.so`, `libhardware.so`, `libsync.so`, `libgralloctypes.so`, `libhidlbase.so`, `android.hardware.graphics.mapper@4.0.so`, and `libnativewindow.so`. These inputs must match `contracts/vendor.sha256`. Never distribute them.

```sh
python tools/build_rga.py --ndk /path/to/ndk/29 --source-archive /path/to/librga_1.10.6_[3].tar.gz --vendor-root /private/link-inputs
```

The build pins Android 12 VNDK headers by revision and checksum, strips debug information, maps source paths to neutral names and suppresses host RPATHs. See `UPSTREAM.json` and the included license notices. The separately built `mediacodec_probe` is a developer diagnostic, not an installed background service.

Sources: [official RGA project](https://github.com/airockchip/librga), [RGA changelog](https://github.com/airockchip/librga/blob/main/CHANGELOG.md), [Android 12 Codec2 RGA integration](https://github.com/khadas/android_vendor_rockchip_hardware_interfaces_codec2/blob/khadas-edge2-android12/osal/C2RKRgaDef.cpp), [Magisk overlay documentation](https://github.com/topjohnwu/Magisk/blob/master/docs/guides.md).
