# Downloads / 下载 · v0.8.6

[完整正式更新 / Complete release](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.8.6)

当前推荐组件 / Current components：Decode Stack、Mali Global，以及按需安装的 LSPosed 客户端。PanVK 独立可选。Frame Guard 已退出推荐组合，当前已验收配置不再需要它。

| Component / 组件 | Version / 版本 | Download / 下载 |
| --- | --- | --- |
| Decode Stack | 0.8.6 | [rk3588-decode-stack-0.8.6.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.6/rk3588-decode-stack-0.8.6.zip) |
| Mali Global | 0.4.2 | [rk3588-mali-global-0.4.2.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.6/rk3588-mali-global-0.4.2.zip) |
| Yamby/mpv LSPosed | 0.3.7 | [RK3588-Mpv-Fix-0.3.7.apk](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.6/RK3588-Mpv-Fix-0.3.7.apk) |
| PanVK (optional / 可选) | RC3 Test8.1 | [rk3588-panvk-rc3-test8.1.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.6/rk3588-panvk-rc3-test8.1.zip) |

PanVK 原字节携带，仍为可选的应用内实验驱动，不是全局替换。

System modules require the exact firmware contracts. Existing choices survive updates; turn a feature off and reboot for rollback. QMS / first binding require independently prepared private boot compatibility; no firmware or boot image is distributed.

系统模块严格匹配固件；安装前会检查依赖，保留现有分项。关闭分项并重启回退。全套源代码不含私有固件、设备地址、原始日志和签名密钥。

[SHA256](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.6/SHA256SUMS.txt) · [Manifest](release-manifest.json) · [Source ZIP](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.6/rk3588-playback-fix-0.8.6-source.zip) · [验收](docs/validation/decode-refresh-session-086.json)

## 历史回退工具 / Legacy rollback reference

Frame Guard 0.1.1 仅保留供旧版本回退核对，[历史文件](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.6/rk3588-frameguard-0.1.1.zip) 不属于当前推荐安装组合。它固定 60Hz，会阻止 Auto／QMS 内容匹配。移除旧安装时先恢复其保存的系统刷新率值；只禁用模块不会自动清除限制。详见 [回退说明](README.zh-CN.md#关闭回退与故障处理)。

Frame Guard is retired from the recommended installation. Existing published assets remain intact for historical reference. Restore its saved system refresh restrictions before removing an old installation; simply disabling it is insufficient. The accepted current configuration does not require it.
