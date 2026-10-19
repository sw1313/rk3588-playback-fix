# 原固件 Exo 短黑：重新核对触发链与 QMS 验收边界

日期：2026-10-19。状态：联网核对和已授权回退完成；短黑未修复，无新驱动候选或无黑验收。

用户再次指出，原固件没有实装 QMS 时已存在播放／暂停短黑，固件群提供的办法是锁帧。该事实必须作为问题边界保留。此前围绕 END 位和等待帧数的负结果不再重复作为主线。

## 原始短黑与新增功能失败是两个问题

1. 原始触发链：Exo 的帧率提示改变，系统选择显示模式，普通 HDMI 模式切换可能让电视短暂黑场。用户观察与此相符，但尚未直接截获本机每次应用帧率调用，不能将整条链视为已逐次证明。
2. QMS 的目标：同一输出格式下改变帧率时避免上述黑场。安装前已有短黑，不能据此否定 QMS 的作用；安装后仍黑，也不能因私有状态为 ACTIVE／命令完成就断言 QMS 已正确作用于接收端。

锁帧切断了帧率变化，能规避故障，但不区分是普通模式重建、短暂输出属性改变还是未正确完成的无缝传输。继续保持 Auto、原生 Surface 和 Frame Guard 关闭，不把锁帧或应用专用拦截交付为系统根修。

## 重新打开并核对的一手来源

