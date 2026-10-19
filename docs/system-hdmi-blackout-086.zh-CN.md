当前正式版（2026-10-20）：Decode Stack 0.8.6／Mali Global 0.4.2。test20 的系统图层帧率保留已实际命中（0→24Hz 保留请求），用户确认原 Exo 场景可以了，并授权整理发布。临时追踪已关闭；Auto/HDR、Frame Guard 关闭、g18/KMD/DVFS、双 ABI Zygisk/LSPosed 及原 boot 均保留。其他片源帧率、所有接收端和完整 HDR 校准不由这次验收推定通过。见 [帧率保留记录](decode-refresh-session-086.zh-CN.md)。

以下为此前阶段记录。

# 系统 HDMI 短黑：当前根因边界与后续适配

最新补充：用户明确原固件未实装 QMS 时已有短黑，群内锁帧仅为规避。test18 END=0 直连对照相同或更差，已实际回退 test17＋Mali test19 并验证重启健康，Frame Guard 保持关闭。接续优先核对原有帧率请求、系统选模和 HWC 分流，区分原始触发与 QMS 未消黑；见 [本轮联网核对](exo-original-refresh-blackout-research-086.zh-CN.md)。未找到可直接套用的新通用补丁，短黑仍未修复。

最新增量：用户本轮确认暂停／恢复均黑。限时平面／vblank 采集抓到整数 STABLE 最终确认迟到导致的两次 `gate93` 恢复，[test16 修复候选](decode-qms-late-latch-086.zh-CN.md) 已安装重启并通过启动检查，用户实屏反馈有所改善但仍有整幅短黑；对应新增 14 次切换零错误、无重建、平面结构未变。新增分支本轮尚未触发，不宣称已完成故障现场硬件验收。此前无重建仍短黑的情形继续保留。

## 直连 Auto 接收端采集：采样期间未见 TMDS 失锁，短黑根因仍未确认

有时限的 SCDC 工具已实际运行约 60 秒并停止，完成 591 次普通状态读取，时钟及三通道锁定低四位始终为 `0xf`。用户在窗口内回复已操作；本次回复未再次单独描述黑屏，不能伪称取得逐次黑屏时间戳。QMS 累计完成序号 26，错误为 0；采样窗口内保留了序号 7–20 的 56 条边界记录。记录上限达到每启动 96 条后，后续序号不再有完整边界日志，不把它们虚称为同等覆盖。

