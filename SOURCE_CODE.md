# Release v0.8.8 · Mali Global 0.4.4 root compatibility

Root admission checks actual startup interfaces and local boot layout, without root-version or official binary-hash whitelists. Firmware/ABI, original-init and per-device backup/rollback guards remain. Ordinary installers do not reject a root brand; dedicated KernelSU/APatch first-binding adapters are not implemented. The existing Magisk 30.7 target passed physical installation and reboot; other variants have offline coverage only.

To reproduce the published package, select its source tag and use the hash-pinned public Mali 0.4.2 base ZIP with NDK 29.0.14206865:

```text
git checkout --detach v0.8.8
python tools/cold-boot/generate_contract.py --check
python tools/build_mali_boot_policy.py --base <public-0.4.2.zip> --ndk <ndk-29.0.14206865> --work <fresh-dir> --qms-barrier --source-ref v0.8.8
```

The builder verifies that the rebuilt keeper matches the hardware-tested binary. All 36 runtime files match the accepted interface policy; only version, documentation, static UI cache marker, provenance and checksums changed during formalization. Test-stage names remain in historical evidence only. See [release validation](docs/validation/release-0.8.8.json) and [root compatibility](docs/root-compatibility.zh-CN.md). Official APK hashes are reproducible regression inputs, not runtime/preparation admission criteria.

Decode 0.8.6, client 0.3.7 and optional PanVK Test8.1 retain their original bytes and source tags. Frame Guard is omitted. Private firmware/link inputs, root APKs, boot/profile images, logs and keys are excluded. Per-device first-binding preparation and rollback guards still apply. This update does not add gameplay/HDR/physical-blackout acceptance.

Documentation on main can receive follow-up corrections. Reproduce published assets from the tag in their `OPEN_SOURCE.json`; documentation updates do not rewrite existing installers or checksums. Historical commands below require their corresponding tags.

<details>
<summary>Historical builds and development notes — use their corresponding tags</summary>

# Release v0.8.7 · Mali Global 0.4.3 boot validation

本版以固件组件检查代替个人整份 boot 哈希：`magisk-mali-global/contracts/boot-firmware.json` 固定内核、DTB 和关键启动契约，保留本机 root 与个人原件／候选完整性校验。允许差异及拒绝边界见 [启动文档](magisk-mali-global/boot/README.md)，真实安装、首次绑定、整包回退及重新启用见 [验收记录](docs/validation/mali-boot-compat-043.json)。未知内核、DTB、root 方案和冲突启动钩子仍拒绝。

Build the formal Mali package from the hash-pinned public 0.4.2 ZIP:

```text
python tools/cold-boot/generate_contract.py --check
python tools/build_mali_boot_policy.py --base <public-0.4.2.zip> --ndk <ndk> --work <fresh-dir> --qms-barrier --source-ref v0.8.7
```

The builder reuses unchanged GPU/KMD/CSF/interface bytes and rebuilds only the boot keeper/policy. It verifies the formal keeper equals the actual tested binary. The full `tools/build_mali_global.py` also supports `--qms-barrier`. Per-device private preparation uses `tools/cold-boot/prepare.py`; its boot/profile must match the chosen keeper. Existing private backup stores are not replaced automatically. Foreign boot changes after activation still block full-image rollback.

Mali 0.4.3 is new; Decode 0.8.6 and client 0.3.7 are carried byte-for-byte from v0.8.6, optional PanVK Test8.1 from v0.6.9. Their original provenance stays intact. Frame Guard is omitted from this release and stays historical. No private firmware, boot images, logs, keys or closed Mali/CSF blobs are included. This release does not add new game/HDR/physical-blackout acceptance.

以下为历史版本的构建和验证记录，当前状态以上方说明及对应验收记录为准。

# Release v0.8.6 / 正式版源码与构建

Release components: Decode Stack 0.8.6, Mali Global 0.4.2, LSPosed 0.3.7. The accepted system refresh policy lives in `magisk-hdr/src/refresh_session.cpp`; `tools/build_refresh_session.py` builds its exact-firmware loader chain and `tools/build_decode_stack.py --refresh-session <component>` integrates it. `docs/validation/decode-refresh-session-086.json` records the tested binary SHA and bounded physical feedback.

