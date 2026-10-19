当前进展（2026-10-19）：已恢复 Decode 0.8.6-qms-test14＋Mali 0.4.2-qms-test16，并通过实际回退重启检查。test14 修正稳定状态下过宽的数据包检查，8 次切换完成、零帧错误，实屏播放／暂停／步进仍短黑；切换边界未观察到 GCP／HDCP2 变化。test15 四帧提前通知已实际执行，用户反馈没有改善，已从源码和设备撤回，避免增加无收益延迟。Auto／HDR、Frame Guard disabled＋paused、Mali g18／KMD／DVFS、双 ABI Zygisk Next／LSPosed、模块标记和 boot 均保留。短黑尚未根治，后续核对真实 VTEM 发送／接收时序，不能把本地切换完成视为电视验收通过。详见 [系统 HDMI 进展](system-hdmi-blackout-086.zh-CN.md)。

以下 test11 及更早摘要为历史阶段。

当前状态（2026-10-19，test11）：系统设置修复已通过两次真实启动，display_policy=1，用户确认 Auto／修复开关正常，电视已显示 QMS-VRR。Decode 0.8.6-qms-test11 保留 test9 QMS 核心；Mali g18／内核／DVFS、双 ABI Zygisk Next／LSPosed 正常，Frame Guard disabled＋paused，hdmi_audio_guard=0。Auto 下实屏短黑仍未修复，固定 4K60 不短黑；当前继续系统 QMS 时序／生命周期根修。其他系统实际调用、已知修复及本机适用性见 [系统 HDMI 对照](system-hdmi-blackout-086.zh-CN.md)。完整 HDR、固定模式持续锁定等仍各自验收，不由设置启动成功推定通过。

以下启动失败及 test8／test9 摘要为历史记录，当前状态以上述 test11 为准。

当前状态（2026-10-19，设置候选 test10 已撤回）：“系统分辨率／HDR 设置修复”未修复。jar-only 和配套系统 OAT 候选均在真实启动中触发 system_server 的 ART untrusted data-cache abort，继而整包自动回退；补齐编译产物仍不足，早期缓存选择／加载顺序待查。已撤回 test10，恢复 test9 原脚本、组件与工作选择（display_policy=0、hdmi_audio_guard=0），已实机确认恢复：Decode、Mali g18／内核／DVFS、双 ABI Zygisk Next／LSPosed 均正常，全部模块标记与试验前相同。不能以开关保存、离线检查或配套编译成功计为可启用。QMS 播放验收暂停；Frame Guard 保持 disabled＋paused，Zygisk Next／LSPosed 选择保留。实屏结果仍为步进不黑，跳段和播放／暂停短黑；LG 无 VRR 提示不作为未启用的证据。

2026-10-19 补充：用户要求先修复“系统分辨率／HDR 设置修复”开启后重启又关闭，再进行 Auto／QMS 验收。发现独立回退保留失败 pending，导致重新开启仍反复回退；现修改为失败时归档并消耗该票据，新试验仍须完整启动确认。实际 Android 隔离回退／重试 22 项通过，不能当实机保留通过。恢复过程第一次被另一个 HDMI 音频事件适配启动检查阻断，Decode 整包自动停用；已保存证据，第二次恢复关闭该失败分项、启用 display_policy、保持 Frame Guard disabled＋paused，并恢复现有匹配 cold boot。第二次启动和开关跨重启保留尚待 ADB 回连核对，未宣布修复通过。此前实屏结果为步进不黑，跳段和播放／暂停仍短黑；LG 无 VRR 提示条不作为未识别 QMS 的证据。

当前状态（2026-10-19，test9 已实机启用）：Decode Stack 0.8.6-qms-test9 和 Mali 0.4.2-qms-test13 已完成冷启动；QMS 新 owner／端点／HWC、Mali g18、内核、DVFS、Zygisk Next／LSPosed 双 ABI 均通过本次启动检查，模块启停标记与试验前一致。Frame Guard disabled＋paused。新版恒定时序保持／命令唤醒／100ms 只读核对已进入真实设备，尚待本轮 Exo 操作验证持续 24Hz、恢复和开销；HDMI 实屏无黑仍未通过。当前优先 QMS 主问题，暂停 UI 工作。HDMI 音频事件适配和系统显示设置修复暂时关闭用于隔离启动问题，不算其验收或永久撤销，其他播放升级保留。

## 当前摘要

