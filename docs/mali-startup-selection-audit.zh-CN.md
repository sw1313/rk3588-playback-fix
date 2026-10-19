# Mali 启动选择核对：AGtvExperience / H96 Max V58

日期：2026-10-18。本文记录前置研究；后续已经完成模块与私有镜像候选构建，但未刷 boot、未实机验收。
实现、检查与交付状态见 [0.4.0 冷启动候选](mali-cold-binding-040.zh-CN.md)。以下研究记录不能作为稳定性修复或实机升级证明。

## 已核对的基准

用户指定 AGtvExperience.img；当前 boot 只增加过 root 配置。直接解析该固件的 RKFW/RKAF 表，并核对现有提取结果。固件 boot 长度 45,219,840 字节，SHA256 为 `2c1dd07332a3d2ea5097dea50b336ec66c2979ff2e90ccbfba58fa7a26e7054d`。当前设备单 boot 分区的完整 48 MiB 备份 SHA256 为 `eabcbc53b3e56846036c8cb23fd77d246ad919aae427757e17364cb777186264`，与设备只读核验一致。

分别解析 Android boot header v2 后，内核、second、DTB 和 cmdline 均逐字节一致；ramdisk 不同。内核长 36,323,336 字节，SHA256 为 `d9b764ad8342ce71ddde45bc0978e548ae636ce6d01fb4bf3f426eba5fff0740`。后续启动适配必须以当前 root 版 ramdisk 为基础，保留现有 root 配置，不能直接覆盖固件原始 boot。

内核自带配置明确为 `CONFIG_MALI_BIFROST=y`、`CONFIG_MALI_CSF_SUPPORT=y`。AGtv vendor_dlkm 的 modules.load 仅加载 r8168.ko 和 bcmdhd.ko；检查的 vendor/odm 模块分区没有可替换的 Mali 内核模块。此固件没有可直接复用的模块化 GPU 启动配置。

## 上游的具体做法

- [Radxa 官方 RK3588 切换说明](https://docs.radxa.com/en/som/cm/cm5/radxa-os/mali-gpu)：安装匹配的用户态，选择设备树 overlay 和模块 blacklist，然后重启；文档以 lsmod 核验实际内核模块。属于 Debian/Radxa OS 环境，不能把其中的包、Xorg、udev/modprobe 配置直接搬进 Android 12。
- [Magisk 官方启动与 rootdir overlay 说明](https://github.com/topjohnwu/Magisk/blob/master/docs/guides.md#boot-scripts)：模块 post-fs-data 早于 Zygote，但不早于所有 vendor 图形服务；本固件 init.mount_all.rc 在 late-fs 显式启动 SurfaceFlinger/bootanim，早于 post-fs-data。单纯把脚本挪到 post-fs-data 不能保证 GPU 无客户端。
- [live-gpu-driver 固定源码 c789da663a5b3177b7329a22f2beec85e74b0f68](https://github.com/insterbridge/live-gpu-driver/tree/c789da663a5b3177b7329a22f2beec85e74b0f68)：用户态库 bind mount，运行进程仍须重启加载；明确不能热换内核。不能据其“直接替换”描述推断本盒子无需内核交接。
- [mali-dkms 固定源码 c446d7829308cde21194d8d2f9720b196c37bcbe](https://github.com/ginkage/mali-dkms/tree/c446d7829308cde21194d8d2f9720b196c37bcbe)：Valhall g29p1 与主线内核适配，不是当前 5.10.66/API32 的现成替换；其 BSP 服务替代与直接 PMU 恢复代码不回移到本项目。

原始源码快照、镜像、固件内容和运行记录都位于忽略目录，公开源码不携带这些私有输入。

## 已确认的干扰来源

当前成功启动的控制器记录显示：启动后约 22.3 秒开始停 SurfaceFlinger/zygote/HWC，约 25.0 秒开始恢复；约 77.2 秒完成框架恢复和资格检查。后面的等待不能全算成 KMD 绑定耗时。Decode Stack、全局图形库与内核的晚期加载又可能叠加图形服务重启。整包禁用 Decode Stack 时出现过 Mali 控制器前检退出，尚无具体退出码，不能将其归因为新驱动核心运行故障。

## 采用的目标架构及未满足条件

目标是 Radxa 风格的“启动时决定唯一 GPU 驱动”：选定匹配的 KMD/CSF/用户态，在图形客户端真正运行前完成首次绑定。现有运行中交接仅保留为旧实验后端，不能作为这项新模式的实现。

正规模块化路线需要设备配套内核将原驱动从 built-in 改为模块，并提供原版和新版 KMD/CSF；目前无原厂完整内核源码。不能使用名字相同、版本号相同或同为 RK3588 的另一块板子的整个内核代替匹配证明。

原内核包含 initcall_blacklist 支持，镜像中也存在 Mali 初始化入口，故可进一步研究“保留内核、阻止旧驱动初始化、启动早期加载新版”的启动镜像适配候选。但它还不具备可用回退实现：仅禁用内建驱动会让关闭 Magisk 模块后没有 GPU；原 init 入口属于可回收初始化代码，也不能假定进入用户态后还能安全调用。未构建仅添加 blacklist、没有加载/回退载体的不完整镜像。

启动候选必须一起具备：保留当前 root；精确 boot/内核/DTB/ABI/CSF 检查；新 KMD 首次绑定前图形服务门控；单项关闭、管理器禁用和卸载均恢复原启动路径；未成功启动的有界恢复；单 boot 分区的离线原镜像恢复办法。设置界面仍归 Mali Global，不能用独立刷机包或研究工具代替对应模块集成。

该路线涉及启动镜像的修改，不能声称只是普通 Magisk 文件覆盖。现阶段原 boot 与运行模块保持原状态，未刷写启动分区。