稳定状态的 VTEM TFR 码 2／10 分别对应 VOP vtotal 5625／2250，以 594MHz／htotal 4400 计算为 24／60Hz，宣告值与实际时序相符。参考 [独立上游 RFC 帧率表](https://lists.openwall.net/linux-kernel/2026/09/21/2415)。这些证据暂不支持持续 TMDS 失锁或错误目标帧率解释，但不能排除短于 100ms 的瞬态，也不能替代实际接收的 VTEM／面板静音观察。

又核对了 [通用 VTEM 打包 RFC](https://lkml.iu.edu/2609.2/14407.html) 与 [QP 寄存器映射 RFC](https://lists.openwall.net/linux-kernel/2026/09/21/2213)：通用报文字节不能直接逐字写入 QP 寄存器，组织标识等由硬件提供的字段在 QP 映射中被跳过；没有据此向保留位乱写。通用 RFC 使用 END=0，与此前矩阵路径的无改善单项试验相关；本轮未再次安装该改动，当前保留已回退的厂商 END=1。没有将 RFC 或采样无错计为修复成功。

最新直连 Auto 结果：QMS／GPU 十位合成已实际启用；用户更正为 Auto 仍短黑。现有记录确认 6 次真实 QMS 切换完成、零帧错误，未观察到原厂完整重建、NOISY 广播或边界 GCP／HDCP2 改变。先前固定 4K60 的“无短黑”不能作为 QMS 通过；随后 Auto 保存键和原生刷新范围均已核实。矩阵不是必要触发条件。接收端瞬态 SCDC 采集已准备，诊断与实屏结论分开。

## 最新进展：直连自动启动／声音通过，短黑仍未修复

EDID 分段读取修复已实际集成并加载；完整 EDID 恢复。随后执行原厂完整显示重建，用户确认直连 HDMI1 有信号，SCDC 时钟与三通道锁定；输出为原厂协商的 4K60 YUV4208 SDR，不能计为 HDR／QMS 完成。用户随后反馈 Exo 仍短黑、自动暂停和无声；日志有连续假拔插、NOISY 广播及 HDMI 音频路由重建。已补齐开机一次原厂初始化，并恢复已有系统层假拔插过滤；真实启动检查通过，用户反馈“只有短黑了，其他都正常”。本次 QMS owner／端点未加载，下一次启动保存选择为开启不能代替实际启用。详见 [直连修复记录](decode-ddc-direct-hdmi-086.zh-CN.md)。下文“尚未安装”描述为本轮前的定位快照。

## 直连 HDMI1 无信号：发现原厂 E-DDC 分段读取缺陷（尚未安装修复）

用户更正：此前 test14／test15 等实屏试验经过 HDMI 矩阵；现改为盒子直连 LG C5 42 寸 HDMI1。电视只读 API 已确认前台 HDMI1，用户仍反馈无信号。此前结果保留为矩阵链路结果，不能视为直连 C5 的 QMS 互通验收，也不能仅因有矩阵就断定矩阵是短黑根因。

本次只读检查发现：原生 EDID 属性和直接经 HDMI DDC 的四块读取完全一致，块 2 重复块 0、块 3 重复块 1。虽然每块校验和均正确，但块映射声明应有的 CTA／DisplayID 扩展缺失，驱动当前报告 DVI，输出为 4K60 RGB8。一次 SCDC 读取没有报告时钟／通道锁定；这不是 QMS 确认接口，单次值也不能替代有效 HDMI 基线。

原始 Image 的 QP `dw_hdmi_i2c_xfer` 反汇编确认两处缺陷：写 `I2CM_SEG_PTR` 时没有将分段编号左移 7 位；读取命令固定为普通读，未按 `is_segment` 使用扩展读。它们与上游记录的 `cdc0984c90dc`（`drm/bridge: synopsys: dw-hdmi-qp: Support read ext block edid`）相对应。参考 [原开发者邮件及两处修复说明](https://lkml.iu.edu/2406.0/06496.html)。这是 EDID 分段读取异常的明确代码证据，恢复直连画面仍需修复后实测，不能直接宣称历史短黑已根治。

当前启动已回到原厂 GPU 内核，QMS 和系统设置修复的配置均为关闭；与此前已验证的 test14 启动状态不同。冷启动日志包含 QMS 绑定资格失败及后续未确认启动回退，尚未把重启触发者或全部配置变化归因。此次没有修改这些配置、加载修复内核、写 boot 或重启；Frame Guard 保持 disable／paused。

兼容性表可按 EDID 厂商／产品标识及异常特征选择已验证规则。但这处首先应补通用 E-DDC 读取修复，避免仅给 C5 伪造 HDMI／HDR／QMS 能力。修复须集成 Decode Stack，独立启停、精确内核匹配及重启回退；在读到真实完整扩展、HDMI 输出恢复并经用户实屏验收前保持未交付状态。

## 新诊断方向：电视侧事件与 SCDC（只读核对，未修改驱动）

接收端为 LG C5 42 寸；连接拓扑已由用户更正为此前经过矩阵，当前直连 HDMI1。已通过 webOS TLS SSAP 连接及用户在电视上的配对，客户端仅申请 READ 权限；实测可读取并订阅当前 HDMI 前台应用、输入连接标记、画面模式。固件／电源读取被拒绝，旧版 `dimensionInfo` 查询在本机不支持。这些成功接口不含原始 VTEM、接收时钟锁定、QMS 确认或面板静音，不能把配对成功当成已取得完整 HDMI 诊断。没有使用 ColorControl 的通知回调执行内部 Luna 服务方式，也没有更改电视设置或操作播放器。

下一优先项是 [SCDC 接收端状态](https://lists.infradead.org/pipermail/linux-arm-kernel/2026-July/1154094.html)：盒子确实暴露了原厂 HDMI DDC 的 i2c-dev，可读取电视时钟／通道锁定和扰码状态，补足源端 PHY 检查。受限读取工具仅访问三个普通状态／配置读取位置；不写 SCDC 设置、不读取会清零的 CED 计数。直连无信号时已完成一次读取，尚未建立正常输出基线或覆盖短黑操作，不能把该读取计为 QMS 验收。

VOP `summary/state` 可作图层／缓冲与错误关联。CRC 文件存在但原厂回调仅调用 Analogix eDP CRC，源列表为空，不能直接用作 HDMI 画面校验。上游另有 VTEM 更新可能混包的自动评审疑点，但本地 34 条普通过渡记录没有一次同时改变两个可变字段，适用性需降级，不能据此盲目停发／重开 VTEM。

结构化来源、可诊断范围与限制见 [新方向记录](validation/qms-new-diagnostic-directions-086.json)。本轮没有安装新驱动或重启，短黑仍未根治。

## test15：四帧提前通知单项对照（无改善，已实机回退）

用户反馈“没有改善”。内核记录确认四帧通知实际执行，6 次切换完成且零帧错误；这些切换早于新一轮 logcat／netlink 采集窗口，保留的内核记录可证明窗口执行，不能伪称该实时采集覆盖了随后回复的操作。四帧延迟已从源码撤回；设备已从完整备份恢复 Decode test14＋Mali test16，实际回退重启确认 QMS、g18／KMD／DVFS、双 ABI Zygisk Next／LSPosed、Auto／HDR、组件及模块标记正常，boot 未变。

test14 实屏反馈为“播放、暂停、步进都黑”。本轮完整覆盖操作：8 次切换完成、零帧检查错误、无观察到的拔插或 VOP／PHY 重建；34 条切换边界的 GCP 控制均为 0、HDCP2 bypass 均为 1、GPIO 均为 0，所有采样成功。暂未支持源端 AVMUTE 或 HDCP2 重认证解释，但边界采样不能排除全部瞬态或电视内部静音。

单项试验 `0.8.6-qms-test15` 曾将目标帧率提前通知从 1 个完整帧延长为 4 个，参考 [CoreELEC 的四帧准备窗口](https://github.com/CoreELEC/common_drivers/blob/8a89a9aae1cb7c7993cc20b943491af435b31801/drivers/media/vout/hdmitx21/hdmi_tx_vrr.c)。保留本地“实际锁存后才宣告稳定”、真实帧检查和所有格式／fence 保护，不移植 Amlogic 寄存器，也不强开游戏 VRR。这是接收端准备时间的兼容性假设，不是已证实的 HDMI 违例或 LG 要求。

代价为每次切换增加 3 个旧帧率周期，60Hz 下约 50ms、24Hz 下约 125ms；没有实屏改善就撤回，不作为默认性能升级。详情见 [test15 验证记录](validation/qms-notice-window-086.json)。

## test14：修正稳定检查误判，补齐切换边界证据（启动正常，短黑未改善）

已安装并重启 Decode `0.8.6-qms-test14`，Mali `0.4.2-qms-test16` 仅增加对应精确配套记录，GPU 仍为 g18。50 项 Android 隔离寄存器／生命周期用例、15 项实际配套检查及原始 Image 的 74 项导入 CRC 检查通过；实际启动确认 QMS、Mali／KMD／DVFS、双 ABI Zygisk Next／LSPosed 正常，Auto、HDR、组件选择及模块标记保留，boot 不变。用户已确认播放／暂停／步进仍短黑，未通过无黑验收。

test13 的稳定检查曾因 scheduler 不符进入原厂显示重建，但旧日志无法区分 VTEM 被清除和其他原厂数据包改变。本轮修正该检查中过宽的责任范围：QMS 不再要求其他数据包在两次独立事务之间保持旧值；实际提交前后的原字段保护、完整 VTEM、时序／影子、供电和代际检查仍保留。它修正一条可能导致重建的错误判断，不代表 test12 那种“无重建仍短黑”已经修好。

同时撤回用户反馈“完全相同”的 END=0 改动。通知、换时序、M_CONST 顺序与厂商实现基本一致，尚未发现可直接回移的顺序差异。厂商 VOP 的 `rockchip_connector_update_vfp_for_vrr()` 仅调用可选连接器回调，所核对的 HDMI QP host 未安装这个回调，不能把缺少该函数调用本身当作本机漏写 HDMI 寄存器的证据。

为继续定位无重建短黑，只在实际切换事务前后读取 GCP 静音控制、HDCP2 配置／GPIO 和 VTEM 字段，并关联真实帧号及锁存时序。每次开机最多 96 条，不新增稳定播放采样或工作线程。读取位置已核对原厂 QP 寄存器表，该表也加入运行时完整指纹检查；采样失败单独记录，不改变原提交判定。此证据仍不是电视确认，且可能遗漏两个采样点之间的瞬态。

验收保持 Auto、HDR 设置修复、Frame Guard 禁用并暂停；播放由用户操作，只读日志，不截图。短黑是否消失仍以 HDMI 实屏反馈为准。详情见 [test14 记录](validation/qms-scheduler-boundary-086.json)。

## 检索补充：本地完成八次 QMS 切换但实屏仍黑

“会话未退出”在本项目中指 sidecar 的 ACTIVE、generation／cookie 和本地 ticket 没有中断；不是电视返回的协议成功确认。八次完成证明本地提交／检查成功，不能证明八次 HDMI 线端 VTEM 均在正确帧提前送达，或电视没有短暂静音。此次检索未找到能直接套用到本机固件和 LG 的已确认通用补丁。

| 核对顺序 | 资料支持的机制 | 本机边界及下一项证据 |
| --- | --- | --- |
| 1 | [Synopsys QMS 说明](https://www.synopsys.com/blogs/chip-design/hdmi-quick-media-switching.html) 要求源端通过 VTEM 提前告知目标帧率，显示器据此准备转换；QMS 开启本身不替代双方时序配合 | 优先核对 Next_TFR 通知、真正发送窗口、VOP 新时序锁存与 M_CONST 宣告稳定的相对顺序。当前核对了寄存器和本地帧，未证明每帧的线端发送。此项是定位假设，不能声称已发现具体违反协议的步骤 |
| 2 | [Rockchip f26d0d9c](https://github.com/rockchip-linux/kernel/commit/f26d0d9c77e0a72e5c5137da620aba9938e4e0c4) 修复 HDCP bypass 不在 vblank 及正确认证阶段更改导致的闪黑 | 不重建模式也可能因输出控制短黑；需关联 AVMUTE／HDCP 状态。现有采样未记录其变化，既不能确认也不能以日志缺项排除。不得将提交存在当作本固件缺失或直接关闭 HDCP 的依据 |
| 3 | [Apple 官方 QMS 优化](https://support.apple.com/en-ie/guide/tv/atvb553e7afe/27/tvos/27) 明确尽量在各帧率使用相同 chroma；[HDMI LA](https://www.hdmi.org/spec21sub/quickmediaswitching) 描述的无黑条件是保持分辨率且仅变帧率 | 本机 test12 的格式／色深／EOTF 在软件原生身份检查中保持一致，因此该项比瞬态信令问题靠后；仍不能把“同为4K”当作所有输出属性相同。不以降低位深或永久限制 HDR 作为修复 |
| 4 | [Firecore 用户一手报告](https://community.firecore.com/t/qms-match-content-adjustable-delay-fix/42519/30) 描述 QMS 已瞬时切换但应用仍等待黑场；这是报告，非已确认的 Exo 根因 | 应区分播放器遮罩／停帧与整屏链路黑场。此前 HDMI 实屏黑而投屏正常更偏向后者，故不先替换 SurfaceView，也不直接照搬 Infuse 的等待时长假设 |

NVIDIA [后续评审](https://lkml.iu.edu/2609.3/13407.html) 进一步区分源端固定输出和接收端保持 VRR／QMS 模式。它支持避免不必要的电视端模式切换，但不意味着私有 ACTIVE 状态已经证明接收端实际持续保持。END=0 单字段试验没有改善，不能继续当成已找到的根因。

当前优先核对的是“信令何时实际发出、何时实际换时序、是否被输出静音”，保持 Exo 清除／恢复内容帧率为触发线索。此次只检索并审查已有源码和日志，没有安装新候选、重启、截图或操作播放器。

## 用户补充“只有 Exo 触发”后的上游核对

找到现象非常接近的 [ExoPlayer #7921](https://github.com/google/ExoPlayer/issues/7921)：播放／暂停导致 50／60Hz 切换，电视短黑但 scrcpy 正常。维护者归为设备平台问题，未给出通用播放器修复。与本机的 24／60Hz 关联相似，不代表两设备根因已证明相同。

旧 Exo 与当前 Media3 的 `VideoFrameReleaseHelper` 均在 renderer stop 时清除 Surface 帧率提示，在 start 时重设内容帧率。这个路径不要求销毁 Surface。因此“只有 Exo 触发”可以由播放器的投票生命周期解释，不能据此直接排除 HDMI 输出，也不能反过来忽略播放器调用。本机 test12 现有日志只在解码器初始化出现 SurfaceUtils 重连，后续八次显示模式变化没有对应的重复重连记录；尚未直接截获每次 Surface.setFrameRate 参数。

[Media3 #2941](https://github.com/androidx/media/issues/2941) 则讨论换媒体项后的末帧保持／解码器复用及帧率卡顿，其旧 API／特定厂商 workaround 不能直接套到本机 API32 暂停场景。[官方 Surface 指南](https://developer.android.com/media/media3/ui/surface) 仍优先推荐 SurfaceView，包含时序、HDR、功耗和 Android TV 全分辨率优势；不采用全局 TextureView 替换。

test13 END=0 已真实启动，但用户实屏反馈“完全相同”，不能交付为修复；源码已撤回 END 位改动，设备要到下次重启才更换。另取得稳态 diagnostic103 后完整重建的证据；当时未记录调度器具体位，尚需区分 VTEM 被关闭和其他包所有者改变开关。缩小只读检查归属的候选已编译且通过 48 项注入寄存器检查，尚未安装。继续优先关联 Exo 清除／恢复帧率投票、真实无重建变速与黑场，不把捕获的一个恢复故障解释成所有短黑。

详见 [Exo 触发路径核对](validation/exo-qms-trigger-research-086.json)。

## test12：整数过渡通知的跨帧观察候选已启动

2026-10-19 按上节审计顺序先修改第一项。Decode 0.8.6-qms-test12 已安装并真实重启，Mali 0.4.2-qms-test14 只增加新 QMS 模块／加载器的精确配套识别，仍使用 g18 与原有 KMD／DVFS。新旧完整配套均可识别，混搭拒绝；boot、HWC、系统设置 JAR／OAT 及原分项配置保持一致。已保存两模块完整私有备份，分项关闭及整包停用仍沿用原重启恢复机制。

`rkqms_session_repeatable()` 只接受尚在过渡、没有待完成 ticket latch、之前的硬件提交已核对完成的整数时序。原生 flush 窗口内若观察跨过多帧，先只读核对当前完整 VTEM（包括 M_CONST 和已通知的 Next_TFR）、实际及 shadow 时序、包调度器、原生锁、电源与链路 epoch；通过才以当前真实帧继续一次状态动作。不会补造中间帧、按差值跳过通知等待或完成 ticket。分数序列、最终等待 latch、原生提交完成时的 +1 验证及 fence 保留。硬件不符、时间不前进或倒退计数仍拒绝。

新计数 `owner_notice_reanchors` 只在完整原生提交核对成功后增加；故障日志增加出错前 phase／awaiting 状态。稳态不增加逐帧日志，不改变 100ms 只读观察周期。VTEM 字段及 HWC 生命周期本轮不混改，仍留作后续独立定位。

实际 C 状态／HWC 策略 90 项、原生引擎发布 33 项、设备上注入对象的寄存器／电源检查 45 项、控制器 19／端点 11 项、精确配套 15 项通过。它们分别验证策略及故障拒绝，不能代替显示硬件验收。

本次真实启动确认 Decode／QMS、Mali 用户态／首次绑定内核／DVFS、双 ABI Zygisk Next／LSPosed 正常，模块启停标记相同，设置修复已 qualified，保存分辨率仍 Auto、HDR 菜单仍开；Frame Guard disabled＋paused。启动阶段 frame_errors=0 不代表播放问题已修复。已启动有时限的只读日志／连接事件采集，等待用户执行原 Exo 播放／暂停／跳段并观察 HDMI 实屏。本轮没有截图、录屏、输入事件或助手操作播放器。

详见 [test12 验证记录](validation/qms-notice-reanchor-086.json)。第二项 QMS 生命周期仍待关联真实播放证据，不能将第一项候选部署计为全部短黑解决。

用户完成 test12 原 Exo 操作后反馈播放、暂停、步进均短黑。同期八次逻辑 24／60Hz 切换均完成，端点 serial=completed=8，generation／cookie 不变，最后仍为 ACTIVE@60；frame_errors=0，notice_reanchors=0，无新增 VOP／PHY 重建或 DRM／extcon 事件。因此不能宣布第一项实机场景触发通过，更不能把此轮黑场归为完整重建或退出 QMS。已有证据支持继续第三项持续 VTEM 报文对照，不为已经保留 QMS 的基础帧率返回再加无效开关。下一候选只将持续数据集 END 改为 0；NEW／包 FIRST-LAST／Next_TFR／M_CONST 及其时序不混改。

## 2026-10-19：其他系统的实际 QMS 调用对照

最新状态覆盖下文历史阶段：Decode test11 的系统设置修复已通过两次真实启动，用户确认 Auto／修复开关正常，并在 HDMI 实屏看到 QMS-VRR；Auto 下仍短黑，固定 4K60 不短黑。QMS 运行核心仍是 test9。Frame Guard 保持 disabled＋paused。以下为源码审计，未因此改动设备、报文或同步检查，不能计为短黑修复。

### Rockchip 厂商 6.1：先通知，再改变 VOP 时序

核对固定快照 [77168c8d5ab8](https://github.com/rockchip-linux/kernel/tree/77168c8d5ab82399f65a80e9f807b50ba37cf483)，追踪 `dw_hdmi_qp-rockchip.c`、`rockchip_drm_vop2.c` 与 `dw-hdmi-qp.c`。

上层设置 connector 的 `next_tfr`，HDMI 状态机先发送基础帧率且 M_CONST=1，等待两次真实 vblank，再发送 M_CONST=0 并等待。变速时先通知当前／目标帧率，再由 `refresh_rate_ready_to_change` 放行 VOP2 的 VFP/vtotal 更新。稳定后置 M_CONST=1；分数帧率由 VOP2 中断路径维护行数序列。显式关闭才走恢复基础时序和停发 VTEM 的独立流程。

`rockchip_hdmi_wait_vsync()` 等待计数改变，不要求软件前后观察恰好相差一帧。当前快照还把成功等待结果归一化为 0。它不是我们现有“每步完整原厂 atomic carrier”的相同执行模型，不能直接删掉本地提交完成／fence 检查来模仿。

### Amlogic／CoreELEC：无缝变速与完整模式设置分开

核对 [CoreELEC 8a89a9aae1cb](https://github.com/CoreELEC/common_drivers/tree/8a89a9aae1cb7c7993cc20b943491af435b31801) 的 `drivers/drm/meson_hdmi.c` 和 [hdmi_tx_vrr.c](https://github.com/CoreELEC/common_drivers/blob/8a89a9aae1cb7c7993cc20b943491af435b31801/drivers/media/vout/hdmitx21/hdmi_tx_vrr.c)，并对照 Khadas `3a11a86a02e759fc57fc79410215f7c0c3a0d8e0`。

DRM 根据分辨率、色彩属性、基础模式等判断是否可 seamless。满足时只调用 `set_vframe_rate_hint()` 并提前返回，跳过完整 HDMI modeset；disable 回调也跳过关闭。发送端准备 Next_TFR／M_CONST，先在四次 vsync 中发送过渡通知，再更新行数；支持按接收端 MDELTA 能力分步接近目标。逐帧部分在 vsync 中断执行，不能把这一实现的时限直接套到本地工作线程上。

这份源码并非永远保持 QMS：明确收到零帧率提示时会清除 EMP 的元数据及旧配置。因此也不能笼统声称“其他系统都不会关 QMS”。

### 主线 RFC 与驱动评审：区分源端调速和电视端模式

2026-09 的 [qms_enabled 接口提案](https://lists.openwall.net/linux-kernel/2026/09/21/2590) 将 connector QMS 选择与 CRTC 有理数目标帧率配合使用；它仍是 RFC，不能当作已发布的成熟替换驱动。

[NVIDIA 开发者的评审](https://lists.openwall.net/linux-kernel/2026/09/25/158) 明确指出，电视端 VRR 模式切换通常会引起 blanking，应与源端是否正在变速分开控制。[后续讨论](https://lists.openwall.net/linux-kernel/2026/09/25/736) 建议把链路策略显式交给用户态，也讨论了功耗代价。这支持优先审查模式生命周期，尚不能证明本机每次暂停都退出 QMS。

Android 官方还[建议暂停且 Surface 仍可见时清除内容帧率提示](https://developer.android.com/media/optimize/performance/frame-rate)。因此暂停后系统选择 60Hz 本身可能是正常行为；系统修复应让兼容的 24↔60Hz 在 QMS 内完成，不能靠拦截所有播放器清除请求代替。

### 具体上游修复及本机适用性

| 提交／差异 | 实际问题 | 本机结论 |
| --- | --- | --- |
| [Amlogic 4e0263ccc16a](https://github.com/CoreELEC/common_drivers/commit/4e0263ccc16a7425028e79f35222bbf011136936) | 换算采用四舍五入后，23.976Hz 的百分之一 Hz 查表值仍是 2397，修为 2398，解决匹配失败无画面 | 本地 `rk_qms_tfr()` 使用精确分子／分母，保留 23.976 与 24 区别，不复制该数值补丁 |
| [Rockchip cb523b18c40c](https://github.com/rockchip-linux/kernel/commit/cb523b18c40c46b39834f98e9728a319c0735cd5) | QMS 配置查表空哨兵解引用崩溃 | 本地 timing 表按数组长度遍历且检查找不到的情况，无同类直接补丁 |
| VTEM 数据集 END 位 | 厂商 Rockchip 当前仍设 END=1；CoreELEC 设 END=0，NEW 位也有差别 | 本地沿用 Rockchip；存在实现差异，不等于已确认错误。须单独对照，不能与调度修改混成一个候选 |

### 与本机故障对应的修复顺序

1. **软件观察跨帧导致的完整恢复。** test11 私有日志已记录 gate93：真实计数从 6339 到 6341，时间差约 83.33ms，与 24Hz 的两帧相符；随后 VOP2 disable／enable 和 PHY 重建。错误发生前的阶段被现有 fault 路径覆盖，尚不能称为某个确定阶段的故障。修复设计只允许在确认硬件持续重复相同时序／报文、没有待锁存提交的整数阶段重新建立观察点；不补造帧、不跳过过渡通知、不放宽分数序列或提交完成验证。先保留原阶段证据再决定允许范围。
2. **暂停／恢复／退出的 QMS 生命周期。** 对同分辨率、同色深／HDR／链路的合法目标，返回基础 60Hz 仍可留在 QMS 中；真正分辨率／链路变化或用户关闭则完整退出。本地普通同组 `rk_qms_switch()` 已不会因为目标为基础帧率而强制 restore，仍需关联触发 restore 的调用原因；不能把“已有这一分支”重复计为新修复。
3. **没有显示重建仍短黑的路径。** 早期实屏观察存在短黑而无新增 VOP／PHY 重建，不能用第一项解释全部故障。再分别核对 VTEM 连续性／字段、AVMUTE、HDR／色深和 HDCP；每次只改变一类行为。

稳态整数时序由硬件重复，原生实现无需软件每帧做完整 atomic 提交；分数序列仍需要按硬件帧维护。本地 test9 已有整数稳态保持及 100ms 只读核对，这不是本轮新增收益。实际 CPU 开销尚未测量，不报性能数字。

本轮源码快照已固定提交并核对文件 SHA-256。下载源码、接口页面及原始日志留在私有 dist；公开验证摘要见 [QMS 上游审计](validation/qms-upstream-audit-086.json)。

## 以下为历史阶段记录

用户要求跨 ExoPlayer 应用修复，不能用 Yamby 专用 LSPosed 帧率钩子交付。黑场只在 HDMI 实际显示器可见，ADB 投屏不可见；是否消失以用户观察为准，日志用于关联，不能用日志无错误或 ADB 画面正常计通过。

## 当前已执行

已关闭并重启验证应用专用 exo_content_vote，再恢复 Decode Stack0.8.4-test3 和索引修复客户端0.3.7-test2；所有原分项选择、实际共享配置中的 exo_seekhead、原生 Surface／背压修复、双 ABI Zygisk／LSPosed 均保留。源码中撤下该短黑桥与开关，未写 boot，未改 Mali。客户端重新构建通过，8 项索引和4项裁剪测试通过；撤回不是 HDMI 修复。

快进回片头有独立证据：运行钩子装载、读取尾部真实 Cues、原入口用户快进正常。该成果保留，不与 HDMI 黑场混为一项。

Frame Guard disable 和外部 paused 标记保留，secure match_content_frame_rate=1，min／peak 未固定。禁止阶段结束自动恢复 Frame Guard。

## 系统链路证据

现有真实用户操作日志中，HWC UpdateDisplayMode 清空显示平面，VOP2 atomic_disable／atomic_enable 后在3840×2160的24Hz和60Hz模式间变换。ADB 中的已合成图像无法显示输出时序停止或电视重新锁定产生的黑场。不同日志时钟可能有偏差，不能直接用日志打印间隔推算实际黑屏时长。

这确认系统确实执行换时序；尚不能据此把所有黑场都归到同一原因。需要分别判断：重复／多余切换、必要模式切换时的 PHY／时钟重锁、同模式下的 HDR／色深／HDCP 变化。当前未交付跨应用无黑修复。

## 已核对的源码依据

- AOSP android-12.1.0_r11 的 [RefreshRateConfigs.cpp](https://android.googlesource.com/platform/frameworks/native/+/android-12.1.0_r11/services/surfaceflinger/Scheduler/RefreshRateConfigs.cpp) 在无有效 layer vote 时选择策略内最高刷新率；本机状态显示 content detection 关闭。它是清除内容投票后回到高刷的一项系统机制，不能因此直接认定修改该分支即可正确处理桌面、游戏、多视频和退出生命周期。
- Rockchip [4c229b1e1751](https://github.com/TinkerBoard-Android/rockchip-android-hardware-rk35-hwcomposer2/commit/4c229b1e17514a3a9d09a46b1716cba864850bf5) 特意在 UpdateDisplayMode 前添加全平面清空及独立 atomic commit。运行日志与该流程一致，不能拿更早 Toybrick 快照中没有清空的旧实现当作新修复，或直接删除而不验证分辨率变化、多平面及缓冲生命周期。
- Rockchip develop-5.10 的固定快照 [95a3ad83f7f9](https://github.com/rockchip-linux/kernel/tree/95a3ad83f7f90e0e672b9c99b0858b4964318667) 包含 VOP2 的部分 seamless／VRR 逻辑及 HDMI QP 的 AVMUTE／PHY 关闭路径。该快照并非本机匹配厂商源码；存在函数名称不等于本机已有完整 HDMI QMS，也不能据此直接替换内建驱动。
- 电视 EDID 已声明 QMS。现有 DRM 属性中有通用 CRTC VRR_ENABLED，但没有已验证的 HDMI QMS 端到端通道。不能把电视能力或一个通用属性当作可直接开启无黑切换的证明；同样不能断言电视不支持。

## 后续采用条件

修复落在 Decode Stack 的系统显示组件，覆盖不同播放器调用同一路径；按实际原因选择 SurfaceFlinger 模式策略、HWC 提交或可靠可回退的 HDMI／VOP2 适配。保留真实内容帧率、全部十位／HDR和 fence，不用全局锁60Hz、不吞掉清除请求或伪造 modeset 成功。系统策略必须处理 UI 恢复、真正退出、换 Surface、多应用和不同内容帧率。

只有形成精确固件／ABI 通过的系统候选后才要求新的用户操作验收；实际显示器是否黑场和必要链路证据共同记录。本轮未再请求重复播放，也未进行截图、录屏、输入事件或新显示压力负载。


## 本轮补充：厂商6.1已有成套QMS实现

此前仅提到2026年RFC的属性入口，范围不完整。Rockchip厂商6.1分支另有直接相关的实现与修复，现已读取具体提交及补丁，未安装到本机：

| 组件／提交 | 处理的问题 | 本机采用边界 |
| --- | --- | --- |
| [VOP2/DRM d0e8f558](https://github.com/rockchip-linux/kernel/commit/d0e8f558bc58984f8f4508133143cc9ca77c8b0d) ＋ [HDMI QP 5a89a1cb](https://github.com/rockchip-linux/kernel/commit/5a89a1cb8b9c3cba5666151e838d2a3d760fe0ea) | HDMI QMS-VRR：VOP2时序表及分数帧率vtotal更新，HDMI VTEM／next_tfr及启停切换状态机。后一提交的父提交即前者 | 是真正换刷新率减少重锁黑场的重点方向；需端到端适配，本机电视声明能力不等于盒子端已支持。这两份补丁不能直接构建成独立.ko或用少量属性代码代替，专用扩展适配另行验证 |
| [HDCP f26d0d9c](https://github.com/rockchip-linux/kernel/commit/f26d0d9c77e0a72e5c5137da620aba9938e4e0c4) | HDCP开关时闪黑，要求在vblank及正确认证时点改变bypass | 必须先确认黑场与HDCP切换相关；依赖内核通知链与HDCP用户态配套，不能直接改一个寄存器或延时 |
| [HDR 2eeed1b8](https://github.com/rockchip-linux/kernel/commit/2eeed1b87d7d72d1bcfc51b2c6234679b19c644a) | 非BT.2020 HDR在接收端支持YUV422时的十位无缝色彩输出选择 | 只针对相关HDR／色彩条件，不覆盖所有24↔60Hz时钟变化；需确认当前二进制是否已有等价逻辑 |
| [HWC de31070c](https://github.com/rockchip-toybrick/rk_hardware_rk35_hwcomposer2/commit/de31070c66c61ee2bd90c42c6877d26c4c5d7c24) | 标准切换模式表不一致 | 已选择性集成，不计为本轮新增，也不能声称解决物理HDMI重锁 |

另核对主线 [VP clock reset补丁](https://lists.infradead.org/pipermail/linux-rockchip/2024-November/052623.html) 和 [HDMI PHY PLL精度系列](https://lists.infradead.org/pipermail/linux-rockchip/2024-December/053381.html)。前者处理多次切模式后信号丢失／时序错误，后者处理包括23.98／29.97／59.94在内的时钟精度；均不能直接等同于每次短黑。已下载的厂商5.10参考快照本就有dclk reset及HDMI PHY PLL选择实现，不能按主线提交日期认定原厂必然缺失。

较新的 [add6496a](https://github.com/rockchip-linux/kernel/commit/add6496a8b1d7f27bf74b0258a72c5bca95ae0d9) 对特定电视反而强制完整重连以修复快速切换兼容性，不能将所有“switch fix”都作为减少黑场升级。

本轮只核对上游及依赖，没有修改设备。当前排序：先确认HDCP／HDR条件性缺陷及本机等价实现；并行审查成套QMS与当前5.10.66 BSP的适配／回退条件。HWC侧同步核对模式提交是否重复及QMS用户态接口，尚无已证明只升级HWC即可无黑的结论。公开清单不含地址、原始日志或影片。


## 按提交实施前的精确镜像门禁结果

本轮没有可交付的新增修复，未修改设备、未写入 boot，保持原快进／背压修复和 Frame Guard 关闭。以下是未通过落地门禁的证据，不是模块升级成果：

1. 从已确认的原始 boot 中提取原内核，离线恢复141,540个符号，并核对实际函数代码。原 VOP2 enable 路径已经调用 reset_control_assert／deassert，因此不能把主线“添加 VP clock reset”再次作为新修复。
2. 实际原厂内核不含 QMS 的 dw_hdmi_qp_set_qms、dw_hdmi_qp_get_next_tfr 和 rockchip_hdmi_qms_vrr_enable；运行 DRM 接口也没有 next_tfr。QMS 不是给当前 HWC 换一个库就能使用的已存在功能。
3. d0e8f558 与5a89a1cb在固定厂商5.10参考源码上的 git apply --check 均失败，涉及 Rockchip CRTC 状态和 VOP2／HDMI 上下文。它们即使人工移植后能编译，也不能据此证明与更早的原厂内核二进制 ABI 相容。
4. f26d0d9c 依赖的 dw_hdcp_register_notifier／unregister 接口在原厂镜像中缺失；现有用户操作日志未提供黑场恰由 HDCP 开关触发的证据。不能以一个 bypass 寄存器替代完整认证生命周期配套。
5. 2eeed1b8 的两行修改能匹配较新的5.10参考源码，但原厂实际 select_output 使用不同控制流。对原厂函数进行了18个离线合成结构案例，保留真实选择函数、仅为 DRM 辅助函数提供明确替身；确认该上下文差异。它不等于真实 HDR 画质／黑场验收，也不能据此宣称原厂没有相关色彩问题。

当前阻点是缺少与这台机器匹配的显示内核源码／生成 ABI，或一套已经独立证明板级兼容且可完整恢复的替代内核。现有 Mali 构建输入只覆盖此前独立驱动，不能代替 DRM／VOP2／HDMI 的内部类型、设备树和启动依赖。当前原始 boot 和冷启动框架能保存回退镜像，但备份存在不能证明新显示内核兼容。

原定系统短黑目标保留，不能用应用专用钩子、静态锁60Hz、源码套补丁成功或本报告替代模块交付。下一次继续时应从上述缺失依赖接续，不重复已经排除的 clock reset 和应用专用桥。


## 独立内核扩展路线：待验证，非已否定

用户提出通过内核注入补接口。本机运行配置已确认 MODULES／MODULE_UNLOAD／MODVERSIONS 开启，KPROBES 和 FUNCTION_TRACER 未启用，available_tracers 只有 nop；内核和模块代码均严格 RWX。drm_property_create_enum／drm_object_attach_property 为可导出符号。因此“独立 .ko 添加实现并接入现有 DRM 回调”可以作为另一条适配路线审查，不能把此前直接回移失败等同于所有扩展方式都不可能。

kallsyms_lookup_name／aarch64_insn_patch_text 在运行内核中存在，但不在已核对的模块导出表中；存在不等于普通模块可合法直接链接。当前没有证明可使用标准 kprobe／ftrace／livepatch 路径完成挂接，也不采用未知地址强写代码。

拟审查启动前的、精确固件限定的 DRM／HDMI 回调挂接与独立实现。需要证明可取得正确设备对象、内部结构与对象生命周期、锁／IRQ上下文、VOP2分数vtotal更新和HDMI VTEM状态机都成立。只添加 next_tfr 属性、只导出同名空函数不会产生 QMS；用户态还须配套。模块停用后重启回退与热卸载是不同承诺，未验证安全前不承诺可热卸载。

本次仅检查能力，没有加载新模块、修改内核代码或 boot。上述扩展路线尚未成为可安装候选。

## 专用 .ko 的实机研究结果

随后按用户要求构建并短时加载两个专用研究模块，均已卸载。它们没有 QMS实现，不是新升级包；原 Decode Stack／索引／背压修复保留，Frame Guard持续关闭，未写boot、未改Mali、未操作播放器或截图。源码与构建说明在 `kernel-probe/display-extension/` 和 `tools/build_display_probe.py`。

| 已完成门禁 | 实际结果 | 不能外推的结论 |
| --- | --- | --- |
| 模块生命周期 ABI | 发现并修正原生Linux头文件的init／exit偏移差异；最终两个模块的拒绝加载、正常初始化和非强制卸载均通过 | vermagic、module_layout CRC和总大小匹配仍不能代替字段偏移验证 |
| 原生对象与静态表 | 7张表匹配；3个connector、4个CRTC；HDMI QP与原生connector列表恰好匹配一次 | 未进行对象内回调指针切换，也未证明其并发／解绑安全 |
| 标准逐帧事件入口 | 现有DRM vblank tracepoint可经导出API注册；最终版本短时读取197次，卸载前210次，随后unregister＋SRCU／RCU完成 | 仅计数，不是无黑切换、帧时间或寄存器更新验收 |
| 清理与配置 | 两模块结束后均不在sysfs；boot标识、显示配置和Frame Guard关闭状态保持 | 被动观察可卸载不代表接管显示的功能扩展可热卸载 |

关键新增发现：未启用FUNCTION_TRACER不代表tracepoint不可用。原厂内核保留 `drm_vblank_event`、`for_each_kernel_tracepoint` 和注册／注销API，可用它继续研究帧边界，避免将共享IRQ当作已证明的入口。VOP2原处理程序会读取并清除硬件IRQ状态，后来注册的共享处理程序不能假定仍能读到同一次事件。

原厂实测的对象内字段偏移：DRM mode_config funcs=0x348；QP connector funcs=0x158、helper=0x5a0、bridge funcs=0x7b8；CRTC funcs=0x198、helper=0x1b0。只在已限定的原始固件上成立。DRM设备实际分配0x640字节、QP分配0xc78字节，探针没有按较新头文件扩大访问边界。

后续最关键的限制已经收窄：vblank tracepoint位于中断／DRM event_lock上下文，不能直接放入会睡眠的VTEM状态机，也不能假设任意寄存器写入安全。需要独立证明VOP2锁序与cfg_done、分数vtotal／VFP逐帧更新、HDMI多帧VTEM／M_CONST状态转换，以及HWC真实调用配套。旧对象不能直接扩写新结构字段；可研究模块私有状态，但其atomic state复制／回退／销毁仍待实现。功能候选应在图形服务启动前受门禁加载并保持模块存活，先以停用后重启作为回退边界，不能继承探针的热卸载承诺。

结论更新为：**专用内核模块的对象访问和被动逐帧入口已在本机验证，完整QMS及短黑修复仍待实现与HDMI实机验收。** 这条路线没有被否定，也尚未达到发布／默认启用条件。

## QMS实现阶段：控制核心与原生事务构造已完成

已新增 `decode-kernel/qms/` 的实际C实现：56组时序、逐帧状态机、VTEM事务层、EDID能力解析和原厂原子事务构造器。没有启用QMS硬件或修改已安装模块；Frame Guard关闭，保留现有索引和背压修复，不改Mali或boot。

本轮实际ARM64 C测试通过66组分数周期检查、738组切换／恢复、94,464个稳定阶段采样、15个错误／门禁案例、11个I/O故障注入及10个EDID场景。这是新增代码的离线验证，不是模拟器里看图证明HDMI无黑。上游d0e8f558有两项数据不满足精确帧率：720p60→29.97表多一个分数位；1080p120→47.95表整数vtotal少3行，已分别修正，保留其余序列和来源。

精确固件的内部ABI探针进一步在实机证明：当前基础模式可用于核心；HDMI regmap采用32位值、4字节步长及自旋锁；通过原生DRM锁／分配／复制／销毁函数，可以构造并释放一个CRTC事务，实时CRTC状态指针保持原样。新探针29项导入CRC及7张表通过，拒绝加载／正常加载／非强制卸载通过，最终无残留。没有调用atomic_check或commit，也没有读写MMIO。

当前没有完整QMS交付。寄存器写入必须进入正确的原生提交窗口，避免与普通图层的影子寄存器／cfg_done冲突；tracepoint观察入口不能直接代替它。旧VOP2表使用合并vtotal_pw，不能套用新版本字段布局；同样位号还可能对应色彩控制。尚需真实后端、挂接生命周期、HWC调用、显示启动屏障和重启回退，再做物理HDMI及HDR验收。具体代码边界与未完成项见 `decode-kernel/qms/README.md`。

## 增量实现：会话、HWC候选与启动协调

新增会话层将“接受请求”“写入完成”和“后续真实vblank确认”分开；固定大小UAPI包含generation、link cookie、串号及连接器身份，不传寄存器或指针。漏帧、时间戳回退、超时／中断和部分I/O失败不能计为切换成功。热插拔后需要原生排空与恢复确认，再使用新generation；声明UAPI并没有注册可用的内核设备。

HWC候选接入SetActiveConfig、GetActiveConfig及SetPowerMode。它仅在目标已确认、色深／EOTF／色彩空间及分辨率均保持不变时返回逻辑配置；不改原厂物理mode或伪造vsync。后端状态不明时返回失败，不能同时发普通modeset；不支持的配置须先确认恢复，随后沿用原路径。该前端编译通过，但真实内核端点和原厂vsync变速语义尚未验证。

启动候选在现有pre-exec屏障内，先检查Decode的独立选择、HWC／模块／helper哈希及本次overlay计划，再等待显示helper确认，之后才允许GPU加载和图形服务释放。显示失败只保存显示项关闭并请求重启；未确认的上次试启动在下一次跳过显示项。普通Mali构建不包含该候选阶段。**没有提供真实显示loader，没有更改boot；隔离场景通过不代表真实首次绑定／重启回退通过。**

新增实际C检查通过22个请求门禁、20个生命周期案例、25个HWC策略案例、14个帧率匹配案例，以及1个缓存私有EDID解析检查；ARM32／ARM64 UAPI布局均编译通过。盒子上的实际HWC回调隔离检查22条断言通过，启动协调17个受限文件系统案例通过；测试没有打开显示设备或操作播放器。Decode打包门禁已实际拒绝只有前端的候选，未产出可安装QMS整包。

剩余主要工作仍是VOP2原生提交窗口／挂接生命周期、真实VOP2／VTEM逐帧后端、内核控制设备及取消恢复、真实boot／loader集成，之后才是物理HDMI无黑和完整HDR验收。当前设备保留原有Decode／客户端／Mali，Frame Guard关闭。

## 提交、恢复与控制端源码进展（覆盖上面的旧阶段描述）

限定原厂提交窗口已实机通过：all-modeset锁与原厂commit-tail mutex共同保护helper表的复制／发布／恢复，只对指定任务和原生事务委托一次flush，保留原生planes／fence／cfg_done。原值写回验证8个VTEM数据字和基础2250行时序，TX_EN bit9始终关闭。内部OFF会话也接入真实vblank观察，只有提交和完成检查成功后才发布；这不等于持续QMS绑定通过。

已纠正旧私有笔记的寄存器误认：port-data的regs在+0x58；表+0x90／0xc4c是HACT，垂直总行数／VS_END在表+0x9c／0xc50（按端口加0x100），mask=0x1fff1fff。错误候选在自定义写入前被门禁拒绝，没有放松检查。DSP_CTRL bit15仍保留为post CSC。

原厂完整显示重建已执行一次，原模式、图层缓冲、基础行数、QP供电和scheduler状态经ADB检查恢复；研究模块正常卸载且临时文件已清理。用户目前只能ADB查看，**物理桌面恢复与短黑均未验收**。该测试未改变QMS时序，不能替代已启用VTEM／变速后恢复测试。

新增native_engine提交后发布及错误检查，13项实际C测试通过。controller／endpoint源码已按内核环境编译，25项原生导入的CRC可用；它们仍是独立对象，未链接入功能模块或注册真实端点。Android隔离执行15项并发／超时和11项接口分发测试通过，使用注入显示操作，不计为内核或HDMI验收。

当前尚需持续硬件绑定与正常合成／热插拔／电源生命周期、分数帧截止时间、真实控制端接入HWC、Decode loader／boot及重启回退。设备保留原配置、Frame Guard关闭；没有完整QMS升级包，也没有短黑已修复的结论。

## 按厂商整套提交继续回移：关闭、休眠和后端连接

已对照厂商两份提交列出完整映射：`decode-kernel/qms/PORTING.json`。新增生命周期代码覆盖encoder-disable重置语义，并补充原厂系统PM的独立旁路：旧epoch在mode_set／disable／enable时失效，自有VTEM在断电前清理；suspend／freeze／poweroff需要先退出QMS，失败则保留原厂设备供电；恢复后不自动重用旧链路／HDR资格。候选表只构造在私有内存，没有发布到实际驱动。

`native_binding.c`连接了控制器后端与原生carrier／flush／vblank／私有会话完成发布，并实现恢复前排空、外部模式保护及PM退出入口。原厂完成检查增加了QP供电、CRTC active及生命周期epoch核对，再允许读取MMIO；也核对VTEM TX_EN实际值，不能只检查其他scheduler位。缺少实际固件的实时链路／HDR资格检查时，初始化会拒绝继续。

新验证：17项实际C生命周期策略、26项实际寄存器／bridge／PM代码注入检查通过。设备上的只读准备检查匹配8张原厂表，bridge私有指针与PM布局通过，拒绝门禁和正常卸载通过；没有发布回调、提交显示事务、写寄存器或启用QMS。用户回到桌面后，供电完成检查更新后的实机提交已通过：一次原厂OFF会话提交保留2个plane的模式／缓冲，真实vblank前进1帧，暂存观察成功发布，flush窗口恢复并正常卸载。仅验证供电正常时的正向路径；断电失败分支仍仅有注入测试。没有写自定义寄存器或启用VTEM，内部身份为测试注入，不能当作真实链路／HDR资格证明。

实时EDID／PHY链路／色深／HDR校验、首次挂接和恢复后的重新资格检查、实际功能端点、loader／boot及重启回退仍未完成。不能将源代码连接或注入测试当作完整厂商移植，更不能据此宣称HDMI短黑已修复。

## QMS启用前检查：当前TMDS链路已具备接收端条件

已补齐`native_link.c`与`native_qualify.c`，接入后端逐帧提交前后的板级检查及恢复后清理检查。资格读取当前保留引用的原生EDID，并直接调用原厂DDC块读取器与物理电视逐块核对；没有更新EDID属性或使用软件覆盖。当前实际4K60、YUV420八位SDR路径通过HPD／PHY就绪、297MHz时钟框架读数、AVI包／输出格式及原生基础时序／VTEM关闭检查。探针正常卸载、临时文件清理、同次启动与Frame Guard关闭均确认。

54项实际C注入检查覆盖断电、HDR包及元数据、过期身份、外部VRR和恢复清理。十位HDR只是注入测试；尚未在当前硬件HDR场景验收。当前首个适配只允许已核对的未压缩TMDS格式；422、FRL／DSC及其他精度／打包路径保持拒绝启用，不切换到低质量模式以通过检查。

当前仍未启用QMS，没有真实VTEM启用／变速实验。剩余直接工作是图形服务前持久挂接及对象寿命、原生事件后重新资格、功能端点／HWC和Decode加载／重启回退，再验证实际变速、恢复与逐帧截止时间。电视支持QMS和当前链路准入通过，不能代替这些功能条件；物理HDMI无黑仍须用户实屏观察。
