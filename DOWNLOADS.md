# Complete project downloads / 完整项目下载

**[v0.6.6 — RK3588 Playback Fix: all four components / 全部四个组件](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.6)**

Every public release includes all four installers. Unchanged components retain their original versions and bytes. Latest identifies the newest complete suite.
每个公开 Release 附齐四个安装包；未更新组件保持原版本与原字节。

| Component / 组件 | Version / 包内版本 | Download / 安装包 |
| --- | --- | --- |
| Decode Stack / 解码合并模块 | 0.6.6 | [rk3588-decode-stack-0.6.6.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.6/rk3588-decode-stack-0.6.6.zip) |
| Native Mali Global / 全局图形驱动 | 0.2.0 | [rk3588-mali-global-0.2.0.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.6/rk3588-mali-global-0.2.0.zip) |
| Yamby/mpv LSPosed plugin / 插件 | 0.3.4 | [RK3588-Mpv-Fix-0.3.4.apk](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.6/RK3588-Mpv-Fix-0.3.4.apk) |
| Frame Guard / 刷新率模块 | 0.1.0 | [rk3588-frameguard-0.1.0.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.6/rk3588-frameguard-0.1.0.zip) |

[SHA256](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.6/SHA256SUMS.txt) · [Manifest / 组件清单](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.6/release-manifest.json)

This update adds language controls to Decode Stack and LSPosed settings. Mali and Frame Guard already have English messages and are unchanged. No decoder or hook behavior changes.
本次补齐四组件英文覆盖，两个设置界面新增语言按钮；不修改解码或钩子行为。

[Languages / 语言入口](docs/localization-066.md) · [Decoder limits / 解码边界](docs/decode-av1-063.zh-CN.md) · [Rollback / 回退](magisk-decode/README.md)

## 整套版本历史 / Complete release history

| 项目版本 | 首次发布时间（Asia/Shanghai） | 解码 | Mali | Yamby 插件 | Frame Guard |
| --- | --- | --- | --- | --- | --- |
| [v0.6.6](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.6) | 2026-10-16 | 0.6.6 | 0.2.0 | 0.3.4 | 0.1.0 |
| [v0.6.5](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.5) | 2026-10-16 | 0.6.5 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.6.4](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.4) | 2026-10-16 | 0.6.4 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.6.3](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.3) | 2026-10-16 | 0.6.3 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.5.1](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.5.1) | 2026-10-16 | 0.5.1 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.5.0](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.5.0) | 2026-10-15 | 0.5.0 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.4.0](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.4.0) | 2026-10-15 | 0.4.0 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.3.2](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.3.2-system) | 2026-10-15 | 0.3.2 | 0.2.0 | 0.3.3 | 0.1.0 |

旧独立组件 Release 及其归档草稿已从列表移除；原说明与全部 20 个附件保存在 v0.3.2 整套版本的[历史备份 ZIP](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.3.2-system/historical-standalone-components.zip)，Git Tag 保留。旧组件仅供历史参考与恢复，请勿与 Decode Stack 叠加安装。
