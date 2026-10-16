# Open-source information / 开源说明

Original RK3588 Playback Fix code and documentation are licensed under
**MIT**. See [LICENSE](LICENSE) and [NOTICE](NOTICE). Existing third-party
licenses and copyright notices take precedence for their respective files.
本项目自写代码与文档采用 MIT；上游代码保留原许可、版权及专利声明。
允许商业使用；修改和再分发须遵守对应许可。本说明不重新授权第三方闭源库。

| Installer / 安装包 | Open-source material / 开源内容 | External material / 外部组件 |
| --- | --- | --- |
| Decode Stack | Scripts, offline WebUI, compatibility adapters, source-built MPP/RGA/Codec2/libyuv/software backends and probes | Original firmware ABI/link inputs and system services are not bundled |
| Native Mali Global | Installer/downloader, probes and AOSP interface libraries | Proprietary Mali driver is fetched separately from a pinned upstream URL |
| Yamby/mpv LSPosed plugin | Plugin Java/resources and settings | Xposed API is compile-only; LSPosed, Yamby and mpv are not bundled |
| Frame Guard | Installer, refresh policy, switches and rollback scripts | Uses existing Android commands; no Android or Magisk binaries bundled |

All three Magisk ZIPs carry LICENSE, NOTICE, this file,
THIRD_PARTY_NOTICES.md, SOURCE_CODE.md, SOURCE_CODE.zh-CN.md and OPEN_SOURCE.json
at ZIP root. The APK carries them under `assets/open-source/`. Applicable
license texts accompany the material, with Decode Stack also retaining the
original per-component `components/*/licenses/` directories.
三份 ZIP 根目录、APK 的 `assets/open-source/` 均携带许可与源码构建说明。

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