Use the existing component builders with hash-pinned private firmware as link/patch inputs. Public installers contain source-built shims, patches and contracts; original SF/HWUI/HWC, services.jar/OAT, boot images, closed Mali/CSF blobs, device logs and signing keys are excluded. Installers generate the firmware variants locally. QMS and private first-binding preparation are independently gated; a release package is not a universal boot adaptation.

The release retains the accepted executable code and regenerates version, bilingual UI, provenance and integrity manifests. FFmpeg build-directory strings are redacted only in readonly configuration data; executable bytes and ELF offsets remain unchanged, and the payload/source contracts are regenerated. `tools/verify_open_source.py` verifies license documents and nested checksums. The source archive includes the local GPL kernel changes and FFmpeg patch/source obligations; external BSP/link inputs remain separate. New source-rate combinations, all TVs and complete HDR calibration are not newly certified.

以下保留历史构建步骤和阶段记录。旧版本“候选／未发布”仅指当时状态，不覆盖上方正式版与当前验收记录。

---

# Working-tree Decode Stack 0.7.9 / LSPosed 0.3.6 candidates

HWUI mapping and selective HWC mode-cache repair are integrated into Decode
Stack; full HDR remains gated inactive. Rebuild using `tools/build_hdr.py` and
the existing Decode Stack combiner with private hash-pinned firmware link inputs.
Only source-built adapters, guards and recipes belong in an installer, never
private full HWUI/HWC copies. See [current evidence](docs/decode-hwui-rgb10-079.zh-CN.md).
The optional LSPosed FFmpeg client uses public revision `bf4d5037b4`, its local
patch and static dav1d 1.5.3; [staging/build instructions](lsposed/patches/README.md)
describe its contract, accompanying source/licenses and pending player acceptance.
These are unreleased worktree candidates; older release links below do not
contain these changes.

# Working-tree Mali Global 0.4.2-test1

Selective Rockchip CSF fence metadata initialization backport (`194a7d5a7567395d37e11ffa9014589b83ba8aad`), independently selected at first binding. Full frozen source, original upstream patch, read-only KMD parameter, private boot builder and module WebUI are included. Rebuild with the same Linux 5.10.66 headers / pinned CRC inputs using the existing commands below; prepare a new private boot for the rebuilt KMD and keeper hashes. Installer excludes proprietary Mali libraries, CSF, private boot and device data. See [scope and validation](docs/mali-csf-fence-042.zh-CN.md).

---

# Working-tree Decode Stack 0.7.4-test1

This local experimental snapshot selectively backports paired Rockchip output-buffer configuration notifications while retaining existing buffer counts and pixel/fence paths. See [scope and bounded device evidence](docs/decode-output-delay-074.zh-CN.md). Original tested installer bytes are preserved; private diagnostics and signing inputs are excluded.

---

# Working-tree Decode Stack 0.7.3-test1

This local experimental snapshot adds the independently selectable Gralloc HDR transfer ownership optimization. See [scope and device validation](docs/decode-gralloc-transfer-073.zh-CN.md); original API32/firmware contracts and reboot rollback remain required. It does not replace Gralloc or claim a newer vendor SDK.

---

# Working-tree Mali Global 0.4.1-test2 repair

This unpublished snapshot includes the Zygisk startup repair, its source pins and bounded on-device rollback/injection results. The tested installer bytes are preserved; see [repair validation](docs/mali-zygisk-repair-041.zh-CN.md). Private boot/profile inputs are excluded. Older release instructions below remain historical.

---

# v0.6.9 optional 应用内 PanVK driver / 可选 应用内驱动

Checkout `v0.6.9` for `panvk/` and `tools/build_panvk.py`. The new driver is
RC3 Test8.1, a documentation/metadata packaging revision of tested Test8.
Its complete original adapter source, strict address-width binary patcher,
one-line Mesa source fix and compiled-engine offline tests are included.
It is **not a complete Mesa rebuild**. Public upstream pins, toolchain,
requirements and exact reproduction commands: [panvk/README.md](panvk/README.md).

