# Decode Stack 0.8.2-test1：HDMI 换时序假断开兼容修复

当前是指定 API32 固件的私有实机候选，不是公开发布版，也不是全新版内核或 Composer。
修复集成在 Decode Stack，新增独立、默认关闭、重启生效的 `hdmi_audio_guard` 分项。
保留 Frame Guard 的原策略 `match=0 / min=60 / peak=60`，不把解除保护当作升级。

## 已确认的故障链

关闭 Frame Guard 并采用原生内容帧率匹配时，实际 Exo 播放触发 60→24Hz。
旧内建 HDMI 驱动在时序切换期间发出 extcon `HDMI=0`，约 225–255ms 后又发出 `HDMI=1`；
独立 DRM 连接状态在这段时间一直为 `connected`。Android 随之广播
`ACTION_AUDIO_BECOMING_NOISY`，Exo 误暂停并清除内容帧率请求，显示又回到 60Hz。
反复换时序导致额外短黑，部分运行最终出现 CCodec queue watchdog fatal。

HWC 模式查找开／关两态均曾复现，且 config 请求本身均成功；该模式表修复不能单独
解决这条音频热插拔链。也不能把 Surface 断开后出现的取帧失败误当成最初根因。

## 上游依据和本地范围

Rockchip 的两条具体提交与此行为直接对应：

