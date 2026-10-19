当前正式版（2026-10-20）：Decode Stack 0.8.6／Mali Global 0.4.2。test20 的系统图层帧率保留已实际命中（0→24Hz 保留请求），用户确认原 Exo 场景可以了，并授权整理发布。临时追踪已关闭；Auto/HDR、Frame Guard 关闭、g18/KMD/DVFS、双 ABI Zygisk/LSPosed 及原 boot 均保留。其他片源帧率、所有接收端和完整 HDR 校准不由这次验收推定通过。见 [帧率保留记录](decode-refresh-session-086.zh-CN.md)。

以下为此前阶段记录。

当前进展（2026-10-19）：test19 系统帧率保留候选未通过，用户确认播放器内仍短黑。实际读取其内存计数为 retain=0、source=0、trace=0，同一有界窗口 QMS serial 从 12 增至 20，说明不能把“库已加载”计为策略生效。旧入口未覆盖本轮请求；现改为原厂 Layer::setFrameRate 共同落点的 test20 候选，正在集成／重启验收。Frame Guard 保持关闭，短黑尚未修复。见 [会话帧率保留](decode-refresh-session-086.zh-CN.md)。

以下为此前阶段记录，当前状态以上方为准。

当前对照（2026-10-19）：test17 已取得新边界证据，播放／暂停／步进仍整幅短黑。现已安装并重启 Decode `0.8.6-qms-test18-ddc1`＋Mali `0.4.2-qms-test20`，只重测直连条件下的 VTEM END=0，启动检查通过，实屏结果待反馈。此前 test13 经矩阵的无改善结论保留，本次不计新升级；无改善则退回 test17。Frame Guard 保持关闭。见 [直连单字段对照](decode-qms-direct-end0-086.zh-CN.md)。

最新实屏结果（2026-10-19）：Decode `0.8.6-qms-test16-ddc1` 已安装重启，Mali `0.4.2-qms-test18` 仅更新精确配套识别。前一轮抓到整数 STABLE 最终观察迟到触发完整重建，候选修正该路径；用户反馈比上一版好一些，但播放／暂停仍偶有整幅短黑。新采集 14 次切换完成、零帧错误、无原厂重建，3,600 次结构采样无变化。新增分支本轮命中为 0，不能把改善全部归功于它或计为故障现场硬件验收。Auto/HDR、DDC／声音修复、g18/KMD/DVFS、双 ABI Zygisk/LSPosed 及 Frame Guard 关闭保留；诊断均停止。完整 HDR 及剩余无重建短黑继续单独待验收。见 [test16 记录](decode-qms-late-latch-086.zh-CN.md)。

以下为此前采集及阶段状态。

最新实屏更正（2026-10-19）：用户确认直连 Auto 仍短黑，撤回此前“直连无短黑”的判断。当前保存键已为 Auto、刷新范围已解除固定 60 限制，QMS 已实际完成 6 次切换，错误码与帧错误均为 0；保留记录未观察到 VOP／PHY 重建或音频假拔插。矩阵不是触发故障的必要条件，不能据前次误看归咎矩阵。正在做有时限的接收端 SCDC 锁定关联；完整 HDR 实屏仍待单独验收。

最新启用状态（2026-10-19）：按用户要求恢复既有已验证冷启动配置，直连 EDID 修复后先执行一次原厂初始化，再绑定 QMS；QMS owner／HWC／端点、GPU RGB10 allocator／SF／HWC target plan 均已在实际启动 active，错误码为 0。Mali g18／KMD／DVFS、双 ABI Zygisk Next／LSPosed、Frame Guard disable／paused 均通过。桌面当前为 4K60 YUV4208 SDR；仍待用户播放后验收短黑、声音和真实 HDR10 位深／EOTF，不能以开关 active 计完整 HDR 或短黑修复通过。详见 [本轮启用记录](validation/qms-hdr-direct-enable-086.json)。

以下为本轮启用前的直连修复及更早阶段状态。

