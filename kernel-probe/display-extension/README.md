# 系统显示专用内核扩展研究

当前已验证精确固件上的对象识别、被动vblank、限定原厂提交／flush窗口及基础模式完整重建。这里没有可持续运行的QMS实现、真实控制端或可发布的短黑修复，不进入Decode Stack安装包，不配置开机加载。

后续QMS控制核心和事务层已写入 `decode-kernel/qms/`，完整硬件绑定仍未完成。新增 `--qms-abi` 验证原生模式、regmap元数据，以及原厂原子事务的分配／CRTC复制／销毁；不调用atomic_check／commit，不访问显示寄存器。其正常卸载结果不能外推到未来功能挂接。

`--qms-carrier`是会产生显示操作的独立实验类型。默认仅原厂check；`commit_once=1`执行一次原厂提交；`window_once=1`增加受原厂mutex约束的限定flush挂接；`register_echo=1`只写回原寄存器值；`session_observe=1`只验证内部OFF会话的提交后发布。`recovery_check=1`检查原厂恢复事务，与`commit_once=1`合用会做一次完整modeset，可能短黑。QMS从未启用，这些限定卸载结果不承诺功能模块可热卸载。

`--qms-control-objects`仅编译／核对候选controller和endpoint的独立对象，不将它们链接进探针、不注册接口或加载功能驱动。详细证据和剩余工作见`decode-kernel/qms/README.md`。

`lifecycle_inspect=1`只在私有内存构造bridge／PM候选表，并检查原厂对应指针、函数槽及8张表的完整契约；不发布表、不调用候选回调，不与`commit_once`／window／echo／session实验混用。该只读检查已实机通过，不能据此推导实际休眠或持续QMS挂接通过。`--qms-control-objects`现也编译lifecycle和native_binding，依然是与探针分开的独立对象，实时板级验证器尚未提供。

`probe.c` 只在初始化期间获取平台设备引用和 trylock，验证指令／回调表指纹、DRM／HDMI QP 对象归属，再调用原生 connector／CRTC 查询 API。所有设备锁和引用在初始化返回前释放；不保存设备指针，不替换回调、添加属性、注册 IRQ 或访问 MMIO。

`vblank_observer.c` 使用 `for_each_kernel_tracepoint` 和 `tracepoint_probe_register` 接入已有 `drm_vblank_event`，回调只增加原子计数。它不主动开启 vblank、不更改模式或时序。退出调用 unregister 和 `tracepoint_synchronize_unregister`，等待 SRCU／RCU 后才释放模块。注销失败时留在退出流程中重试，保留回调代码内存；该异常路径未做实机故障注入。此标准 API 会修改内核自身的 tracepoint 注册／静态分支状态，不能描述为完全不改变内核状态；它没有显示写入或直接文本补丁。

## 已取得的实机证据

- 对象探针：7 张静态表校验通过，3 个 connector、4 个 CRTC；QP 对象与原生 connector 枚举恰好匹配一次。
- 事件观察：只有一个目标 tracepoint；最终版本短时读取 pipe0 计数197，注销完成日志计数210，其余 pipe 和越界计数均为0。两次取值时刻不同，不是丢帧或性能结果。
- 两个最终探针均验证了 `reject_guard=1` 拒绝加载、正常加载和非强制卸载；结束后无模块残留。Frame Guard 关闭，boot 和显示配置保持不变。

只在同一固件的一次短时运行中验证了这些行为；没有长期、切模式、热插拔、HDR 或物理黑场验收。被动回调没有 DRM 设备身份参数，仅有 pipe，后续实现不能在多 DRM 设备上直接用 pipe 当全局设备标识。

## 模块元数据 ABI 差异

首个构建虽然通过版本串、896字节结构总大小和 CRC 检查，原生 Linux 头文件却将 init／exit 放在0x150／0x348；原厂内核实际读取0x158／0x360。负向试验揭示初始化未执行，已卸载该探针，未进入显示对象访问。

构建工具现在用已经实测的同固件 GPL guard 的**非执行元数据布局**作精确模板，并与原内核 do_init_module／delete_module 反汇编交叉核对。只序列化名称、init／exit重定位和零填充，不复制 guard 代码；独立验证输出 ELF 的偏移、对齐、大小、导入白名单和全部 modversions。探针代码只取 THIS_MODULE 的地址，不按不匹配的头文件访问其成员。这不是 force-load，也没有忽略 CRC。

这项发现说明版本串、CRC 和大小相同仍不足以证明私有结构兼容。功能扩展中的每一个实际访问字段都需要相同程度的验证。

## 构建

从仓库根运行 `python tools/build_display_probe.py --help`。必需输入为 NDK、Linux5.10.66头文件树、精确原 Image、恢复符号 ELF、已验证模块模板和全新 work 目录；`--observer` 选择事件观察模块。缺少 pyelftools 时用 `--python-libs` 指定依赖目录。

原 Image 与模块模板有固定 SHA256 门禁，所有 Python 完整性检查在 `python -O` 下仍执行。生成契约／二进制只放私有工作目录，不公开原镜像、绝对符号表或运行日志。工具只构建，绝不连接设备、加载模块或修改 boot。

## 功能实现仍需完成

1. 原固件 QP connector／CRTC 回调的对象内指针可定位，但复制表再切换指针仍需证明 atomic commit、state复制销毁、解绑、并发读者和启动依赖。只找到地址不等于已证明可安全挂接。
2. vblank tracepoint 在 DRM event_lock／中断上下文中。不能在其中睡眠、等下一帧、调用可能重入 event_lock 的 DRM API，或直接运行 HDMI工作队列状态机。寄存器更新的锁顺序、预算、vblank启停与正确硬件帧边界尚未证明。
3. 上游 d0e8f558 的分数 vtotal／VFP及 cfg_done，与5a89a1cb 的 VTEM、M_CONST、next_tfr、多帧状态机必须共同适配；不能扩写旧私有对象的分配边界，应研究独立状态存储及其生命周期。
4. HWC须真实使用新时序路径，支持合适的 QMS 基础模式、电视能力、色深／HDR约束，并为不适用的模式保留正常完整切换。不能只注册一个空 DRM 属性或报告虚假的 seamless 成功。
5. 功能候选最终集成 Decode Stack，默认关闭，包含精确固件／ABI门禁和启动依赖。功能挂接不能套用被动探针的热卸载结论；在安全撤钩未证明前，按停用后重启恢复设计，boot保持可恢复。

上游依据及完整边界见 `docs/system-hdmi-blackout-086.zh-CN.md`；公开研究验收记录见 `docs/validation/system-hdmi-commit-086.json`。
