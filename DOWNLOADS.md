# Downloads / 下载 · v0.8.8

[完整正式更新 / Complete release](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.8.8)

当前推荐组件 / Current components：Decode Stack、Mali Global，以及按需安装的 LSPosed 客户端。PanVK 独立可选。Frame Guard 已退出推荐组合，当前已验收配置不再需要它。

| Component / 组件 | Version / 版本 | Download / 下载 |
| --- | --- | --- |
| Decode Stack | 0.8.6 | [rk3588-decode-stack-0.8.6.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.8/rk3588-decode-stack-0.8.6.zip) |
| Mali Global | 0.4.4 | [rk3588-mali-global-0.4.4.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.8/rk3588-mali-global-0.4.4.zip) |
| Yamby/mpv LSPosed | 0.3.7 | [RK3588-Mpv-Fix-0.3.7.apk](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.8/RK3588-Mpv-Fix-0.3.7.apk) |
| PanVK (optional / 可选) | RC3 Test8.1 | [rk3588-panvk-rc3-test8.1.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.8/rk3588-panvk-rc3-test8.1.zip) |

PanVK 原字节携带，仍为可选的应用内实验驱动，不是全局替换。

System modules require the exact firmware contracts. Existing choices survive updates; turn a feature off and reboot for rollback. QMS / first binding require independently prepared private boot compatibility; no firmware or boot image is distributed.

系统模块严格匹配固件；安装前会检查依赖，保留现有分项。关闭分项并重启回退。全套源代码不含私有固件、设备地址、原始日志和签名密钥。

[SHA256](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.8/SHA256SUMS.txt) · [Manifest](release-manifest.json) · [Source ZIP](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.8/rk3588-playback-fix-0.8.8-source.zip) · [本版启动验收](docs/validation/release-0.8.8.json) · [播放验收](docs/validation/decode-refresh-session-086.json)

## 本次更新与兼容 / Update and compatibility

本次 Mali 0.4.4 改用实际 root 接口与本机布局检查，不再使用版本或官方 root 二进制哈希白名单。当前 Magisk 30.7 已实机安装重启，其他变体仅离线回归；KernelSU／APatch 专用首次绑定后端尚未实现。详见 [root 兼容边界](docs/root-compatibility.zh-CN.md)。

本次更新 Mali 0.4.4；Decode 0.8.6、客户端 0.3.7 与 PanVK 均原字节携带并保留原来源。Frame Guard 不随本版发布。Mali 新首次绑定仍需本机私有适配；不是任意 RK3588 固件通用驱动。

已使用 v0.8.7 的用户只需覆盖更新 Mali 并重启；Decode 0.8.6 与客户端 0.3.7 无需重复安装。更新保留现有选择，已匹配的私有 boot 不因版本更名要求重新写入。

## 历史回退工具 / Legacy rollback reference

Frame Guard 0.1.1 仅保留供旧版本回退核对，[历史文件](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.6/rk3588-frameguard-0.1.1.zip) 不属于当前推荐安装组合。它固定 60Hz，会阻止 Auto／QMS 内容匹配。移除旧安装时先恢复其保存的系统刷新率值；只禁用模块不会自动清除限制。详见 [回退说明](README.zh-CN.md#关闭回退与故障处理)。

Frame Guard is retired from the recommended installation. Existing published assets remain intact for historical reference. Restore its saved system refresh restrictions before removing an old installation; simply disabling it is insufficient. The accepted current configuration does not require it.
