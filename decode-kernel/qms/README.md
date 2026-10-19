# Decode Stack QMS 实现进度

**最新冷启动试验结论：** owner／endpoint／Decode loader 与 HWC 已连接并构建为受限候选，实机 finit_module 在身份门禁返回 ESTALE，尚未发布功能端点或启用 VTEM。设备已恢复原工作 boot／Decode 0.8.4-test3／g18，并确认 Zygisk Next 双 ABI 注入及 LSPosed 正常。Frame Guard 关闭；候选未通过，不再自动重试。详见 [冷启动集成与恢复](../../docs/decode-qms-cold-086.zh-CN.md)。下文保留前序分层研究记录，硬件结论以本段为准。

这里是系统显示扩展的 GPL-2.0-only 源码。已实现控制核心、事务／会话、原厂提交窗口、寄存器写入器、提交后发布、板级链路校验，以及候选控制线程和ioctl分发源码。**仍缺少可持续运行的完整硬件绑定与启动加载，设备上未注册QMS控制端，也没有可安装的完整QMS包。** 当前设备未启用QMS；研究探针均已卸载。它不依赖应用专用LSPosed钩子，不修改Frame Guard或已安装的Mali。

## 已实现

- `core.c`／`core.h`：以真实vblank推进启用、目标帧率通知、变速、稳定、恢复基础时序和关闭；generation／revision防止旧操作回写；硬件错误不计为完成，离线规划不改变已提交状态。
- `timings.h`：选择性引入Rockchip 56组时序，VIC102复用VIC97；按明确长度查找，保留逐帧分数模式及真实1000/1001帧率。本地修正两处上游数据，范围见 `UPSTREAM.json`。
- `edid.c`：检查EDID长度、块校验和及CTA边界；解析HDMI Forum QMS／TFRmin／TFRmax和VRR范围。只输出接收端能力，不能代替实际链路／硬件门禁；暂不支持仅HF-SCDB形式的EDID或冲突重复VSDB。
- `transaction.c`：按上游布局写8个VTEM字，再仅修改TX_EN bit9；通过后端事务操作更新时序。每步检查返回值；部分失败标记必须经原生恢复，不能报告无缝成功或自行猜测寄存器回退。真实写入器在`native_registers.h`，当前实机仅验证原值写回，未启用VTEM或改变帧率。
- `native_atomic.h`：原厂内核原生事务分配／CRTC复制／引用销毁；只设置新分配事务的原生acquire_ctx和关闭ALLOW_MODESET，不改实时CRTC状态，也不调用atomic_check或atomic_commit。偏移来自原始镜像函数反汇编，非较新头文件结构推测。
- `uapi.h`／`session.c`：固定ARM32／ARM64布局的状态／切换／恢复协议和私有会话。命令有generation、link cookie、串号、连接器和超时门禁；时序写入成功后，还须观察下一次真实vblank才确认。热插拔／超时／漏帧进入待原生恢复状态，不能冒充已恢复；原生排空确认使用新generation取消旧请求。**协议声明不等于已经注册了内核ioctl设备。**
- `native_carrier.h`／`native_engine.h`：使用原厂受影响planes和正常fence路径，在同一次原生flush中执行I/O；会话私有暂存，原厂提交和实际vblank／寄存器完成检查都通过后才发布。关闭状态的真实观察已实机通过；变量时序和持续调度尚未验证。
- `native_recover.h`：在原厂克隆中设置ALLOW_MODESET／mode_changed，包含受影响连接器和planes，调用原厂完整重建。基础模式重建已实机通过，仅通过ADB核对模式、缓冲和寄存器，物理显示观察仍待验收。新增“仅清除自有VTEM”及恢复后寄存器检查；清除已启用VTEM和变速后的恢复尚未实机验证。
- `controller.c`／`endpoint.c`：候选内核线程、独立状态发布、限时阻塞命令及固定大小ioctl分发。后台等待原生fence不会占住状态发布锁，迟到的完成不能报告按时成功，未受理的过期命令会取消。只在完整proof和运行中的控制器存在时允许注册。两者已按原厂头文件环境编译为独立对象、核对导入符号；**尚未连接完整硬件绑定、链接进功能模块或注册真实设备端点。**
- `lifecycle.c`／`native_lifecycle.h`／`native_pm.h`：回移厂商显示关闭重置语义，补充系统休眠旁路。桥接mode_set／disable／enable使旧epoch失效，断电前只清理自有VTEM；失败保留dirty，恢复后不自动重用旧链路。PM的suspend／freeze／poweroff在退出QMS失败时拒绝进入原厂断电步骤；resume／thaw／restore保留重新资格检查。当前仅构造候选表，**没有发布bridge或PM指针**。本机已验证相关原厂表、bridge私有指针和device／driver PM布局；回调执行仅有注入测试。
- `native_binding.c`／`native_qualify.c`：把逐帧carrier／flush窗口／私有会话发布、原生恢复及PM退出接成控制器后端；恢复先排空正常原厂事务，外部模式改变时不覆盖为旧基础模式。已接入实际板级校验源码，逐帧提交前后检查当前链路、cookie、generation、生命周期及格式／HDR身份；恢复检查实际原生时序与VTEM关闭。独立内核对象已编译，尚未作为持续功能绑定执行。
- `native_link.c`／`native_link.h`：使用当前原生EDID blob和原厂DDC块读取器直接核对物理接收端，不调用会更新EDID属性的`drm_get_edid`，也不接受软件覆盖。检查实际HPD、PHY就绪位、PHY时钟、输出格式、AVI校验和及静态HDR包与原生元数据一致；资格失败立即失效，不采纳外部VTEM／AMD VRR／DSC。当前限定未压缩TMDS及已核对的RGB／444／420八位、十位格式，其他格式和FRL拒绝启用，不主动改变显示质量。断电清理需要生命周期所有权证据；断电标记本身不能证明已清理。它不创建BOOT等proof位。
- `magisk-hdr/src/qms_client.h`／`qms_frontend.h`：MIT候选前端，接入真实HWC2 SetActiveConfig／GetActiveConfig／SetPowerMode回调；仅在内核确认目标已生效后保存逻辑配置，不改原厂物理mode／vsync／fence。检查色深、EOTF、色彩空间及几何不变；后端失联／恢复未确认时报告失败，不并行跑普通modeset。仅`tools/build_hdr.py --qms-candidate`编译，并需当前boot token及真实设备端点激活。
- `magisk-decode/qms-cold-stage.h`：MIT候选启动协调，由`cold_boot.c`的`RKQMS_DISPLAY_STAGE`编译门禁调用。Decode的独立选择／payload／当前overlay计划确认在GPU加载及图形服务释放前完成；失败关闭显示项并请求重启，未确认的上次试启动自动跳过显示项。**真实显示loader、payload及boot适配尚未交付；普通Mali构建不包含此阶段。**