当前状态（2026-10-19）：设备运行 Decode Stack 0.8.6-qms-test8，QMS owner／端点／HWC 已启用；Mali g18、DVFS 和 Zygisk Next／LSPosed 双 ABI 正常，Frame Guard 关闭。Auto 菜单勾选已由用户确认正常，HDR10 默认显示开启已确认。指定 Exo 暂停恢复和快进索引有各自的局部通过记录，不能等同所有应用验收。test8 的真实播放日志定位到恒定 24Hz 观察跨过两个真实帧触发 gate93，导致回到 60Hz；QMS 持续输出与 HDMI 实屏无黑仍待验收。新版 test9 候选改为恒定时序硬件保持、事件唤醒及 100ms 只读核对，尚未部署。

test8 的五次 gate93 故障均为 previous→sampled 相差 2，观察时间差约 83.33ms，expected/completed timing 都是 0x15f9000a（恒定 24Hz），native_error=0。严格连续观察把稳定阶段的跨帧当作故障，不能据此认定播放器丢帧。test9 仅对已完成票据、STABLE 且表项 bits=0 的恒定时序允许只读保持；新变速必须在原厂提交窗口核对旧硬件并建立真实帧锚点，随后仍逐帧严格推进。分数时序不进入保持路径。

性能候选由 controller 等待命令事件，稳定时每 100ms 最多一次只读核对；不再每帧提交相同 atomic carrier。时序、VTEM、供电、生命周期与所有权仍核对；切换时仍等真实 vblank 和 fence。74 项原 Image 导出 CRC 与 8 张表核对通过；19 项 controller 与 11 项 endpoint 隔离测试通过。当前尚无新版实机 CPU／锁竞争测量，不能声称零开销或游戏帧率提升。

以下为逐版本历史记录，旧“当前进度”以本摘要为准。

# QMS 冷启动集成候选与恢复记录

当前进度（2026-10-19，test6 已启动，Auto／固定模式交互验收待进行）：test5／test6 已真实绑定 QMS owner／端点／HWC，冷启动 baseline VTEM 启用及恢复通过。Mali g18、kernel／DVFS、Zygisk Next 双 ABI LSPosed 注入和 lspd 均正常，Frame Guard 仍 disabled＋paused。实际播放能完成 60→24Hz，但持续运行会发生 EIO 并走原厂完整恢复，反复回到 4K60；**尚未修复持续变速或 HDMI 短黑，不可发布**。

已定位的冷启动 ESTALE 是 PHY 软件时钟 API 初始返回 0，像素／TMDS 已为正确基准。只在无 DRM 客户端、原生模式干净、完整原厂事务 check 通过且失败恰为该零时钟条件时，执行模式不变的原厂初始化事务，再重做全部链路资格检查；独立试验取得 PHY 0→297MHz，并正常卸载。没有直接改缓存、猜寄存器或跳过门禁。

真正启用后出现过一次 Mali 回退：GPU 精确配套检查只认识旧 HWC，拒绝新的 QMS 完整组合。已先恢复并验过原工作路径，再补齐精确 HWC／KO／helper／profile 与当前启动回执配套；只接受列出的组合，混用仍拒绝。test5 实机 g18 保持 active；test6 新组合的实际 checker 隔离测试共 13 项通过，当前旧组合也现场通过。没有改变原生 Magisk zygisk=0 的独立 Zygisk Next 配置。

test5 保留的完成检查显示，故障发生于本次寄存器 finish 前，日志中的 diagnostic=0 和 timing 来自上次成功提交，不能把它解读成当前事务通过。test6 继续保留全部校验，另外记录 native 返回值、window／session 返回值、故障前后帧号和真实 vblank 时间戳，以区分采样重复、跳帧与原厂提交失败。源码引擎的 17 项发布／故障测试通过，包括重复／跳变帧与不递增时间戳必须拒绝；没有将漏帧当成功或合成追帧。

系统固定模式候选已作为 Decode Stack 的独立 `display_policy` 项集成。精确原 `services.jar` DEX 四处默认语言 formatter 改用 Locale.ROOT；保留原 native setter，再向原 DisplayModeDirector 通知。固定的受支持模式通过 desired specs 锁定尺寸与真实帧率，Auto 原样委托原策略；不改全局语言、不改播放器。桌面生成的 firmware variant 不进入公开组件。实际 Android 离线生成、DEX hash 和反射 ABI／六种语言检查通过；JVM 19 项策略、Android sh 11 项开关、10 项独立启动回退及 WebUI 85 项检查通过。它们不等于系统菜单与真实播放验收。首次启动未确认时下一次启动只撤回该项，保留其他模块状态。

