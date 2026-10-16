# Third-party notices / 第三方许可

This inventory supplements, and does not replace, original file-level notices.
The project MIT license covers original contributions only. Components
are not all governed by one license. Source links and revisions are in
SOURCE_CODE.md and component UPSTREAM.json/SOURCE.json files.
本表补充上游声明，不替代原许可；未列为随包分发的软件不受本项目重新授权。

| Material | Packages carrying it | Applicable notices / license copies |
| --- | --- | --- |
| Rockchip MPP 1.1.0 and legacy VPU wrapper, local patches | Decode Stack | Apache-2.0 and MIT on respective files; `LICENSES/mpp/` and component copies |
| Rockchip librga 1.10.6 [3] | Decode Stack | Apache-2.0, `LICENSES/rga/Apache-2.0`; original COPYING retained |
| Rockchip Codec2 API32, SDK 1.14.3 and SDK 1.14.16 | Decode Stack | Apache-2.0, `LICENSES/codec2*/Apache-2.0`; original Rockchip notices retained |
| AOSP Codec2 wrappers, Android platform interfaces and derived service code | Decode Stack | Apache-2.0; original notices in source/patches and `LICENSES/codec2/Apache-2.0` |
| libyuv 1974 and isolated Android 12 compatibility objects | Decode Stack | BSD-style license for each source tree and PATENTS in `LICENSES/libyuv/` |
| libvpx 1.17.0 decoder code | Decode Stack | BSD-style license and PATENTS in `LICENSES/software/libvpx-*` |
| libgav1 AV1 decoder | Decode Stack | Apache-2.0 with AUTHORS in `LICENSES/software/libgav1-*` |
| LLVM/libc++ portions and Android NDK headers/runtime | Decode Stack | License depends on source/runtime version; preserve `LICENSES/rga/LLVM-libcxx.txt` and complete NDK notices in `LICENSES/ndk/` rather than applying project MIT terms to all portions |
| AOSP common V2, graphics.common V3, allocator V1 frozen AIDL, Binder C++ support and Vulkan HAL header | Mali Global | Apache-2.0; `LICENSES/Apache-2.0.txt`, original file notices in `src/` and NOTICE.txt |
| Original plugin source/resources | LSPosed plugin | Project MIT; no Xposed/LSPosed framework classes or player libraries bundled |
| Original refresh policy/installer scripts | Frame Guard | Project MIT; no separately compiled third-party payload |

Copyright holders include Rockchip Electronics Co. LTD, The Android Open
Source Project, The LibYuv Project Authors, The WebM Project authors, Google
LLC, LLVM contributors and the other holders identified in the original files.
Complete license copies retain their original wording. Upstream notices must
remain when redistributing; modified source files/patches identify local work.

## Build dependencies and external applications / 构建依赖与外部软件

- Gradle wrapper 8.11.1: Apache-2.0, license also embedded in
  `gradle/wrapper/gradle-wrapper.jar` as `META-INF/LICENSE`. Android Gradle Plugin
  8.7.3 and Android SDK/NDK are build tools, not shipped as installed tools.
- Xposed API 82 is a **compileOnly** dependency (provided at runtime by the
  user's framework); JUnit 4.13.2 is test-only. Their artifacts are not packaged
  in the plugin APK. Consult their own upstream licenses before distributing
  those dependencies separately.
- Magisk, KernelSU, KsuWebUI, LSPosed, Yamby, mpv and FFmpeg are independent
  projects. Calling their APIs does not distribute or relicense their code.
  No Yamby APK or bundled player library is attached to this release.
- Original firmware libraries are private ABI/link inputs or patched locally
  on the device after exact-hash checks. They are not included in the source
  archive or installers; the project does not grant rights to redistribute them.
- Proprietary Mali g18p0 binaries come from a pinned Khadas repository at
  installation (`magisk-mali-global/contracts/driver-sources.txt`). Public hosting
  is not proof of an open-source license. They remain separately licensed;
  the project ZIP contains neither those binaries nor a license grant for them.

For component upgrades, retain the license/patent/author files from the exact
new source revision, update this table and source provenance, then regenerate
package checksums. Do not replace third-party copyright notices with the
project author's name. / 升级组件时同步更新许可、专利、作者、来源及校验值。


## Mali kernel option / Mali 内核选项 · v0.6.8

Mali Global 0.3.0 additionally distributes GPL source-built g18p0 kernel and
resource-handover modules. Their corresponding frozen source, API compatibility
headers, generated kernel configuration, guards and build scripts are in
`mali-kernel/`. The driver baseline is rockchip-linux/kernel commit
`7a76350c1e0df810d8c11eb0d18285da4e79e6ed`; original file-level GPL terms remain.
`LICENSES/GPL-2.0-only.txt` and `LICENSES/Linux-syscall-note.txt` accompany the ZIP.
The installer, offline bilingual WebUI and userspace controller are project MIT.
Closed userspace libraries and CSF firmware are separately downloaded, never
relicensed or redistributed here. The boot image and original firmware are not bundled.
内核组件保留 GPL 许可，自写安装器、WebUI 和用户态控制程序采用 MIT；不将第三方
内核代码重新授权为 MIT。公开包不含闭源 Mali 库或 CSF 固件。

Unchanged installers in the complete v0.6.8 snapshot retain their v0.6.7 source
metadata and exact bytes. Their historical licenses/build notices remain inside.