## 验证证据

`tools/qms/test_core.py` 使用NDK构建实际C源码并在ARM64模拟器执行，不用Python重新实现状态机：66组完整周期精度检查（含VIC102）、738组切换／恢复、94,464个稳定阶段采样、15个门禁／旧操作／失败案例、11个事务故障注入及10个EDID场景通过。没有调用设备或显示寄存器；这些数字不代表HDMI无黑或游戏性能验收。

`tools/build_display_probe.py --qms-abi ...` 构建受精确Image和模块元数据门禁限制的内部探针。本机已验证当前模式能初始化核心、HDMI regmap使用32位值／4字节步长及自旋锁、原厂CRTC事务能复制并销毁且实时状态指针保持不变。29项导入CRC及7张表通过；拒绝门禁、加载、正常卸载通过，无残留。探针参数 `qms_private_state_backend=1` 仅表示**离线核心接受当前模式**，不表示硬件后端已实现。

新增`tools/qms/test_session.py`执行实际ARM64 C：22个请求门禁、20个生命周期／故障案例、25个HWC策略案例和14个精确帧率案例通过；缓存私有EDID也通过实际C解析器，但不代表当前链路可用。UAPI尺寸／偏移在ARM32和ARM64均编译检查。另在盒子上运行注入传输的实际HWC回调，22条断言通过；启动协调的17个受限文件系统场景通过。两项实机隔离测试均没有打开显示硬件、加载功能模块、操作播放器或改变实际boot／GPU／Android属性。

普通冷启动binary编译检查不含QMS阶段；显示候选进入Decode打包器会被拒绝，因为当前缺少真实内核／启动payload。没有生成可安装的QMS整包，不能将这些测试计为硬件或实屏验收。

## 硬件接入的关键限制

不能在已验证的vblank tracepoint中直接调用这套事务层：那里持有DRM event_lock，并处于中断上下文。原厂VOP2的reg_lock还存在普通spin_lock用法，直接在中断取得同一锁可能阻塞被中断的持锁任务。也不能单独写cfg_done，提交到原厂尚未完成的图层影子寄存器。

