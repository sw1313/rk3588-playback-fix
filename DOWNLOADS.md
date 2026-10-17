# Complete project downloads / 完整项目下载

**[v0.6.9 — RK3588 Playback Fix: four installers + optional in-app Vulkan driver / 四包齐全，另附可选 应用内驱动](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.9)**

All four existing installers keep their previous versions and exact bytes.
This release adds experimental PanVK RC3 Test8.1 for **the tested application's GPU
driver manager**, not Magisk. It is not a system-wide GPU update.
四个原包原字节携带。本次新增 PanVK Test8.1 是 **应用内导入的可选驱动 ZIP**，
不是面具模块。只有公开说明、许可和打包元数据相对本地 Test8 更新，运行库一致。

| Component / 组件 | Version / 包内版本 | Download / 安装包 |
| --- | --- | --- |
| Decode Stack / 解码合并模块 | 0.6.7 | [rk3588-decode-stack-0.6.7.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.9/rk3588-decode-stack-0.6.7.zip) |
| Native Mali Global / 全局图形驱动 | 0.3.0 | [rk3588-mali-global-0.3.0.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.9/rk3588-mali-global-0.3.0.zip) |
| Yamby/mpv LSPosed plugin / 插件 | 0.3.5 | [RK3588-Mpv-Fix-0.3.5.apk](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.9/RK3588-Mpv-Fix-0.3.5.apk) |
| Frame Guard / 刷新率模块 | 0.1.1 | [rk3588-frameguard-0.1.1.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.9/rk3588-frameguard-0.1.1.zip) |
| PanVK for the tested app / 应用内可选实验驱动 | RC3 Test8.1 | [rk3588-panvk-rc3-test8.1.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.9/rk3588-panvk-rc3-test8.1.zip) |

[PanVK install, rollback, scope and build / 安装、回退、范围与构建](panvk/README.md) · [中文验证说明](docs/panvk-test8.zh-CN.md)

PanVK short title/start/touch-overlay retesting improved, but long-run stability
and all-game compatibility are unproven. The reported game used a temporary
per-game JIT CPU comparison setting; this ZIP does not set CPU backend or quality.
Tested with the existing matched g18p0 kernel/CSF pair, not every factory firmware.
Restore System GPU Driver in the tested app after stopping the game to roll back.
实测基于已有配套 g18p0；尚无长期或所有固件／游戏兼容结论，FPS 不是最终性能结果。

[SHA256](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.9/SHA256SUMS.txt) · [Manifest](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.9/release-manifest.json) · [Project source ZIP](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.6.9/rk3588-playback-fix-0.6.9-source.zip) · [Validation](docs/validation/panvk-test8.json)

[Open-source scope / 开源范围](OPEN_SOURCE.md) · [Third-party notices / 第三方许可](THIRD_PARTY_NOTICES.md) · [Build guide](SOURCE_CODE.md) · [中文构建](SOURCE_CODE.zh-CN.md)

The source archive contains project source and patches, not an entire BSP or
private firmware inputs. Closed Mali/firmware blobs and raw device logs are
excluded. PanVK is the separately licensed open driver described above.

## 整套版本历史 / Complete release history

| 项目版本 | 首次发布时间（Asia/Shanghai） | 解码 | Mali | Yamby 插件 | Frame Guard |
| --- | --- | --- | --- | --- | --- |
| [v0.6.9](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.9) | 2026-10-17 | 0.6.7 | 0.3.0 | 0.3.5 | 0.1.1 |
| [v0.6.8](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.8) | 2026-10-17 | 0.6.7 | 0.3.0 | 0.3.5 | 0.1.1 |
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
