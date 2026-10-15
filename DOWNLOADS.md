# 完整项目下载 / Complete project downloads

**[v0.5.1 — RK3588 Playback Fix：全部四个组件](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.5.1)**

每个公开 Release 都附齐四个安装包。项目发布版本标识整套快照，组件包内版本不强行同步。
All four installers are attached to each public project release. GitHub Latest points to the newest complete snapshot.

| 组件 / Component | 包内版本 / Component version | 安装包 / Download |
| --- | --- | --- |
| Decode Stack 解码合并模块 | 0.5.1 | [rk3588-decode-stack-0.5.1.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.5.1/rk3588-decode-stack-0.5.1.zip) |
| Native Mali Global 全局图形驱动 | 0.2.0 | [rk3588-mali-global-0.2.0.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.5.1/rk3588-mali-global-0.2.0.zip) |
| Yamby/mpv LSPosed 插件 | 0.3.3 | [RK3588-Mpv-Fix-0.3.3.apk](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.5.1/RK3588-Mpv-Fix-0.3.3.apk) |
| Frame Guard 刷新率模块 | 0.1.0 | [rk3588-frameguard-0.1.0.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.5.1/rk3588-frameguard-0.1.0.zip) |

[SHA256 校验值](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.5.1/SHA256SUMS.txt) · [组件清单](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.5.1/release-manifest.json)


系统模块仍为实验性质并限制固件哈希。使用解码合并模块时，保持旧独立 MPP／RGA／Codec2／libyuv 模块停用。
完整剪辑提前 EOS 和图像特效退出异常仍未修复，0.5.1 新一轮手动 Yamby 验收待确认。
详见[模块开关与回退](magisk-decode/README.md)、[已知问题](README.zh-CN.md#051-更新与已知限制)。

## 整套版本历史 / Complete release history

| 项目版本 | 首次发布时间（Asia/Shanghai） | 解码 | Mali | Yamby 插件 | Frame Guard |
| --- | --- | --- | --- | --- | --- |
| [v0.5.1](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.5.1) | 2026-10-16 | 0.5.1 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.5.0](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.5.0) | 2026-10-15 | 0.5.0 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.4.0](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.4.0) | 2026-10-15 | 0.4.0 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.3.2](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.3.2-system) | 2026-10-15 | 0.3.2 | 0.2.0 | 0.3.3 | 0.1.0 |

旧独立组件 Release 及其归档草稿已从列表移除；原说明与全部 20 个附件保存在 v0.3.2 整套版本的[历史备份 ZIP](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.3.2-system/historical-standalone-components.zip)，Git Tag 保留。旧组件仅供历史参考与恢复，请勿与 Decode Stack 叠加安装。
