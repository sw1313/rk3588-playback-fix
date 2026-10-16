# Complete project downloads / 完整项目下载

**[v0.6.7 — RK3588 Playback Fix: all four components / 全部四个组件](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.7)**

Every release carries all four installers. This documentation snapshot adds license and source/build notices to every package; runtime behavior is unchanged.
每个版本附齐四包；本次补开源及构建说明，运行行为不变。

| Component / 组件 | Version / 包内版本 | Download / 安装包 |
| --- | --- | --- |
| Decode Stack / 解码合并模块 | 0.6.7 | [rk3588-decode-stack-0.6.7.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.7/rk3588-decode-stack-0.6.7.zip) |
| Native Mali Global / 全局图形驱动 | 0.2.1 | [rk3588-mali-global-0.2.1.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.7/rk3588-mali-global-0.2.1.zip) |
| Yamby/mpv LSPosed plugin / 插件 | 0.3.5 | [RK3588-Mpv-Fix-0.3.5.apk](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.7/RK3588-Mpv-Fix-0.3.5.apk) |
| Frame Guard / 刷新率模块 | 0.1.1 | [rk3588-frameguard-0.1.1.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.7/rk3588-frameguard-0.1.1.zip) |

[SHA256](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.7/SHA256SUMS.txt) · [Manifest](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.7/release-manifest.json) · [Project source ZIP](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.7/rk3588-playback-fix-0.6.7-source.zip)

[Open-source scope / 开源范围](OPEN_SOURCE.md) · [Third-party notices / 第三方许可](THIRD_PARTY_NOTICES.md) · [Build guide](SOURCE_CODE.md) · [中文构建](SOURCE_CODE.zh-CN.md)

Source ZIP contains project source and patches. Pinned upstream sources and private firmware link inputs are separate prerequisites; see build guide. Closed Mali/firmware binaries are not bundled.
源码 ZIP 不是完整 BSP；上游源码与私有固件链接输入需另备，详见构建说明。闭源驱动及原厂固件库不分发。

## 整套版本历史 / Complete release history

| 项目版本 | 首次发布时间（Asia/Shanghai） | 解码 | Mali | Yamby 插件 | Frame Guard |
| --- | --- | --- | --- | --- | --- |
| [v0.6.7](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.7) | 2026-10-16 | 0.6.7 | 0.2.1 | 0.3.5 | 0.1.1 |
| [v0.6.6](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.6) | 2026-10-16 | 0.6.6 | 0.2.0 | 0.3.4 | 0.1.0 |
| [v0.6.5](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.5) | 2026-10-16 | 0.6.5 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.6.4](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.4) | 2026-10-16 | 0.6.4 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.6.3](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.3) | 2026-10-16 | 0.6.3 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.5.1](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.5.1) | 2026-10-16 | 0.5.1 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.5.0](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.5.0) | 2026-10-15 | 0.5.0 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.4.0](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.4.0) | 2026-10-15 | 0.4.0 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.3.2](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.3.2-system) | 2026-10-15 | 0.3.2 | 0.2.0 | 0.3.3 | 0.1.0 |

旧独立组件 Release 及其归档草稿已从列表移除；原说明与全部 20 个附件保存在 v0.3.2 整套版本的[历史备份 ZIP](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.3.2-system/historical-standalone-components.zip)，Git Tag 保留。旧组件仅供历史参考与恢复，请勿与 Decode Stack 叠加安装。
