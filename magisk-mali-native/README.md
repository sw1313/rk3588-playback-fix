# RK3588 native Mali app driver — local compatibility test

Historical app-scoped experiment. The current global upgrade is documented in
[Mali Global 0.2.0](../magisk-mali-global/README.md); it covers both ABIs and the
system compositor without app opt-in. Keep this older module disabled when
using the global module. Its firmware-containing local ZIP is not a public asset.

This separate Magisk module registers two native Mali driver APKs using Android
12's updatable graphics-driver mechanism. It retains the original system GPU
libraries, mapper/allocator services, CSF firmware, kernel, boot, Yamby, LSPosed
and unified decode module. Only explicitly opted-in ARM64 apps use a candidate.
This is restricted to the exact checked API32 firmware and 5.10.66 kernel.

| Profile | EGL | OpenGL ES | Reported Vulkan API |
| --- | --- | --- | --- |
| Original g13p0 | 1.4 | 3.2 | 1.1.218 |
| Native g15p0 | 1.4 | 3.2 | 1.3.231 |
| Native g18p0 | 1.5 | 3.2 | 1.3.231 |

These are actual test-process reports. They are not a Vulkan 1.3 CTS result,
an increase in GPU hardware resources, a game performance guarantee, or proof
that every newly reported command/extension works through the API32 loader.
This module does not use PanVK, ANGLE or kernel hooks.

## Installation and selection

Install the local ZIP in Magisk and reboot once to register the system driver
APKs. Both APKs contain ARM64 libraries only. Their native libraries remain in
their APK namespaces; no original vendor library is overlaid. The system APKs
declare `forceQueryable` so opted-in apps can discover them under Android 12
package visibility rules. They request no permissions and contain no Java code.

The initial managed list contains only `com.sw1313.malitest`. The independent
test APK is a separate local diagnostic build. An existing installation keeps
its managed apps and selected profile. No global all-app graphics mode is set.

From a root shell:

```sh
sh /data/adb/modules/rk3588_mali_native/mode-control.sh status
sh /data/adb/modules/rk3588_mali_native/mode-control.sh app add PACKAGE
sh /data/adb/modules/rk3588_mali_native/mode-control.sh g18
sh /data/adb/modules/rk3588_mali_native/mode-control.sh g15
sh /data/adb/modules/rk3588_mali_native/mode-control.sh off
sh /data/adb/modules/rk3588_mali_native/mode-control.sh app remove PACKAGE
```

Force-stop and reopen the target app after changing selection. **Action** toggles
the selected candidate/original driver without rebooting. Magisk module disable
or removal needs reboot to remove the driver APK mounts. Existing unrelated
opt-in entries are retained. Original opt-in entries are not removed when this
module is turned off. Protected system/player package names are rejected.

The current user-selected trial game is Legend Clover (ARM64, version 4.0.0).
Its game files are unchanged. The generic package does not automatically opt
that game in on a fresh installation.

## Validation

Both candidates passed independent `dlopen`, EGL initialization, GLES3 shader
drawing with exact RGBA readback, Vulkan instance/device creation, GPU buffer
fill and exact 4096-byte readback. The same checks passed inside the framework
driver namespace, including loading all libraries directly from aligned APKs.

The installed driver packages passed a real Android Surface test in this order:
g18 → original → g15 → g18. Each case rendered at least 180 frames including
home/resume, reported zero sampled pixel errors and passed the Vulkan buffer
test. Process maps verified the selected APK and absence of the stock Mali
library for candidate cases. The original case loaded the stock vendor library.
SurfaceFlinger continued using g13p0 and all original graphics/CSF and decode
overlay hashes matched. Candidate installation/cold boot also passed.

The API32 NativeHandle V2 compatibility library passed a real Binder parcel
roundtrip for file descriptors and integers, empty vectors, and rejection of
an invalid parcelable size. No dummy allocator service, fake success return or
replacement Binder runtime is used.

The user confirmed Legend Clover 4.0.0 home, battle and exit/reopen normal on
g18p0, with no crash, black screen, corruption or obvious stutter in that check.
Its process maps and native release log confirmed g18p0. See `VALIDATION.json`.
No full Vulkan
graphics-pipeline/WSI, long-duration thermal, other-game, ARM32 candidate or
exhaustive extension coverage is claimed by the initial diagnostics.

## Build and provenance

`tools/build_mali_native.py` requires user-supplied hash-pinned g15/g18 libraries,
matching allocator/common interface libraries, original API32 Binder/C++ link
inputs, the checked API32 platform headers, SDK/NDK and a private signing key.
Use a fresh `--work` directory and pass its requested inputs. Set the keystore
password through `RK_MALI_KEYSTORE_PASSWORD`; keys are not included.

Firmware-derived Mali/interface binaries are local inputs and are not committed
to this repository. The installer checks exact original platform hashes and
each driver's independent loading before staging it.

`NativeHandle.aidl` is the immutable AOSP Android 13 frozen V2 parcelable.
The NDK backend was generated with SDK34 AIDL at minSdk32. For its two vector
fields, the generated `AParcel_readData/writeData` calls are expressed using
API32's corresponding `AParcel_readVector/writeVector` helpers. The wire format
and frozen fields are retained. It links the original API32 Binder/C++ runtimes.
`src/generated` and `src/NativeHandle.cpp` contain this generated/adapted code.

Sources:

- [AOSP NativeHandle V2](https://android.googlesource.com/platform/hardware/interfaces/+/android-13.0.0_r1/common/aidl/aidl_api/android.hardware.common/2/android/hardware/common/NativeHandle.aidl)
- [API32 graphics driver selection](https://android.googlesource.com/platform/frameworks/base/+/android-12.1.0_r11/core/java/android/os/GraphicsEnvironment.java)
- [API32 graphics namespaces](https://android.googlesource.com/platform/frameworks/native/+/android-12.1.0_r11/libs/graphicsenv/GraphicsEnv.cpp)
- [Khadas MaliG610 inputs](https://github.com/khadas/android_vendor_rockchip_common/tree/khadas-edge2-android13/gpu/MaliG610)
- [Khadas Android 14 OTA interface-library donor](https://dl.khadas.com/products/edge2/firmware/android/ota/edge2-android-14-v20250606-ota.zip)
