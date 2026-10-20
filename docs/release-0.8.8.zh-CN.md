# v0.8.8 — Mali Global 0.4.4 root 兼容更新（正式版）

本版更新 Mali Global 至 **0.4.4**。Decode Stack 0.8.6、LSPosed 客户端 0.3.7、可选 PanVK Test8.1 保留原字节和原来源；已安装这些版本的用户仅需更新 Mali。发布标签沿用 `v0.8.8`，组件版本不带测试后缀。

解决同一固件下不同 root 版本、测试版或本地构建被版本号／官方 root 文件哈希名单拒绝的问题。普通模块入口不按 root 品牌或版本号拦截；首次绑定单独核对实际启动接口、运行目录、本机载荷结构及启动布局。不是取消固件检查：内核、DTB、原厂 init、启动契约、图形 ABI 和冲突钩子检查继续保留，各用户仍保存自己的原 boot 与候选完整性哈希。外部工具改写 boot 后，仍拒绝用旧整镜像覆盖回退。

当前 Magisk 30.7 已完成完整安装、重启、图形服务前首次绑定及 ARM32／ARM64 GLES／Vulkan 验证；Mali g18、配套内核／CSF fence、DVFS、QMS 与帧率保留正常，Zygisk Next 双 ABI／LSPosed、模块启停和原选项、Auto／HDR 均保留。Frame Guard 关闭并暂停。本次接口策略验收没有写入新 boot，GPU／内核／CSF 与 keeper 二进制沿用已验收版本。

其他 Magisk 稳定版、31.0 预发布版和本地变体仅完成离线接口／布局回归，未分别实机启动。KernelSU／APatch 的普通安装入口不设品牌门槛，但仍需可用挂载和匹配固件 ABI；**其专用首次绑定后端尚未实现**。接口存在不等于全部 root 启动语义都已验证。

正式化只更新版本、文档、WebUI 静态缓存标记、来源及完整性清单；运行脚本、原生库、内核模块和 keeper 与已实机验收的接口策略逐字节对照。历史验证记录保留当时的测试版本名；不将更名当成新一轮实机启动或性能验收。

首次绑定仍需本机私有 boot 适配；现有 keeper 未变，不因此次更名要求重新刷 boot。新设备按自己的原镜像准备，既有备份仓不会自动覆盖。关闭分项并重启可回退，涉及 boot 的回退仍受个人镜像归属保护。公开附件不包含固件、root APK、私有 boot、设备地址、原始日志或密钥。

本次不宣称新增游戏帧率、HDR 校准或 HDMI 短黑收益。Frame Guard 不随本版提供。

[兼容边界](root-compatibility.zh-CN.md) · [接口策略原始验收](validation/root-interface-compat-044.json) · [正式包对照及验收范围](validation/release-0.8.8.json) · [构建说明](../SOURCE_CODE.zh-CN.md)
