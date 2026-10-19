# Downloads / 下载 · v0.8.6

[完整正式更新 / Complete release](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.8.6)

| Component / 组件 | Version / 版本 | Download / 下载 |
| --- | --- | --- |
| Decode Stack | 0.8.6 | [rk3588-decode-stack-0.8.6.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.6/rk3588-decode-stack-0.8.6.zip) |
| Mali Global | 0.4.2 | [rk3588-mali-global-0.4.2.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.6/rk3588-mali-global-0.4.2.zip) |
| Yamby/mpv LSPosed | 0.3.7 | [RK3588-Mpv-Fix-0.3.7.apk](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.6/RK3588-Mpv-Fix-0.3.7.apk) |
| Frame Guard | 0.1.1 | [rk3588-frameguard-0.1.1.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.6/rk3588-frameguard-0.1.1.zip) |
| PanVK (optional / 可选) | RC3 Test8.1 | [rk3588-panvk-rc3-test8.1.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.6/rk3588-panvk-rc3-test8.1.zip) |

Frame Guard 与 PanVK 原字节携带，分别保留原发布来源。Frame Guard 不是本次短黑修复，Auto 内容匹配时保持关闭。PanVK 仍为可选的应用内实验驱动，不是全局替换。

System modules require the exact firmware contracts. Existing choices survive updates; turn a feature off and reboot for rollback. QMS / first binding require independently prepared private boot compatibility; no firmware or boot image is distributed.

系统模块严格匹配固件；安装前会检查依赖，保留现有分项。关闭分项并重启回退。全套源代码不含私有固件、设备地址、原始日志和签名密钥。

[SHA256](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.6/SHA256SUMS.txt) · [Manifest](release-manifest.json) · [Source ZIP](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.6/rk3588-playback-fix-0.8.6-source.zip) · [验收](docs/validation/decode-refresh-session-086.json)