四个原安装包沿用原字节和原源码标签：Mali 0.3.0 对应 v0.6.8，其余三包对应 v0.6.7。
重建历史安装包请使用其各自标签，勿把文档变化冒充运行组件升级。新增 PanVK 的运行库
与本地 Test8 一致，构建器校验哈希；无原版应用 APK、私人固件输入、游戏配置或日志。

---

# v0.6.8 Mali kernel / CSF integration

Mali Global is now 0.3.0. Decode Stack 0.6.7, plugin 0.3.5 and Frame Guard 0.1.1
are carried with unchanged bytes and v0.6.7 source metadata. Use v0.6.8 for the new
Mali sources; the older sections below document the unchanged component inputs.
本次新增 Mali 内核／CSF 与中英文 WebUI；其余三包沿用原版本、字节和源码标签。

For this new Mali build, checkout the exact tag first: `git checkout --detach v0.6.8`.

## Rebuilding the GPL kernel option

`mali-kernel/` contains the complete frozen driver subtree and compatibility changes,
all helper source, exact object list, linker export namespace CRCs, image-derived ABI
contract, matching configuration and generated headers. Original kernel/boot images
are not inputs to this release build. Obtain pristine Linux 5.10.66 source headers
from kernel.org and build its host `scripts/mod/modpost` (GNU host build tools/MSYS2
on Windows; normal host compiler on Linux). The frozen configuration/generated
headers are used by the standalone builder. Do not configure against arbitrary
installed distribution headers or replace the pinned import versions.

```sh
python tools/build_mali_kernel.py --ndk /inputs/ndk --kernel-source /inputs/linux-5.10.66 --modpost /inputs/kernel-host/scripts/mod/modpost --work /work/kernel
python tools/build_mali_kernel_runtime.py --ndk /inputs/ndk --work /work/kernel-runtime
```

On Windows `--host-tools /inputs/msys/usr/bin` supplies modpost's host runtime.
The build checks 394 KMD and 34 handover-helper import versions against the original
ABI contract and maps compiler paths. The module loader additionally verifies the
live original code/layout before installing any handover callbacks. Build success
alone does not establish compatibility with a different firmware.

Then add `--kernel-artifacts /work/kernel --kernel-runtime /work/kernel-runtime`
to the existing `tools/build_mali_global.py` command below. Both native helpers and
GPL modules are compiled from the published files. Proprietary userspace libraries
and CSF are downloaded at installation using the pinned URL/size/SHA256 contracts.
Do not copy those blobs into source archives or public installer ZIPs.

原内核版本仍为 5.10.66；新版可加载模块在内存接管 GPU，不编译或刷写 boot。构建需要
上述 Linux 头、NDK 和主机 modpost 工具，不需要私人内核镜像。运行前仍严格校验当前固件。

---

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

Decode Stack 0.7.6-test1 applies `hevc-native-surface.patch` after the output
delay patch in the fixed next Codec2 core. This local API32 guard correction
retains upstream native HEVC composer output across late ten-bit discovery;
it does not force FBC changes or replace the SDK baseline. The independent
default-off `hevc_native_surface` option requires `codec2_sdk_next` and
`hdr_static`. New acceptance and rollback records are in
`docs/decode-hevc-native-076.zh-CN.md` and its validation JSON. Optional test
sources/builders are in `tools/decode-performance/` and `tools/hevc-native-test/`;
private media, raw output, keys and test APKs are excluded from delivery.

Decode Stack 0.7.5-test1 additionally builds `magisk-hdr/src/plane_probe.c`.
`overlay-scale.sh` generates a private factory-HWC variant at install only for
the exact original hash, implementing the two removed scaling restrictions in
Rockchip commit e3e3d168e524da25620ef55da734e48c8e56afe6. Four bytes differ;
the original companion is retained. This is not a full latest-HWC rebuild or
VOP2 kernel update. Preserve the patch and `LICENSES/hwc/` notices. Never copy
the generated `components/hdr/variants/` or factory companion into a public
source/installer. The tested installer remains immutable; the device validation
and legal addendum accompany it separately. See
`docs/decode-hwc-overlay-075.zh-CN.md` and its structured validation JSON.

