# v0.8.7 — Mali 启动兼容检查与回退验收

本版更新 Mali Global 0.4.3。Decode Stack 0.8.6、LSPosed 客户端 0.3.7、可选 PanVK Test8.1 保留原包、原哈希与原来源。Frame Guard 不再随新版本提供，历史文件仍在旧发布页。

解决“同一固件、不同 root 打包就被整份 boot 哈希拒绝”的适配问题。现在检查内核、DTB、固定启动头、关键参数、固件 fstab／属性、支持的 Magisk 30700、原 init 和图形／Zygote 启动服务。gzip 时间与压缩级别、受支持的本机 root 配置、无关附加文件、允许的服务格式差异不再被误当成另一固件。各用户仍保留自己的原 boot、候选及完整性哈希。

实机已完成新模块安装、实际私有 boot 暂存与校验、图形服务前首次绑定、整包停用恢复原 root boot、重新启用。g18／KMD／CSF fence／DVFS 与 QMS 配套正常，双 ABI Zygisk Next、LSPosed 未被关闭。回退后重新启用会等待 Decode 启动检查完整结束；测试期间检测到一次未确认启动导致帧率保留自动撤回，已恢复原选择并单独复核。

新 keeper 需要重新生成本机私有启动配套，不自动替换既有备份仓，不分发他人 boot。未知内核、DTB、root 实现或冲突启动钩子仍拒绝；启用后被其他工具改写的 boot 仍拒绝整镜像覆盖回退。它不是通用 RK3588 或 GKI 驱动。

GPU、KMD、CSF 及已有播放策略执行代码没有升级，沿用此前验收结果。本次不宣称新增游戏帧率、HDR 校准或 HDMI 短黑收益。其他 root 打包变体仅完成离线／隔离验证，不冒充其他设备实测。Frame Guard 始终关闭并暂停。

完整边界见 [启动适配](../magisk-mali-global/boot/README.md)、[本版验收](validation/mali-boot-compat-043.json) 和 [构建说明](../SOURCE_CODE.zh-CN.md)。