用户目前仅能远程观察。HDMI 实屏短黑、fractional QMS、完整 HDR 与实际菜单固定／Auto 对照均未验收；不得用日志、投屏或启动回执代替。

test6 当前桌面／设置空闲读取：Auto、min/peak 设置均未设定、desired primary 为 0–60Hz，当前模式等于默认模式，QMS frame_errors=0。用户补充“回到 4K60”可能发生于重启或远控，并非指视频播放；不能把该现象直接归因于 test5 播放故障。远控触发与固定菜单保持仍需分开验证。

test7 增补系统 HDR10 开关：原厂 menu 的 isHDR10Status 将 nativeGetModeState 返回的空串与 "0" 比较，因而显示关闭；原始 HWC 的 SwitchHdrMode（RVA 0x3fa04）在 property_get 调用 0x3fac4 使用默认 "0"，实际默认允许 HDR。启动日志也取得了空值→界面 false、用户打开→真实写 0 的对应关系。候选 getter 仅补齐空值默认；setter 保留实际 HAL 调用，在成功后保存明确的 0/1，保存失败不得报告已保存。仅 display_policy 生效时在图形服务前恢复已保存值；关闭此项重启恢复原逻辑。30 项实际 Java 策略、15 项 Android 隔离启动／恢复、85 项 WebUI 检查通过；设备 DEX 生成和反射 ABI 通过。test7 已启动并取得当前 boot 的策略 qualified。经 app_process 初始化 Android runtime 后，调用原厂 RkDisplayOutputManager.isHDR10Status 的只读 Binder 查询取得 true，原属性／保存属性仍为空，验证了空值默认显示修复且未强行写开关。裸 dalvikvm 未注册 SystemProperties JNI，最初只读查询失败，已改用正确 runtime；不是系统服务故障。用户明确选择的跨重启保存仍待验收，不能作为 HDR 输出修复或 QMS 通过。

test8 增补 Auto 已选项读取：用户确认 HDR10 已显示开启，但 Auto 菜单仍显示 4K60。本次同 boot 的只读 Binder 查询取得 primary interface=11、保存值 Auto、getCurrentMode 返回 3840x2160p60.00，且 QMS frame_errors=0。getCurMode 仍读取 persist.vendor.resolution.main 的旧 Auto 标记，实际写入使用 persist.vendor.resolution.HDMI-A-0。候选只在确认 primary HDMI 后，为该旧键读取提供有效的新键值，保留原厂实际模式查询和格式化；副屏、非 HDMI、未知值及查询失败保留原读值。Auto 下实际输出与上一级“当前输出”摘要仍可为 4K60，不伪造 Display.Mode。39 项实际 Java 策略及设备 DEX／ABI 预检通过，test8 已启动、当前 boot 的策略 qualified。同一只读原厂 Binder 查询取得 interface=11、保存值 Auto、getCurrentMode=Auto，HDR10 仍 true；修复后 API 对照通过，用户随后确认“Auto 正常”，菜单勾选已验收。QMS 播放的持续变速 EIO 和短黑仍独立待修。

以下为早期过程记录，状态已被上述进度覆盖。


当前设备恢复已知工作 boot、Decode Stack 0.8.4-test3 和 Mali g18；Zygisk Next／LSPosed 的 32／64 位注入、lspd、Mali 冷启动和 Decode 启动检查均已恢复验证。Frame Guard 保持 disabled＋paused。没有启用 QMS，也没有 HDMI 无黑结论。

已补齐的候选源码包括 `owner.c` 的冷启动对象／模块寿命管理、真实控制端点、bridge／PM 回调发布、原生模式变化后的重新资格，以及 Decode loader、独立选择和 HWC 接入。功能模块离线构建通过 68 项原生导入 CRC、8 张表检查；运行中 Android 会因已有 DRM 客户端而拒绝加载。只接受原始 Image 的精确代码；VHE 指令替换取自该 Image 的 alternatives 表，不按观测值放宽指纹。

本轮试启动发现并修正了三项集成问题：

1. Windows 私有重打包丢失 Linux 权限。现按设备原始 tar 元数据和新增文件明确权限生成包，并在安装前逐项检查权限和哈希。
2. permissive umask 导致 active-boot-id 不满足 root 私有回执要求。现使用 0600 临时文件和原子 rename；实际 Android sh 隔离检查覆盖 umask 000。
3. 显示项主动回退被误认为 GPU 启动失败。候选仅在 GPU 未绑定、图形进程仍为停止的 init 子进程、pending 恰为当前启动时，移除尚未开始的 GPU 试验回执。旧启动或已开始的 GPU pending 不清除。