最新直连状态（2026-10-19）：用户更正此前经过 HDMI 矩阵。当前直连 LG C5 HDMI1；Decode 已集成 E-DDC 分段读取修复和一次性原厂初始化，用户曾确认恢复信号；随后补入的自动开机初始化已提交成功、时钟／三通道锁定，用户已确认本次重启后自动出画面、无自动暂停且声音正常，仅剩短黑。Exo 自动暂停日志有假拔插→NOISY 广播链，已恢复已有系统层音频事件过滤；短黑未修复。当前实际 QMS 未加载，GPU 用户态 g18、KMD 原厂、DVFS off，双 ABI Zygisk Next／LSPosed 正常，boot 与本轮开始一致。不要用下方旧冷启动通过状态替代当前状态。见 [直连修复](decode-ddc-direct-hdmi-086.zh-CN.md)。

下方 test14／test15 为矩阵链路与先前冷启动的历史记录。

当前进展（2026-10-19）：已恢复 Decode 0.8.6-qms-test14＋Mali 0.4.2-qms-test16，并通过实际回退重启检查。test14 修正稳定状态下过宽的数据包检查，8 次切换完成、零帧错误，实屏播放／暂停／步进仍短黑；切换边界未观察到 GCP／HDCP2 变化。test15 四帧提前通知已实际执行，用户反馈没有改善，已从源码和设备撤回，避免增加无收益延迟。Auto／HDR、Frame Guard disabled＋paused、Mali g18／KMD／DVFS、双 ABI Zygisk Next／LSPosed、模块标记和 boot 均保留。短黑尚未根治，后续核对真实 VTEM 发送／接收时序，不能把本地切换完成视为电视验收通过。详见 [系统 HDMI 进展](system-hdmi-blackout-086.zh-CN.md)。

以下 test11 及更早摘要为历史阶段。

当前状态（2026-10-19，test11）：系统设置修复已通过两次真实启动，display_policy=1，用户确认 Auto／修复开关正常，电视已显示 QMS-VRR。Decode 0.8.6-qms-test11 保留 test9 QMS 核心；Mali g18／内核／DVFS、双 ABI Zygisk Next／LSPosed 正常，Frame Guard disabled＋paused，hdmi_audio_guard=0。Auto 下实屏短黑仍未修复，固定 4K60 不短黑；当前继续系统 QMS 时序／生命周期根修。其他系统实际调用、已知修复及本机适用性见 [系统 HDMI 对照](system-hdmi-blackout-086.zh-CN.md)。完整 HDR、固定模式持续锁定等仍各自验收，不由设置启动成功推定通过。

以下启动失败及 test8／test9 摘要为历史记录，当前状态以上述 test11 为准。

当前状态（2026-10-19，设置候选 test10 已撤回）：“系统分辨率／HDR 设置修复”未修复。jar-only 和配套系统 OAT 候选均在真实启动中触发 system_server 的 ART untrusted data-cache abort，继而整包自动回退；补齐编译产物仍不足，早期缓存选择／加载顺序待查。已撤回 test10，恢复 test9 原脚本、组件与工作选择（display_policy=0、hdmi_audio_guard=0），已实机确认恢复：Decode、Mali g18／内核／DVFS、双 ABI Zygisk Next／LSPosed 均正常，全部模块标记与试验前相同。不能以开关保存、离线检查或配套编译成功计为可启用。QMS 播放验收暂停；Frame Guard 保持 disabled＋paused，Zygisk Next／LSPosed 选择保留。实屏结果仍为步进不黑，跳段和播放／暂停短黑；LG 无 VRR 提示不作为未启用的证据。

当前状态（2026-10-19，test9 已实机启用）：Decode Stack 0.8.6-qms-test9 和 Mali 0.4.2-qms-test13 已完成冷启动；QMS 新 owner／端点／HWC、Mali g18、内核、DVFS、Zygisk Next／LSPosed 双 ABI 均通过本次启动检查，模块启停标记与试验前一致。Frame Guard disabled＋paused。新版恒定时序保持／命令唤醒／100ms 只读核对已进入真实设备，尚待本轮 Exo 操作验证持续 24Hz、恢复和开销；HDMI 实屏无黑仍未通过。当前优先 QMS 主问题，暂停 UI 工作。HDMI 音频事件适配和系统显示设置修复暂时关闭用于隔离启动问题，不算其验收或永久撤销，其他播放升级保留。

