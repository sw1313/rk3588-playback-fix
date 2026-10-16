# 完整项目下载 / Complete project downloads

**[v0.6.3 — RK3588 Playback Fix：全部四个组件](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.3)**

每个公开 Release 附齐四个安装包；未更新组件保留原版本与原始字节。GitHub Latest 指向最新整套版本。

| 组件 / Component | 包内版本 | 安装包 / Download |
| --- | --- | --- |
| Decode Stack 解码合并模块 | 0.6.3 | [rk3588-decode-stack-0.6.3.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.3/rk3588-decode-stack-0.6.3.zip) |
| Native Mali Global 全局图形驱动 | 0.2.0 | [rk3588-mali-global-0.2.0.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.3/rk3588-mali-global-0.2.0.zip) |
| Yamby/mpv LSPosed 插件 | 0.3.3 | [RK3588-Mpv-Fix-0.3.3.apk](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.3/RK3588-Mpv-Fix-0.3.3.apk) |
| Frame Guard 刷新率模块 | 0.1.0 | [rk3588-frameguard-0.1.0.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.3/rk3588-frameguard-0.1.0.zip) |

[SHA256 校验值](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.3/SHA256SUMS.txt) · [组件清单](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.3/release-manifest.json)

本次更新 Decode Stack；无需重装未变化的 Mali、Yamby 插件和 Frame Guard。系统模块仍为实验性质，只接受匹配的固件哈希。旧独立解码模块保持停用。

AV1 收益限于报告中的片源和原生显示路径；非 64 对齐宽度、Texture/CPU 等保留原策略。没有改 Yamby、LSPosed、boot 或内核，也不降低画质。完整剪辑提前 EOS 和图像特效退出问题仍未修复。

[本次验证与限制](docs/decode-av1-063.zh-CN.md) · [开关与回退](magisk-decode/README.md)

## 整套版本历史 / Complete release history

| 项目版本 | 首次发布时间（Asia/Shanghai） | 解码 | Mali | Yamby 插件 | Frame Guard |
| --- | --- | --- | --- | --- | --- |
| [v0.6.3](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.3) | 2026-10-16 | 0.6.3 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.5.1](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.5.1) | 2026-10-16 | 0.5.1 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.5.0](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.5.0) | 2026-10-15 | 0.5.0 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.4.0](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.4.0) | 2026-10-15 | 0.4.0 | 0.2.0 | 0.3.3 | 0.1.0 |
| [v0.3.2](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.3.2-system) | 2026-10-15 | 0.3.2 | 0.2.0 | 0.3.3 | 0.1.0 |

旧独立组件 Release 及其归档草稿已从列表移除；原说明与全部 20 个附件保存在 v0.3.2 整套版本的[历史备份 ZIP](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.3.2-system/historical-standalone-components.zip)，Git Tag 保留。旧组件仅供历史参考与恢复，请勿与 Decode Stack 叠加安装。