原厂同一次提交窗口已通过限定实机实验：持有原生all-modeset锁，复制helper表且只替换flush，在原厂commit-tail mutex下发布／恢复；仅对专用任务及其事务执行新增操作，原flush恰好委托一次。原值写回时8个VTEM数据字、基础时序、scheduler其他位均保持不变，TX_EN bit9始终关闭。该限定实验不能外推为永久挂接或任意热卸载安全；分数帧的截止时间、正常应用合成并发、热插拔／电源生命周期仍待完成。

实际原厂寄存器表使用合并的vtotal_pw描述。已纠正早期私有笔记的误认：端口regs指针在port-data+0x58，表+0x90／寄存器0xc4c是HACT；vtotal／VS_END在表+0x9c、寄存器0xc50（按port加0x100），mask=0x1fff1fff。旧地址检查在自定义写入前即被拒绝，未放松门禁。DSP_CTRL bit15仍是post CSC，未将其作为即时vtotal位操作；vtotal字保留pulse和reserved位。

新增实际C验证：13项提交后发布／错误注入检查通过；Android隔离执行通过15项控制线程并发／超时和11项ioctl分发检查。后两者使用pthread和注入的显示操作，未注册真实字符设备，不构成内核ABI、硬件或短黑验收。

生命周期新增17项纯C策略及26项实际native_registers／bridge／PM代码注入检查通过，包括断电后禁止MMIO读取、TX_EN实际值不符、清理失败、恢复后不自动重启QMS、休眠前退出失败和其他HDMI端口透传。它们没有操作显示硬件。本机只读准备检查匹配8张原厂表，未提交显示事务／写寄存器／发布回调，随后正常卸载。厂商整套提交的逐项映射见`PORTING.json`；仍保留完整移植未完成的结论。

功能挂接需要在图形服务首次使用前完成，并保留代码／对象生命周期；停用后重启恢复。现有GPU冷启动屏障会自行释放图形服务，因此单纯把insmod放进Decode的post-fs-data，不能证明顺序正确。候选协调代码已在隔离测试中核对先后顺序，尚未连接真实显示loader或写入boot；不能把该测试当成真实首次绑定／重启回退通过。

## 下一步尚未完成

1. 把已验证的限定提交窗口扩展为持续绑定，完成正常合成并发、热插拔／电源、已启用VTEM及变速后的原生恢复、分数帧截止时间验证。
2. 将控制线程和ioctl源码连接完整硬件绑定并进行实际内核测试，再将HWC候选接到真实端点；核对变速时原厂vsync时间戳，不能只改返回值或注册空next_tfr属性。
3. 完成真实Decode显示loader、分项选择／依赖、固件／ABI门禁及boot适配，将已测试的协调阶段接入；构建候选后再做首次加载及关闭重启回退验收。
4. HDMI实屏无黑、完整十位／HDR、兼容性和回退分别验收。Frame Guard全程关闭，播放操作由用户完成，不截图或录屏。

源码状态与研究验收见 `docs/validation/system-hdmi-commit-086.json`。保持 `release_accepted=false`，不升级模块版本号或把核心测试计为用户目标完成。

## 首次启用前的实时链路检查

当前实际TMDS 3840×2160p60、YUV420八位SDR路径已通过新增只读检查：物理DDC逐块读取与当前原生EDID完全一致，QMS能力为真；HPD与原厂PHY就绪位匹配，时钟框架读数297MHz符合原生输出格式。原生基础时序／影子值一致，VTEM关闭。最终探针32项模块CRC、8张原厂表通过，正常卸载并清除临时文件；没有显示提交、寄存器写入、回调发布、重启或boot修改。中间一次新增PHY身份检查因把platform-data误当PHY而被拒绝；已按原厂完整bind中实际的+0x108存储纠正，并扩展完整函数指纹，未绕过门禁或进行显示写入。

`tools/qms/build_native_link_test.py`构建实际板级读取／资格／清理及绑定校验代码，Android隔离执行54项检查通过，包括DDC不一致、PHY未就绪、断电禁止MMIO、原生时序不符、HDR校验和／元数据不一致、旧cookie／generation／epoch和外部VRR占用。十位／HDR及异常断电场景使用注入数据，尚无对应硬件验收。控制后端对象已重新编译；这些对象没有链接到功能模块。

这表示当前接收端和TMDS SDR链路具备继续做QMS试启用的前提，**并非已经可以打开功能开关**。还需要持久cold owner、事件后重新资格、真实端点／HWC与Decode启动加载及重启回退，再执行真正启用VTEM、变速和恢复测试。Frame Guard持续关闭；短黑结论只接受实际HDMI显示器观察。