当前状态（2026-10-19）：设备运行 Decode Stack 0.8.6-qms-test8，QMS owner／端点／HWC 已启用；Mali g18、DVFS 和 Zygisk Next／LSPosed 双 ABI 正常，Frame Guard 关闭。Auto 菜单勾选已由用户确认正常，HDR10 默认显示开启已确认。指定 Exo 暂停恢复和快进索引有各自的局部通过记录，不能等同所有应用验收。test8 的真实播放日志定位到恒定 24Hz 观察跨过两个真实帧触发 gate93，导致回到 60Hz；QMS 持续输出与 HDMI 实屏无黑仍待验收。新版 test9 候选改为恒定时序硬件保持、事件唤醒及 100ms 只读核对，尚未部署。

# 主要目标：游戏与播放统一排序

当前先推进系统 HDMI QMS 持续输出及短黑，保留已修复的队列背压和 LSPosed 快进索引。实际播放／暂停／快进由用户操作；不截图或远程输入。游戏目标继续保留，未做的游戏验收不计为收益。下表是长期目标及历史阶段记录；其中旧版本“保护继续开启”“QMS 未启用”的叙述，以本页当前状态和 [QMS 记录](decode-qms-cold-086.zh-CN.md) 为准。

用户要求按实际收益、影响范围、可集成和可回退程度推进；下列是执行顺序，
不替代最初七项方向的历史编号。预期收益取决于 GPU／CPU／带宽或应用接口
是否确为瓶颈。已有局部交付不等于整行完成，也不以版本号或诊断包代替升级。

内核组件允许升级，不因类别一概后置；匹配当前固件、ABI、启动依赖且能可靠
回退的，按预期收益纳入。现有Mali首次绑定／boot恢复仅作可回退架构的参考，
不代表把g25作为主线。g25尚未完整验证兼容、稳定和收益，当前不升级；其他
组件分别确认可替换条件，不能把Mali的可行性直接外推。

| 顺位 / ID | 目标 | 预期收益及适用条件 | 已有成果 | 尚需完成 | 归属及回退 |
| --- | --- | --- | --- | --- | --- |
| 1 / G1 | 游戏 Mali 用户态/KMD/CSF 配对、队列调度与公平性 | GPU 受限或多 GPU 客户端竞争时，可能改善帧率及长帧 | g18 配对、图形服务前首次绑定；CSF fence 追踪修复仅属稳定性 | 性能相关具体候选、游戏／并发帧时间收益；完整新版驱动 ABI 配对 | Mali Global；独立分项、配对校验、重启回退 |
| 2 / G2 | 游戏 DVFS 响应及持续负载稳定性 | 突发负载、调频不及时或持续卡顿场景 | 100→20ms 检测周期，已有有界 GLES 突发负载验收 | 真实游戏持续帧时间、功耗和温度；CPU/GPU 竞争须先定位，不全局锁频 | Mali Global；独立开关，保留 OPP、电压及温控 |
| 3 / P1 | AFBC、DMA-BUF、原生十位输出与零复制 | 4K／十位复制和格式转换开销大的场景 | AV1 显示/纹理 AFBC、mapper 复用；0.7.6 特定 HEVC 原生输出消除 P010 拷贝；0.7.8 必需 P010 解包减少 CPU 工作量 | 更多 GPU 纹理／必需转换路径，原播放器 UHD 取帧超时 | Decode Stack；按项关闭并重启 |
| 4 / P2 | HWC 平面分配和合成策略 | 图层因错误限制进入 CLIENT 时减少 GPU 合成 | 0.7.5 过时缩放限制修正、特定图层 DEVICE；0.7.6 配套原生显示 | 通用多图层规划、稳定显示延迟收益；完整新版 HWC | Decode Stack；恢复原 HWC 路径 |
| 5 / P3 | Codec2 异步队列、取帧、seek/EOS | 解码队列／生命周期确有问题的卡顿和拖动 | 所有权、flush/release、独立配置通知、0.7.7 HEVC EOS 非图像输入归还等局部修复 | 原播放器 UHD pacing、其他路径 HLG seek/EOS 超时；不缩减参考帧或伪造统计 | Decode Stack；每组修复独立回退 |
| 6 / P4 | HDMI 内容帧率匹配、HDR 状态和刷新恢复 | 24fps 等内容播放节奏及切换体验 | 限定十位 HDR 通过；0.8.2-test1 合并换时序假断开；QMS 已首次绑定启用；Frame Guard 按用户要求关闭 | 修复恒定输出观察导致恢复 60Hz 的故障；变速、恢复和实屏无黑验收 | Decode Stack；重启回到原策略 |
| 7 / P5 | RGA/libyuv 格式转换与库升级 | 仍需转换的实际路径可能获益 | RGA 用户态 1.10.6、libyuv 选择性修复 | 确认新格式的内核能力及全十位精度，不能仅看库头文件 | Decode Stack；保留原库和分项开关 |
| 8 / K1 | DDR/DMC、VOP2/HDMI/VPU 内核 | 内存带宽受限或具体内核缺陷时可能较大 | 当前未交付这些内核升级 | 匹配厂商源码、板级适配、可恢复 boot 与真实收益；当前后置 | 对应现有模块与 boot 配套验证，不能假设为可换 .ko |