MIT applies to original project code. Preserve upstream notices and the
licenses/patent grants in LICENSES and component directories. Upstream source
links do not grant permission to redistribute proprietary Mali or firmware.
The default-off cold-binding candidate adds a statically built native keeper and private exact-root boot preparation (`tools/cold-boot/prepare.py`). Selecting that backend writes boot; private images and root profiles are never part of the source or installer. See `magisk-mali-global/boot/README.md`. The legacy backend does not flash boot. No Dolby or player changes are introduced.


Decode Stack 0.7.7-test1 selectively backports the remaining HEVC EOS pending-input completion from Lineage-Rockchip e577f69d04d7435a8e4a1088337ff2ed92a3a7e1. The API32 adaptation batches callbacks outside internal locks and keeps a default-off reboot-latched independent option. Scope/results: docs/decode-hevc-eos-077.zh-CN.md. Rebuildable diagnostic sources/builders: tools/decode-lifecycle/eos_pending_probe.cpp and build_eos_pending_probe.py. No media or private link inputs are included.


Decode Stack 0.7.8-test1 adds local MIT NV15-to-P010 NEON pair-gather optimization. It is not a direct new upstream backport or a newer complete SDK/library. Canonical source and independent bounds/precision test are in tools/pixel-conversion; the pinned next-core builder verifies the applied new header. Scope, installed CPU results, two-level rollback and explicit-P010 HDMI HDR limitation: docs/decode-p010-pairs-078.zh-CN.md.


Decode Stack 0.8.2-test1 adds the internal MIT source `magisk-hdmi-event/src/` and `tools/build_hdmi_event.py`. Build with `--ndk`, exact private `--runtime` and `--hardware-legacy` inputs, and fresh `--work`; pass its clean package as `--hdmi-event` to `tools/build_decode_stack.py`. Inputs stay private; only source, checked byte recipes and newly built helper/probes are packaged. This is a local compatibility repair, not a newer kernel or Composer driver.


The private Decode Stack 0.8.4 candidate adds native-backpressure.patch on the pinned next-core: actual native Surface prefetch deferral and MPP BUFFER_FULL flow control. AOSP V4L2 b/184020290 and the pinned MPP implementation are mechanism references, not direct backports or buffer-count transplants. Watchdog, fences and genuine work completion remain intact. Test1 failed actual Exo; test2 passed a bounded user trial; test3 tightens copy/tunnel exclusion and has separate acceptance. See docs/decode-native-backpressure-084.zh-CN.md and tools/decode-lifecycle/native_backpressure_probe.cpp. This is not a public release or a complete SDK upgrade.

The in-progress Decode Stack QMS kernel core is GPL-2.0-only, under
`decode-kernel/qms/`, with selective Rockchip d0e8f558 timing data and 5a89a1cb
VTEM/state-machine semantics. Exact scope and two local timing-data corrections
are recorded in its UPSTREAM.json. Builders/tests are in tools/qms/ and
tools/build_display_probe.py. Session/UAPI source now includes separate MIT HWC
frontend and pre-exec rendezvous candidates. Their compile gates are off in
normal builds; no functional hardware binding, kernel endpoint, display loader
or installable QMS upgrade is delivered yet. Private Image/ELF/contracts/logs remain
excluded. GPL source is not covered by the original adapters' default MIT grant.

`decode-kernel/qms/uapi.h` uses GPL-2.0-only WITH Linux-syscall-note. The MIT
frontend includes only this syscall header, not GPL implementation code.
Reproduce the new isolated checks with `tools/qms/test_session.py`,
`tools/build_hdr.py --qms-candidate --qms-frontend-test`, and
`tools/tests/test_qms_cold_stage.py`; read each tool's help for private build
inputs. These checks do not perform real QMS or qualify a boot image. The Decode
builder refuses the frontend-only component until a functional backend exists.

</details>
