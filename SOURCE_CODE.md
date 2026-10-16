# Source and build guide

## Exact release source

Repository: https://github.com/sw1313/rk3588-playback-fix

Release **v0.6.7** is the licensing/documentation snapshot: Decode Stack 0.6.7,
Mali Global 0.2.1, plugin 0.3.5 and Frame Guard 0.1.1. OPEN_SOURCE.json in each
installer identifies its component version and tag. Checkout that tag instead
of a moving branch. The release's project-source ZIP contains the tracked
repository and licenses, excluding `.git`, build output, private inputs and
installers. Its SHA256 is published alongside the four installers.

```sh
git clone https://github.com/sw1313/rk3588-playback-fix.git
cd rk3588-playback-fix
git checkout --detach v0.6.7
```

The project-source ZIP is not an all-in-one Android BSP. Download the pinned
public upstream sources below; some system components also require your own
legitimately obtained, exact-hash-matching firmware link inputs. Those inputs
are not redistributed and a source checkout alone cannot rebuild every system
library. No bit-for-bit rebuild or arbitrary-firmware compatibility is claimed.
v0.6.7 repackages already validated binaries; see its validation record for
the exact byte-preservation checks, not a claim of fresh runtime compilation.

## Public source pins and modifications

| Component | Exact public baseline | Local changes / builder |
| --- | --- | --- |
| MPP / libvpu | [rockchip-linux/mpp](https://github.com/rockchip-linux/mpp/tree/c08762ebfadeb4e986d2fed993bc7a54862d3ebe), 1.1.0 | `magisk-mpp/compat/*.patch`, `mpi_legacy_prep.h`; `tools/build_mpp.py` |
| librga | Official [airockchip/librga source share](https://console.zbox.filez.com/l/fuGojC), `librga_1.10.6_[3].tar.gz`, SHA256 `dae723cfc3cf4d076f21e7f5c609647408f86ac612a94ceb889163959eeb2f0b` | Android gralloc4 build; `tools/build_rga.py`, `magisk-rga/UPSTREAM.json` |
| Codec2 API32 | [Khadas Codec2](https://github.com/khadas/android_vendor_rockchip_hardware_interfaces_codec2/tree/1e9d80a10b09c9a6e0bf5cc1dbe4b6d3e66dbc2b) | `magisk-codec2/patches/`, `service-src/`, `bridge-src/`; `tools/build_codec2.py` |
| Codec2 1.14.3 SIMD | [Khadas Codec2](https://github.com/khadas/android_vendor_rockchip_hardware_interfaces_codec2/tree/076fe2f6ec28546d329964ae9e175301717df6ab) | `magisk-codec2-sdk/patches/`; `tools/build_codec2_sdk.py` |
| Codec2 1.14.16 | [Khadas Codec2](https://github.com/khadas/android_vendor_rockchip_hardware_interfaces_codec2/tree/e3c30156793f778541b6eaec54ac2a1ac0f6a70c) | `magisk-codec2-sdk-next/patches/` in builder order; same builder with `--sdk-next` |
| Android Codec2/platform headers | [AOSP frameworks/av android-12.1.0_r11](https://android.googlesource.com/platform/frameworks/av/+/android-12.1.0_r11/media/codec2/), corresponding AOSP hardware headers; [VNDK v32](https://android.googlesource.com/platform/prebuilts/vndk/v32/+/e1ba8043f8368220b412c18ee8392850ab009aae/arm64/include/) | Prepared SDK header inputs; full hash contracts in component `contracts/` |
| libyuv 1974 | [Chromium libyuv](https://chromium.googlesource.com/libyuv/libyuv/+/0b9e5c922deec7c9921ae385f6242a9a27754d6e) | `tools/build_libyuv.py`; removes no original public exports |
| libyuv legacy API objects | [AOSP external/libyuv](https://android.googlesource.com/platform/external/libyuv/+/715a8b0584d482569ed39ee95291cfa9061c6aaf/) | Isolated symbol renaming and selected legacy objects in same builder |
| libvpx | [WebM libvpx](https://chromium.googlesource.com/webm/libvpx/+/6df3ec34557879fff673706f4a1d9fbd0f3a6f0e), v1.17.0 | Decoder-only PIC, VP9 high bit depth; `tools/build_software.py` |
| libgav1 | [Chromium libgav1](https://chromium.googlesource.com/codecs/libgav1/+/085032331f60c10055d32ab52932bd7bd77e6f53) | Built by `tools/build_software.py`; no tests/examples in payload |
| Software Codec2 wrappers | [AOSP android-12.1.0_r27 components](https://android.googlesource.com/platform/frameworks/av/+/android-12.1.0_r27/media/codec2/components/) | `magisk-software/patches/full-precision-p010.patch` and `src/full_precision.h`; baseline hashes in builder |
| HDR adapter and compatibility probes | This release's `magisk-hdr/src/`, contracts and public platform headers | `tools/build_hdr.py`; original HWC is obtained/patched locally, never bundled |
| Mali open-source interfaces | [AOSP hardware/interfaces android-13.0.0_r1](https://android.googlesource.com/platform/hardware/interfaces/+/android-13.0.0_r1/) | Frozen common V2 / graphics.common V3 / allocator V1 and Binder headers under `magisk-mali-global/src/`; `tools/build_mali_global.py` |
| Mali proprietary input | [Khadas fixed commit](https://github.com/khadas/android_vendor_rockchip_common/tree/6cf5e034b39c19dfd9e1a15ae5227f61c127d128/gpu/MaliG610) | Separate install-time download, hash/size pins in `contracts/driver-sources.txt`; not open-source project material |
| Plugin / Frame Guard / WebUI | This release's `lsposed/`, `magisk/`, `magisk-decode/webroot/` | Original project source; no player or framework binary bundled |

## Toolchain and input preparation

Use Python 3.10+, Git, CMake, Ninja, JDK 17, and a complete Android NDK.
Existing system builds used NDK 29.0.14206865, target API32. Software libvpx
also needs a POSIX shell/GNU Make. Plugin: SDK platform 35, AGP 8.7.3 and
Gradle 8.11.1 via the supplied wrapper. Mali AIDL compiler defaults to SDK
Build Tools 34.0.0; D8 uses 35.0.0. Compiler/tool updates need fresh validation.

All paths below are placeholders. Do not commit `local.properties`, keys,
firmware libraries, media, logs, generated device data or private paths.
Build in a separate workspace. System scripts reject unknown hashes; do not
bypass contracts to make an arbitrary firmware build succeed.

The Codec2 `--prepared-root` is a developer-prepared directory, not a published
SDK archive. It needs `codec2-a12` at the pin above, `aosp-codec2` headers from
the listed AOSP tree, `aosp-hardware`, matching `platform-private` headers,
and `vendor32` link libraries enumerated in `build_codec2.py`. VNDK headers
come from the pinned archive; generated headers must match API32. RGA needs
`vendor32`/`vendor64` inputs. Software decoder `--link-libs` uses the original
APEX libraries named in its builder. HDR needs original platform link libraries
and matching DRM UAPI headers. These dependencies are prerequisites, not
automatically recoverable from the project source ZIP.

## Four installer builds

### LSPosed plugin

```sh
./gradlew -p lsposed :app:assembleDebug :app:testDebugUnitTest
```

On Windows use `gradlew.bat`; set ANDROID_HOME or a local untracked SDK path.
APK output: `lsposed/app/build/outputs/apk/debug/app-debug.apk`. Build-time
license staging is automatic into `assets/open-source/`. Xposed API 82 is
compile-only; JUnit is test-only. The public package uses the existing debug
signing identity for upgrade continuity. Signing keys are excluded; another
developer's build will not have the same certificate and cannot replace that
installed APK without their own migration/uninstall. Do not distribute keys.

### Frame Guard

```sh
python tools/package_magisk.py
```

Reads the version from `magisk/module.prop`, uses LF endings and packages
scripts plus open-source notices. No proprietary SDK input is needed.

### Native Mali Global

```sh
python tools/build_mali_global.py --ndk /inputs/ndk --sdk /inputs/sdk --headers /inputs/vndk32 --binder32 /inputs/vendor32/libbinder_ndk.so --binder64 /inputs/vendor64/libbinder_ndk.so --libcpp32 /inputs/vendor32/libc++.so --libcpp64 /inputs/vendor64/libc++.so --work /work/mali
```

Rebuilds both architectures of AOSP interface libraries, downloader and probes;
no proprietary driver or original firmware library is copied into the ZIP.
Use firmware-matching link inputs, not host libraries. Installation still
downloads the separately licensed, pinned Mali binaries.

### Decode Stack

First build component packages, using fresh work directories. Run each builder
with `--help` for exact required input names. Representative commands:

```sh
python tools/build_mpp.py --ndk /inputs/ndk --work /work/mpp
python tools/build_rga.py --ndk /inputs/ndk --source-archive /inputs/librga_1.10.6_3.tar.gz --vendor-root /inputs/firmware --headers-archive /inputs/vndk32-headers.tar.gz --work /work/rga
python tools/build_codec2.py --ndk /inputs/ndk --prepared-root /inputs/api32 --vndk-headers /inputs/vndk32 --rga-source /work/rga/source --work /work/codec2
python tools/build_codec2_sdk.py --upstream /inputs/codec2-1.14.3 --prepared-root /inputs/api32 --vndk-headers /inputs/vndk32 --ndk /inputs/ndk --rga-source /work/rga/source --libhardware /inputs/system32/libhardware.so --work /work/codec2-sdk
python tools/build_codec2_sdk.py --sdk-next --upstream /inputs/codec2-1.14.16 --prepared-root /inputs/api32 --vndk-headers /inputs/vndk32 --ndk /inputs/ndk --rga-source /work/rga/source --libhardware /inputs/system32/libhardware.so --work /work/codec2-next
python tools/build_libyuv.py --ndk /inputs/ndk --stock-libyuv /inputs/system64/libyuv.so --stock-libjpeg /inputs/system64/libjpeg.so --jpeg-headers /inputs/jpeg-headers --work /work/yuv
python tools/build_software.py --ndk /inputs/ndk --prepared-root /inputs/api32 --vndk-headers /inputs/vndk32 --link-libs /inputs/apex64 --vpx-source /inputs/libvpx --vpx-build /work/vpx --gav1-source /inputs/libgav1 --work /work/software
python tools/build_hdr.py --ndk /inputs/ndk --headers /inputs/vndk32 --private-libs /inputs/system64 --drm-headers /inputs/drm-headers --work /work/hdr
```

Configure/build the pinned libvpx source first with NDK API32 cross tools and:

```text
--target=arm64-linux-gcc --disable-examples --disable-tools --disable-docs
--disable-unit-tests --disable-vp8-encoder --disable-vp9-encoder --enable-pic
--enable-vp9-highbitdepth --disable-shared --enable-static --disable-webm-io
```

Combine the **clean, pre-install package directories**, not device module
directories (which may contain locally generated firmware patches):

```sh
python tools/build_decode_stack.py --mpp magisk-mpp --rga magisk-rga --codec2 /work/codec2/package --codec2-sdk /work/codec2-sdk/package --codec2-sdk-next /work/codec2-next/package --libyuv /work/yuv/package --software /work/software/package --hdr /work/hdr/package --work /work/stack
```

MPP/RGA builders stage their own component output under `magisk-mpp` and
`magisk-rga`; generated output is ignored by Git. Preserve supplied upstream
license files and patent grants. The license staging helper adds common
documentation before generating package integrity manifests.

## Validation and publishing

Run plugin unit tests and resource tests; validate ZIP/APK notices, upstream
license copies, nested SHA256 manifests and all source/archive privacy before
upload. `tools/verify_open_source.py` checks this release's required notices and
source links. Changes to runtime libraries require the separate device playback,
pixel precision, lifecycle and rollback matrix; packaging tests do not replace
those tests. Publish all four installers, source ZIP, manifest, validation and
SHA256SUMS together. Never overwrite an older immutable package with new bytes.

## Licensing boundaries

Apache-2.0 applies to original project code. Preserve upstream notices and the
licenses/patent grants in LICENSES and component directories. Upstream source
links do not grant permission to redistribute proprietary Mali or firmware.
No boot/kernel update, Dolby component or player modification is introduced.