游戏图形兼容也属于 G1：原 g13 和当前 g18 均未提供 geometryStreams，
此前模拟器缺绘制的问题不能仅靠宣称 Vulkan 版本更高来解决。已有 PanVK
应用内实验项保留原边界，不能算作系统 Mali Global 或所有游戏性能升级。
NCE/JIT 属于应用 CPU 执行路径，其卡住曾在系统驱动下同样出现，不归因于 GPU
驱动已被证明的问题，不擅自更换用户游戏配置或降低渲染画质。

## 独立兼容目标 C1：Yamby HW/Main10 绿屏

历史原固件 Main10 复制输出与原播放器缺少 P010 映射是两个断点。
目前 Decode Stack `p010_copy_pool` 和现有 LSPosed test6 已分别补齐系统字节布局、
容量及客户端格式映射／复制；原 **HW** 菜单播放、暂停、seek 后恢复，最终挂载框架＋
客户端的 4K／HLG 六帧共 41,472,000 个样本与参考逐字节相等，且两级重启回退通过。
因此“当前只能切 HW+”已过时。范围仅为当前固件和客户端的 Main10 HW copy-back；
不外推所有绿屏均已修复，也不以十转八或菜单改名充当修复。
见[系统专项](decode-p010-pool-080.zh-CN.md)及[最终客户端验收](validation/lsposed-p010-036.json)。

## 当前按项梳理

