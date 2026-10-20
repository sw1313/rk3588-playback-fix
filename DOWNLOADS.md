# Downloads / 下载 · v0.8.7

[完整正式更新 / Complete release](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.8.7)

当前推荐组件 / Current components：Decode Stack、Mali Global，以及按需安装的 LSPosed 客户端。PanVK 独立可选。Frame Guard 已退出推荐组合，当前已验收配置不再需要它。

| Component / 组件 | Version / 版本 | Download / 下载 |
| --- | --- | --- |
| Decode Stack | 0.8.6 | [rk3588-decode-stack-0.8.6.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.7/rk3588-decode-stack-0.8.6.zip) |
| Mali Global | 0.4.3 | [rk3588-mali-global-0.4.3.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.7/rk3588-mali-global-0.4.3.zip) |
| Yamby/mpv LSPosed | 0.3.7 | [RK3588-Mpv-Fix-0.3.7.apk](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.7/RK3588-Mpv-Fix-0.3.7.apk) |
| PanVK (optional / 可选) | RC3 Test8.1 | [rk3588-panvk-rc3-test8.1.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.7/rk3588-panvk-rc3-test8.1.zip) |

PanVK 原字节携带，仍为可选的应用内实验驱动，不是全局替换。

System modules require the exact firmware contracts. Existing choices survive updates; turn a feature off and reboot for rollback. QMS / first binding require independently prepared private boot compatibility; no firmware or boot image is distributed.

系统模块严格匹配固件；安装前会检查依赖，保留现有分项。关闭分项并重启回退。全套源代码不含私有固件、设备地址、原始日志和签名密钥。

[SHA256](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.7/SHA256SUMS.txt) · [Manifest](release-manifest.json) · [Source ZIP](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.7/rk3588-playback-fix-0.8.7-source.zip) · [本版启动验收](docs/validation/mali-boot-compat-043.json) · [播放验收](docs/validation/decode-refresh-session-086.json)

## 兼容候选更新 / Compatibility candidate

[Mali Global 0.4.4-compat2 发布说明](https://github.com/sw1313/rk3588-playback-fix/releases/tag/mali-v0.4.4-compat2) · [安装包](https://github.com/sw1313/rk3588-playback-fix/releases/download/mali-v0.4.4-compat2/rk3588-mali-global-0.4.4-compat2.zip) · [源码](https://github.com/sw1313/rk3588-playback-fix/releases/download/mali-v0.4.4-compat2/rk3588-mali-global-0.4.4-compat2-source.zip) · [SHA256](https://github.com/sw1313/rk3588-playback-fix/releases/download/mali-v0.4.4-compat2/rk3588-mali-global-0.4.4-compat2-SHA256SUMS.txt)

本候选按实际接口和本机启动布局判断 root 兼容性，取消版本号及官方 root 二进制哈希白名单，保留固件／ABI 和备份保护。当前 Magisk 30.7 已实机安装重启验收；其他版本及变体仅完成离线回归。KernelSU／APatch 的专用首次绑定后端尚未实现。仅需更新 Mali，其他组件不变；正式推荐版仍为上方 v0.8.7。详见[验证范围](docs/validation/root-interface-compat-044.json)。

This candidate qualifies root compatibility by interfaces and local boot layout, while retaining firmware/ABI and backup guards. The existing Magisk 30.7 target passed physical installation and reboot; other variants have offline coverage only. KernelSU/APatch first-binding adapters are not implemented. Stable v0.8.7 remains the recommended release.

## 历史回退工具 / Legacy rollback reference

Frame Guard 0.1.1 仅保留供旧版本回退核对，[历史文件](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.8.6/rk3588-frameguard-0.1.1.zip) 不属于当前推荐安装组合。它固定 60Hz，会阻止 Auto／QMS 内容匹配。移除旧安装时先恢复其保存的系统刷新率值；只禁用模块不会自动清除限制。详见 [回退说明](README.zh-CN.md#关闭回退与故障处理)。

Frame Guard is retired from the recommended installation. Existing published assets remain intact for historical reference. Restore its saved system refresh restrictions before removing an old installation; simply disabling it is insufficient. The accepted current configuration does not require it.

本次更新 Mali 0.4.3；Decode 0.8.6、客户端 0.3.7 与 PanVK 均原字节携带并保留原来源。Frame Guard 不随本版发布。Mali 新首次绑定仍需本机私有适配；不是任意 RK3588 固件通用驱动。
