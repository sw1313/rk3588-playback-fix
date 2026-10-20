# Open-source information / 开源说明

Original RK3588 Playback Fix code and documentation are licensed under
**MIT**. See [LICENSE](LICENSE) and [NOTICE](NOTICE). Existing third-party
licenses and copyright notices take precedence for their respective files.
本项目自写代码与文档采用 MIT；上游代码保留原许可、版权及专利声明。
允许商业使用；修改和再分发须遵守对应许可。本说明不重新授权第三方闭源库。

| Installer / 安装包 | Open-source material / 开源内容 | External material / 外部组件 |
| --- | --- | --- |
| Decode Stack | Scripts, offline WebUI, compatibility adapters, source-built MPP/RGA/Codec2/libyuv/software backends and probes | Original firmware ABI/link inputs and system services are not bundled |
| Mali Global | Installer/downloader, WebUI, startup adapter, GPL kernel source/modules, probes and AOSP interface libraries | Proprietary Mali driver and CSF firmware are fetched separately from pinned upstream URLs |
| Yamby/mpv LSPosed plugin | Plugin Java/resources and settings | Xposed API is compile-only; LSPosed, Yamby and mpv are not bundled |
| PanVK (optional) | In-app driver adapter, Mesa/kbase core, patches and build/provenance records; upstream terms retained | Application-specific driver import, not a global system module |

The Decode Stack and Mali Global ZIPs carry LICENSE, NOTICE, this file,
THIRD_PARTY_NOTICES.md, SOURCE_CODE.md, SOURCE_CODE.zh-CN.md and OPEN_SOURCE.json
at ZIP root. The optional PanVK ZIP also carries its source, provenance and license notices.
The APK carries the project notices under `assets/open-source/`. Applicable
license texts accompany the material, with Decode Stack also retaining the
original per-component `components/*/licenses/` directories.
当前完整正式版为 v0.8.8：Decode 0.8.6、Mali 0.4.4、客户端 0.3.7，以及独立可选的 PanVK Test8.1。系统 ZIP 与 APK 均携带对应许可、构建说明和来源元数据；原字节沿用的组件保留各自原始标签，不将重新携带标成重新构建。

Frame Guard is historical and is not bundled with current releases. Its published
source and notices remain available for rollback reference; it is not part of the
current recommended installation. / Frame Guard 仅保留历史源码及已发布附件供回退核对。

Use the tag in OPEN_SOURCE.json for the exact project snapshot. The release
also supplies a project source archive and SHA256SUMS.txt; the source archive
contains this repository, not a complete Android SDK or proprietary firmware.
Pinned public upstream sources and required private link inputs are described
in [SOURCE_CODE.md](SOURCE_CODE.md) / [中文构建说明](SOURCE_CODE.zh-CN.md).

Public releases exclude personal paths, device addresses/identifiers, private
logs, accounts, keys, test media and original firmware libraries. Local builds
must keep those inputs outside the source/package tree and use path mapping.
公开材料不含私人路径、设备地址和标识、原始日志、账号、密钥、测试影片或原厂固件库。

MIT applies to original project contributions, including newly added helper files.
Upstream-derived files keep their original license; changes to those files do
not remove upstream rights or notices. Earlier published Apache-2.0 grants
remain valid for the versions that carried them. / 自写部分采用 MIT，上游及历史已授予许可不撤回。


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


PanVK optional in-app Vulkan driver / 可选 应用内驱动 · v0.6.9

PanVK RC3 Test8.1 additionally carries the open Mesa/kbase driver core and the
original API32 loader, mapper/fence bridge and single-CSG adapter. Original
project adapters/build tools are MIT. Upstream Mesa/Khronos notices, the
zenithblue-oss patch stack, AOSP Apache-2.0 headers, kbase UAPI GPL-2.0 WITH
Linux-syscall-note and NDK/LLVM runtime notices retain their original terms.
The UAPI exception does not relicense the headers. This open PanVK component
is distinct from proprietary Mali userspace/CSF blobs, which remain excluded.
See panvk/LICENSES/, panvk/PROVENANCE.txt and panvk/README.md for exact pins,
source, binary-counterpart scope, build and validation limits. No application code,
game material, user configuration, keys, raw logs or device identifiers ship.
Test8.1 runtime matches local Test8; documentation/metadata only changed.
Unchanged installers carry the original v0.6.7 or v0.6.8 source/license metadata
as identified by release-manifest.json; they are not silently repackaged.