- 实际游戏目标已指定为《传奇四叶草》和 Eden；沿用原画质、分辨率、场景及运行设置。Eden原驱动帧率对照由用户操作，记录时区分应用内“系统驱动”（当前g18）与出厂g13，不混用作同一基线。
- 采用门槛：出现明确性能回退就撤销，或调整后重新证明净收益；极端负载改善不能掩盖常见负载变差，不以多一个实验开关代替该门槛。
- 当前执行范围为全部可升级组件；G1/G2保留现有g18及已验收分项，等待真实游戏对照，不继续把未验证的g25优先做成升级。并行梳理P1/P2/P3中有具体根因和上游修复依据的候选，再按可落地收益选择，不让高风险驱动候选阻塞其他方向。
- G1：已核对现有配对及上游历史。g21/g22/g24/g25 KMD 升级必须连同匹配用户态和 CSF 核对，未直接替换。已回移 fence 修复不算性能进展；不适用的 quick-reset 路径和禁用 tiler reclaim 未拿来充数。
- G1最新核对：Khadas Android14的[44ed4693453c](https://github.com/khadas/android_vendor_rockchip_common/commit/44ed4693453ce374713d2f4b5dfbfb1a987e3038)为Nomad Sculpt问题替换64位库；该库SHA256与本机g18完全相同，不是可再计一次的新升级。同一快照32位库实际为g25；此前[4bf9018882e3](https://github.com/khadas/android_vendor_rockchip_common/commit/4bf9018882e3ec9c7ccc22380a38c1beadf70865)的两ABI g25依赖allocator-V2/common-V4，不能把仓库最新快照直接当作API32配套。目标两游戏均为64位，因此只换新32位库不满足其目标。g25内核仅完成初步离线编译筛查，尚未完成模块ABI、实机稳定及收益验收，当前不继续优先推进。
- G1 已排除：CSF调度tick从100ms缩短为20ms，虽减少8后台客户端的长等待，但2后台客户端P95由约6.8ms升至44ms。用户明确要求去掉；已撤销模块集成、开关和候选交付，恢复原100ms。此结果不计为性能升级，后续不再重复此方案；与既有20ms DVFS轮询无关。
- P5：RGA 用户态虽为 1.10.6，实际内建内核为 1.2.15；P010 等新格式的枚举出现不等于当前驱动可用，尚未启用新转换。

## 下一轮执行取舍

P4 的固定刷新策略有已复现的兼容性原因，并非可直接撤销的性能限制。
用户明确要求：修复并验证 ExoPlayer 刷新率切换报错／黑屏，或不修改 Frame Guard。
临时范围设置验证已恢复原来的 match=0、min=60、peak=60，没有集成自动匹配或开启 SurfaceFlinger 内容检测。

| 方向 | 下一步实际工作 | 采用条件 |
| --- | --- | --- |
| P1/P2缓冲与显示 | 继续找当前仍有的完整帧转换／拷贝和错误CLIENT合成；0.7.6已消除的路径不重做 | 同画质、位深、分辨率和帧数下有净收益；必要转换不得强行删除 |
| P3解码队列 | 对照仍未包含的EOS/pending、丢弃标记及分配修复，先定位真实未完成work或取帧卡住的根因 | 不通过提前报完成、丢有效帧或放松fence处理取得改善；新修复单独验收 |
| G1/G2实际游戏 | 《传奇四叶草》、Eden同场景帧率／帧时间与稳定性；保持g18及现有已验收设置 | 有升有降先撤销或调整，不用合成负载替代实际游戏结论 |
| RGA/VPU内核 | 已核对原厂CONFIG_ROCKCHIP_MULTI_RGA、MPP_SERVICE、RKVDEC/RKVDEC2均为内建y；逐项检查源码、接口、启动替代与原boot恢复 | 可以升级但尚无可直接替换的原厂.ko；需先证明兼容和回退，不能照搬Mali交接 |
| VOP2/HDMI/DMC | 原厂VOP2、DW_HDMI、DMC_DEVFREQ亦为内建y；按实际显示／带宽瓶颈选具体修复 | 对应板级ABI、设备树、启动依赖和恢复能力成立才推进，不以整套别板内核替代 |

P3的上游`7792393ed08e`针对RGA2的4GiB分配；当前旧基线没有同一`hasRga2`接口，
不能只加一个usage位就认定本机适用。`2dc3fb88fa2e`会在累积pending时改变工作流，
必须排除正常深重排序的误触发；`e577f69d04d7`剩余EOS清理已独立复现并集成0.7.7：HEVC AUD=8时实际EOS后未归还输入8→0，真实60帧和PTS保留，十位/HDR及两级重启回退通过，详见[专项记录](decode-hevc-eos-077.zh-CN.md)。
前两项仍是候选筛查结果，未写入设备或当作已修复；EOS分项的有限通过不代表历史UHD/HLG超时已解决。

每项先核对已包含内容和依赖，再形成对应模块候选；验证只围绕新改动的收益、
兼容性、十位/HDR 和回退。实机未通过的候选不标为已完成或可发布。


P1/P5 新增局部交付：[0.7.8 必需 P010 解包](decode-p010-pairs-078.zh-CN.md)为本地 NEON 优化，保留全部十位样本、原拷贝及同步，实机限定纹理路径服务 CPU 用时减少约 18.7%，两级重启回退通过。不是 libyuv/RGA/完整 SDK 或游戏 FPS 升级。显式 P010→CLIENT→SDR HDMI 的既有 HDR 限制在开/关两态一致，未声称解决；原生 NV15 的新版本 HDR 验证单列。Frame Guard 保持原样。

## 三项故障的根修复跟进（2026-10-19）

| 目标 | 已集成／新增证据 | 尚缺什么 |
| --- | --- | --- |
| HW/Main10 绿屏 | Decode Stack `p010_copy_pool` 修正框架字节布局及缓冲容量；现有 LSPosed test6 补 P010 复制映射。原 HW 菜单播放、暂停／快进／恢复、最终客户端的 4K／HLG 全样本及两级回退已验收 | 仅限定当前 Main10 HW 复制路径，不外推到其他绿屏；见[专项记录](decode-p010-pool-080.zh-CN.md) |
| Exo 切帧率 BadConfig／黑屏 | HWC 模式表选择性修复保留；新找到 HDMI 换时序产生假 extcon 断开→音频 NOISY→Exo 误暂停→反复切时序的链路，Decode Stack 0.8.2-test1 新增独立 `hdmi_audio_guard` | 新候选首次播放／暂停恢复未再出现队列 fatal，真实拔插已保留；精确 seek 仍未通过，真正 24↔60Hz 换时序短黑仍在。Frame Guard 保留。见[本轮边界](decode-hdmi-event-082.zh-CN.md) |
| 显式 P010 → 完整 HDMI HDR | test7 的 PQ／HLG 已有真实 4K AB30 AFBC 扫描、十位 SF 目标读回、完整 PQ 元数据、SDR UI／虚拟显示、退出及两级重启回退验收 | 真实 RGB 相对灰阶参考最大偏差 3／3／4 码值，尚非逐码精确／完整校色验收；限定当前固件及路径，保留默认关闭和独立回退。见[当前记录](decode-physical-hdr-081.zh-CN.md) |

三个目标分别验收；不把启动、隔离探针、HDMI 十位标签或旧版结果当作新版本完整通过。

## 当前尚未完成的部分

| 类别 | 尚未完成 | 下一步判据 |
| --- | --- | --- |
| Exo 刷新切换 | 应用专用候选已撤回；跨应用重复换时序与真正 HDMI 重锁／QMS 待修 | Frame Guard 保持关闭；实际内容帧率、退出清除和可重启回退分别验证 |
| Exo 定点跳转 | 多级 SeekHead 找到真实 Cues，原入口用户快进已正常；分项回退、网络中断／其他片源和长期验收待补 | 保留原生 Cues/SeekMap；不能以单片源通过宣称所有容器已修复 |
| Codec2 生命周期 | 原播放器 UHD pacing、其他历史 HLG seek/EOS 超时 | 找到具体未归还 work／缓冲依赖后单项修复；不延长 watchdog |
| HDR 色彩 | 当前真实十位 HDR 链路已通；灰阶参考 RGB 最大仍差 3／3／4 码值，完整色度校准未完成 | 保留全部十位样本及 PQ／HLG，不把十位标签当校色通过 |
| 游戏兼容与收益 | Eden geometryStreams 缺失；NCE/JIT 卡住未证明是 GPU 根因；两款实际游戏持续帧时间、温度功耗收益待验收 | g18 保持；不恢复已否决的 CSF tick20ms，不以合成负载冒充游戏结果 |
| 整组件升级 | 全新版 Composer/HWC、VOP2/HDMI/VPU/DMC 内核尚未交付 | 每项需兼容、板级依赖、可靠启动回退及实际收益；不归类为已证实故障 |

HW/Main10 限定路径已有修复；其他绿屏需具体复现。以上未完成项与已验收局部修复分开计，
不把诊断、版本号或关闭保护本身算作升级。