| 来源 | 明确说明 | 对本机的约束 |
| --- | --- | --- |
| [Android 帧率 API 官方说明](https://developer.android.com/media/optimize/performance/frame-rate) | 暂停但 Surface 仍可见时建议清除帧率提示；部分盒子的刷新率切换会黑场；应用请求不保证被系统采用 | 暂停不是 Surface 必然被销毁。应查请求与实际选模，不先用 TextureView、重建解码器或修改缓冲参数 |
| [ExoPlayer #7921](https://github.com/google/ExoPlayer/issues/7921) | 播放／暂停导致电视 50/60Hz 切换和短黑，scrcpy 无异常；维护者归为设备平台问题，没有提供通用补丁 | 高度相似，但已在本项目旧记录出现，本轮仅复核，不算新根因或新修复 |
| [AOSP 多刷新率实现说明](https://source.android.com/docs/core/graphics/multiple-refresh-rate) | 配置分组、seamlessRequired 和切换时间线共同决定无缝切换处理；无法无缝时有明确拒绝语义 | 需要核对本机实际 HAL 版本与入口，不能仅把模式放在同组或返回成功就视为物理无黑。当前桥接源码拦截的是原有 SET_ACTIVE_CONFIG 等入口，不直接套用 2.4 的时序结构 |
| [Collabora QMS/VRR RFC 原开发者说明](https://lwn.net/Articles/1095764/) | 固定目标帧率与 QMS 信令为不同控制项，实现经 RK3588 开发测试 | RFC 是设计和实现参考，不是当前原厂固件可直接替换的稳定补丁，不证明本机报文已在线端正确发送 |

本轮扩大检索 RK3588／Android HWC／原固件 Exo 暂停黑场，没有找到可直接适配本机且已确认根因相同的新修复提交。泛化的 Surface 黑屏文章、其他应用切流／解码器复用问题和专利方案不作为本机缺陷证据。LG 的格式切换专利另描述超出仅变帧率的情形，不能据此向现有 VTEM 保留字段注入新协议。

## 后续定位先补哪项证据

优先将同一次操作中的帧率提示／显示策略选模、HWC 目标配置及其实际分流（QMS、原厂、拒绝／恢复）、显示时序和格式身份关联起来。保留已经得到的无重建短黑负结果；不能重复只检查 QMS 累计完成数，也不能预设所有短黑一定来自原厂重建。

若操作期间走原厂分支，先解释其具体原因及是否为合理格式变化；若完整走 QMS 且时序和格式确实不变，继续查实际传输与接收处理。前者定位请求路由，后者定位无缝实现，两者不能互相替代。现有寄存器、SCDC、平面结构和 TV 只读 API 的采样限制仍然存在，最终以用户 HDMI 实屏观察验收。

本轮已实际回退 END=0 到 test17＋Mali test19，启动健康检查通过，保留 DDC、声音及已有播放修复。未新增播放操作、截图、锁帧或 boot 刷写。

## 针对“暂停期间保持片源帧率”的补充检索

用户随后要求查找现成解决方案。本节为后续独立联网检索，没有安装／修改设备配置。

- [Media3 官方 API](https://developer.android.com/reference/androidx/media3/exoplayer/ExoPlayer#setVideoChangeFrameRateStrategy(int)) 明确支持将策略设为 `VIDEO_CHANGE_FRAME_RATE_STRATEGY_OFF`，然后由应用自行调用 `Surface.setFrameRate()`。OFF 的作用是关闭 Exo 的请求；单独使用不会保留新的内容匹配逻辑。由应用另行管理媒体加载、格式变化、暂停和退出，才有条件实现“开始匹配、暂停保持、退出释放”。这是一种可实现的自定义策略，不是官方已修复本机短黑的承诺。
- 本次打开的 [Media3 release 分支源码](https://raw.githubusercontent.com/androidx/media/release/libraries/exoplayer/src/main/java/androidx/media3/exoplayer/video/VideoFrameReleaseHelper.java) 中，`onStopped()` 调用 `clearSurfaceFrameRate()`，后者在自动策略启用时请求 0；`onStarted()` 重新更新帧率，OFF 时不再由该 helper 写入。这证明默认控制行为，不能代替对本机播放器版本和实际每次调用的验证。该链接为可变分支，本轮只作行为核对，未作为构建输入。
- 固定快照 [SmartTube AutoFrameRateController](https://github.com/yuliskov/SmartTube/blob/bbed8fbbf653007bbf0eeb11b5a13ba2db4e88a4/common/src/main/java/com/liskovsoft/smartyoutubetv2/common/app/models/playback/controllers/AutoFrameRateController.java) 在媒体加载后请求 AFR；[AutoFrameRateHelper](https://github.com/yuliskov/SmartTube/blob/bbed8fbbf653007bbf0eeb11b5a13ba2db4e88a4/common/src/main/java/com/liskovsoft/smartyoutubetv2/common/autoframerate/AutoFrameRateHelper.java) 有 5 秒重复调用限流；控制器另有可配置的切换暂停等待。它是集中管理 AFR 的代码参考，不能把限流或暂停等待说成无黑保证，也没有依据照搬其可选 24→23.97 映射。
- 找到新的近似一手报告 [SmartTube #5311](https://github.com/yuliskov/SmartTube/issues/5311)：启用 AFR 后跳段或打开／关闭菜单反复整屏黑，报告者认为频率来回改变。截至本次打开页面仍为 Open，没有关联修复提交；不能作为“别人已经修好”的证据。
- [Amazon HDMI Mode Switch 官方说明](https://developer.amazon.com/docs/fire-tv/4k-apis-for-hdmi-mode-switch.html) 描述通过窗口 preferredDisplayModeId 管理模式、同模式 Activity 转场及覆盖层行为，并提醒逻辑回调到达后画面仍可能暂不可见。文档有明确 Fire TV／旧版设备适用范围，仅借鉴会话管理和完成状态的区别，不直接套用到 RK3588。

检索结论：播放器侧有官方支持的接管机制和集中 AFR 管理参考，但没有找到适用于本机的通用“暂停保持”系统补丁。对本项目可将保留可见暂停视频已匹配模式作为系统策略候选，仍需识别真正退出、Surface 销毁、格式改变和用户固定模式，不能全局吞掉所有 0 帧率请求。该策略减少不必要切换；必要的开始／换片切换仍需独立验收 QMS。应用专用 LSPosed 短黑钩子仍不作为交付。
