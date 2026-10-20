# Root 兼容策略：按接口能力，不按版本名单

当前工作区候选为 **Mali Global 0.4.4-compat2**。普通 Decode／Mali 用户态继续无 root 品牌／版本白名单；首次绑定也已移除逐版本及官方 root 二进制哈希准入。稳定版、测试版、Canary 和本机重编译版本使用同一套接口／布局检查，不需要登记版本号。

## 检查什么

| 部分 | 当前检查 | 不作为准入条件 |
| --- | --- | --- |
| 普通模块功能 | 兼容安装脚本、工具、实际文件挂载、原厂系统依赖 | root 品牌、具体版本、作者的 root 文件哈希 |
| 当前首次绑定后端 | 运行中的 Magisk 命令能报告所需启动回调、提供有效临时目录及可执行命令；只读探测不触发启动阶段 | 稳定版列表、MAGISK_VER_CODE、Magisk APK／init／core 的官方哈希 |
| 本机 boot 准备 | 原厂内核／DTB／启动契约；本机 init 和 root 命令 ELF 结构及所需 overlay／回调标记；标准统一／分离、压缩／未压缩载荷布局 | 与作者完整 boot 或官方 root 文件逐字节相同 |
| 配置和保留 | root 原记录逐字节保留；兼容旧版缺省字段、额外配置字段和可选 ARM32 root 载荷 | 只接受某个版本的配置键集合 |
| 回退 | 本机原 boot、候选及备份哈希；已有启动仓不自动替换；外部改写的 boot 不自动覆盖 | 另一台盒子的备份或整镜像哈希 |

本机原 root 文件的摘要只保存在私有准备记录里，用于说明保存了什么，不参与跨设备／跨版本白名单匹配。原厂 init 的备份哈希仍用于确认固件原件，不能与 root 替换后的 init 哈希混为一谈。

CLI 输出和 ELF 标记只验证启动所需前提，不能静态证明所有未来版本的启动内部行为。首次试启动仍需匹配私有载体、实际挂载、GPU 首次绑定、图形服务与双 ABI Zygisk／LSPosed 完成确认；未确认仍按原框架回退。没有把“接口存在”标为“全部版本实机通过”。

## KernelSU／APatch

普通功能不因这两个名字被拒绝，但必须提供实际模块挂载和脚本接口。KernelSU 使用 metamodule 时需具备系统文件挂载组件，不能仅以管理器显示启用判定覆盖成功。各工具可能改变内核，仍独立验证真实内核 ABI。

当前早期载体调用 Magisk 的 overlay／启动回调，故其他 root 缺少对应后端时，只限制首次绑定及依赖它的功能；不关闭整个模块，不默认切到旧运行时 GPU 交接，也不伪报支持。KernelSU 当前提供 initrc／preinit 机制，但需依实际模式和本设备内核单独适配与验收。

## 上游参考

- [ReZygisk 安装器](https://github.com/PerformanC/ReZygisk/blob/main/module/src/customize.sh)：root 提供方分支和必要最低版本，区别于逐稳定版枚举。
- [KernelSU 模块文档](https://kernelsu.org/guide/module.html)／[metamodule](https://kernelsu.org/guide/metamodule.html)：兼容模块生命周期和挂载组件；模拟的 Magisk 版本不能作 root 类型判据。
- [APatch 模块文档](https://apatch.dev/apm-guide.html)：相近模块结构、工具路径及挂载阶段差异。
- [AnyKernel3](https://github.com/osm0sis/AnyKernel3)：基于本机 boot／ramdisk 最小修改并保留本机内容。
- [Magisk 启动／overlay 文档](https://topjohnwu.github.io/Magisk/guides.html#boot-scripts)：启动阶段与动态 MAGISKTMP 路径。当前内部回调依赖仍需首次启动实际验收。

本地仅参考接口与设计，没有复制这些项目代码。官方载荷的固定摘要现仅放在 `tools/tests/data/magisk-upstream-fixtures.json`，用于可复现回归输入，模块和准备工具均不读取这份测试数据。验证结果与边界见 `docs/validation/root-interface-compat-044.json`；正式 v0.8.7 附件不被候选覆盖。