第三次已进入真正的 finit_module 调用，但原生身份门禁返回 ESTALE，未完成 owner 发布、端点注册或 VTEM 启用。确切失败字段尚未定位，不能据此认定接收端不兼容，更不能宣称完整 QMS 已修复。失败后显示选择关闭；本次验证了该失败路径保留 g18 启动，但不是已启用 QMS 的恢复验收。

第一次异常启动还产生了多个模块禁用标记。最初恢复只覆盖 Decode／Mali，遗漏 Zygisk Next／LSPosed；随后已恢复本次异常影响的原有模块、原 boot 和 Decode，且实际注入状态确认正常。不能将之后注入需求被记录为 off 当成用户选择。没有同时开启原生 Magisk Zygisk来代替 Zygisk Next。

新增隔离验证通过：控制器 18／端点 11、链路读取 55、bridge／PM 26、真实 HWC 回调 22、启动协调 21、QMS 控制／回执 19。它们使用注入对象或临时文件，不是 HDMI 硬件验收。

后续使用独立、可卸载的只读 owner 准备探针定位：运行中的 Android 可通过全部准备检查；两次图形服务前采集均在 phase 7／link step 15 返回 ESTALE。原生模式为 3840×2160p60，像素时钟 594 MHz、TMDS 297 MHz，故障已缩小至 PHY 时钟 API 与预期值核对。冷启动 API 的具体返回值仍待取得，不能假定为 0，也不能直接跳过门禁。原内核反汇编显示该 recalc_rate 读取软件字段，尚不足以判定物理 PLL 异常。两次探针均正常卸载，未提交显示事务、发布 owner 或启用 QMS；后续 g18、Decode、Zygisk 双 ABI 和 LSPosed 正常。

用户随后在中文系统设置中手动将 HDMI 分辨率设为 Auto。读取确认系统属性已为 Auto，实际输出仍是 3840×2160p60、YUV420 8-bit SDR，像素／TMDS 时钟仍为 594／297 MHz；当前没有复现“中文界面无法写入 Auto”，但这不覆盖其他模式切换或本地化解析问题。Frame Guard 的 disable／paused 均保留，系统策略仍为 WITHIN_GROUPS。尚未取得本次 Auto 配置下的播放操作结果及 HDMI 实屏观察，不能计为短黑修复或 QMS 启用。

首轮 Auto 播放操作后，用户反馈仅能远程观察。日志确认实际内核是 mpv／mediacodec-copy，未见 ExoPlayer 初始化；该轮单独记录，不能充作 Exo 暂停／快进验收。操作后 HDMI 仍为上述 4K60 SDR，日志未见明确模式切换，不能由此证明操作全程没有切换或短黑。已请求用户明确选择 ExoPlayer 后重做；未远程操作播放器。

用户随后确认先前启动错内核并改为 Exo。新的独立采集确认 Exo 提取器找到真实 Cues，观察到 seek／flush；截至采集结束，未见 PlaybackException、codec signalError、queue watchdog 或 BadConfig。记录到 11 次框架模式变更（24Hz／60Hz），并由内核日志确认 VOP2 atomic disable／enable、PHY PLL／lane 重新锁定，属于完整原生模式重建。24Hz 时实际输出为 4K24 RGB888 SDR，前述 60Hz 状态为 YUV420 8-bit SDR；TMDS 均为 297 MHz 并不等于输出格式／像素时序未变。单独设 Auto 未消除完整重建；不能将此计为 QMS 开启、整轮交互验收或 HDMI 无短黑。

本轮只读启动观察结束后，已按原始 SHA 精确还原设备 Decode 的 post-fs-data，临时 qms-check 脚本及组件移至模块目录外的私有证据目录。设备没有待执行的该诊断入口，未重启或写 boot；模块启停状态与恢复前一致，Frame Guard 保持 disabled＋paused，双 ABI LSPosed 与 lspd 仍正常。新的只读探针 build4 仅离线构建，尚未部署或采集冷启动 PHY 返回值。

仍需取得冷启动 PHY API 返回值并解决资格检查，完成真实 owner／HWC 联动、实际 VTEM 变速与恢复、正常合成并发和分数帧截止时间，以及完整十位／HDR、热插拔／电源和独立重启回退。物理 HDMI 是否短黑仍须用户实屏观察。当前不继续自动冷启动试验，候选不发布。