- [b84c5d0c819a](https://github.com/rockchip-linux/kernel/commit/b84c5d0c819a3b544738cf00a79cc496917bb767)：
  `Only set extcon status when hdmi plug/unplug`，把 extcon 通知从 bridge enable/disable
  移到物理 connector detect；说明中包含播放器销毁／重启引起反复 enable/disable。
- [48498c68b5da](https://github.com/rockchip-linux/kernel/commit/48498c68b5da4c658d7c363dfbbfb6632ca1d324)：
  配套把音频 `handle_plugged_change` 通知移到真实 connector detect。

当前实际事件确认旧行为仍存在；没有匹配厂商源码，且驱动内建于内核，因此没有直接
回移这两条内核补丁。本项借鉴其“物理拔插与换时序分开”的语义，实施有界用户态兼容修复。
精确上游来源、本地改动边界见 [UPSTREAM.json](../magisk-hdmi-event/UPSTREAM.json)。

只在 `system_server` 的 UEventObserver 导入路径处理该固件唯一匹配的 HDMI extcon 事件。
原始 off 到达且 DRM 仍 connected 时，最多等待 400ms；匹配 on 在期限内到达且仍 connected，
才合并瞬态 reset。真实 disconnected、读取失败、未知状态或超时均传递原断开。
重复 off 不能延长截止时间；其他硬件事件直接传递，原始数据包字节不改写。

安装时按完整哈希检查原 runtime／legacy 库，在设备内生成只替换一条 DT_NEEDED 的 runtime
副本；原依赖仍加载。源码构建 helper 再校验精确导入槽，仅在本进程安装适配。
若 framework 早于 Magic Mount 加载，启动期重启 framework 消费者一次，并核对实际
system_server 映射与 ready 属性；失败则整包停用并重启回原件。GPU 首次绑定与 Zygisk 选择不变。

本项不修改 HDMI 时序、音频 HAL、解码 work、fence、watchdog、播放器统计或像素。
没有降位深、分辨率或帧率，没有全局关闭 AFBC。

## 新增实机结果与边界

| 检查 | 结果 |
| --- | --- |
| 状态机与精确私有 runtime 导入 fixture | 57 项通过；不代替播放器或真实拔插 |
| Android 隔离分项控制 | 11 项通过，未修改 live 模块 |
| WebUI | 73 项通过，含新开关、依赖、语言及待重启状态 |
| 同入口真实 Exo 首次播放、暂停／恢复 | 在临时关闭 Frame Guard 的条件下，4K24→4K60→4K24；合并 9 次瞬态，未再误暂停，未出现 queue watchdog 或 Codec reported error |
| 原生解码路径 | 实际 Exo async MediaCodec、HEVC Main10 AFBC；未换 HW/HW+ 菜单或源格式 |
| 真实拔线／回插 | DRM 真实 disconnected／connected；Android 正常收到 HDMI 拔插和 NOISY，未被合并；回插后 HDMI 输出设备重新列出 |
| 拔插后播放恢复 | 已有实际画面和播放状态恢复；没有人工听音或光学时延测量证据 |
| 精确 seek 补验收 | 标准 seekTo(60000) 与一次 UI 拖动后报告位置接近片头，未达目标；不计通过。当前这些运行未出现新 queue fatal，仍需核对播放器时间线与真实定位 |
| 新候选两级重启回退 | 分项关闭、整包停用、同一候选重新启用均完成重启；实际文件哈希、helper 映射及双 ABI Zygisk／LSPosed 全部通过 |
| 新 runtime 下 HDR 兼容 | 显式 P010 的 PQ 100 帧／HLG 190 帧通过；4K AB30 AFBC 扫描、PQ 完整元数据一致、HLG 清除 PQ 静态字段、混合 SDR UI／SDR 虚拟显示及 AB24 退出通过；未重做旧版全目标精度长测 |

媒体快进／后退键曾触发 flush 后继续播放，但位置接近 0，不能作为准确跳转验收。
MediaSession 的 position 只在状态改变时更新，也不能仅凭数秒不更新判断冻结。
新发现的定位异常尚未完成开／关对照，不能归因于本项，也不能声称已解决。

**真正 24↔60Hz 换时序的短黑，以及暂停清除帧率请求后恢复再切换，仍未解决。**
本项减少假拔线造成的额外误暂停和循环，不保证 HDMI 换时序无黑屏，也未证明所有历史
Exo fatal 永不再现。该历史阶段曾恢复 Frame Guard；用户随后明确要求本轮排查及验收
全程关闭，不在阶段间或收尾自动开启。最新回归补充如下。

## 后续 Exo 报错回归（2026-10-19，当前优先处理）

原固件没有此次升级后的播放器报错是用户提供的重要对照。恢复旧 LSPosed 客户端
test6 仍保留整套 Decode 升级，不能称为原固件对照。实际 Exo 在 test6、Frame Guard
关闭、HDMI event guard 开启时，长暂停后再次触发 queue watchdog 与 codec error。
阻塞栈显示输出线程在 `ensureDecoderState()` 预取 BufferQueue 缓冲时等待，输入线程
同时重试 `sendpacket()`。因此此前短时未报错的测试不能作为报错已根治的证明。

暂停保持内容帧率的 LSPosed test1 候选未通过实机验收，已撤回至 test6，未纳入
Decode Stack。不能用保持帧率的规避替代解码队列修复。

本轮整包停用 Decode、保留 Mali／LSPosed 后，已完成长暂停／恢复且未见上述两类报错；
这只覆盖当次暂停场景，不证明快进通过。随后恢复其余 Decode 升级、仅关闭
`codec2_sdk_next`（使用包内 1.14.3_[2]）后，用户快进触发输入包提交反复重试，继而
`Codec reported err 0xe`。该次没有先出现 queue watchdog；旧核心回退仍不能算修复。
快进回到开头与解码报错均记录为未通过，未归因于媒体索引或要求用户修改片源。

当前继续按升级回归缩小范围，短暂换时序黑屏、QMS 和帧率保持试验暂停推进。
用户负责实际播放／暂停／快进；助手仅读取故障日志与必要状态，不再远程输入、
截图或录屏。Frame Guard 保持关闭。尚无新核心修复部署或可发布声明。

## 回退、完整性与交付

关闭该分项并重启应恢复 factory runtime，helper 不再挂载／加载，其他 Decode 项保留。
整包停用并重启恢复原 SF、allocator、HWC、HWUI、Codec2 框架及 runtime；Mali Global 是
另一模块，保持原 g18 与现有 KMD/CSF fence、DVFS 20ms。再启用使用同一已安装候选验证恢复。

已测试 ZIP 的 SHA256 为
`5ce50cf47a5e815415aabce15a29c0fceb29e8edf40518ba74a6c492d82eb287`。
302 个包内文件校验通过；37 个既有组件二进制与 test7 逐字节相同。
包内不含安装时生成的原固件／变体，私有运行地址、影片、日志和密钥不进入公开载荷。
本轮保留这个 ZIP 不变；构建之后补充的上游引用和实机结果记录在源码及本外部验收中，
不倒改已测 ZIP 的嵌入元数据，也不把重打包后的新文件当成已安装验收。

最终结果见 [机器验收记录](validation/decode-hdmi-event-082.json)。
其他未修复项与升级目标见 [主要目标表](main-goals-priority.zh-CN.md)。
