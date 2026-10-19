# Decode Stack 0.8.6：播放期间保留已匹配帧率

正式版依据：test20 已安装重启并通过启动／回退检查；共同图层入口实际记录到 24Hz 片源请求及两次将清除请求保留为 24Hz，原函数真实返回 unchanged。用户反馈“可以了”，授权整理正式版并上传。采集窗口只观察到开始匹配和退出的两次模式变化，未见中间重复选模。临时追踪已关闭。该结论限指定 Exo 场景；不同帧率换片和所有接收端仍不外推。

正式发布使用与验收完全相同的 librkses.so（SHA256 dc6f5d29a9590a3e65e1808aadcd80da3d13809b1ed05aeefd94ef9e9477e054）；整理版本号、说明与包内来源不改变运行库。

以下保留候选设计、失败对照和检查范围。

# 系统播放帧率保留：test19 失败与 test20 候选

目标：Auto 下保留已匹配的片源帧率，暂停／恢复／同片快进不来回选模；新的正数片源帧率照常更新，隐藏／移除图层由原生选模逻辑排除。归 Decode Stack，独立 display_session 开关及重启回退。Frame Guard 始终关闭。

## test19：未通过

用户确认播放器内仍有 HDMI 短黑。库映射、启动属性与加载签名均正常，但实际只读内存计数 held/accepted/reaped/logs 全为 0；有界采集从 uptime 133.79 至 314.32 秒，QMS serial 12→20。不能以 attach=active、日志无错误或软件切换完成认定策略生效。

test19 只拦截 SurfaceFlinger 的旧 Binder setFrameRate 虚函数入口。本机 libgui 还具有 BLASTBufferQueue→SurfaceComposerClient::Transaction→SurfaceFlinger::setClientStateLocked 的图层事务路径；后者无需调用旧入口。计数结果证明原候选没有获得本轮保留效果；尚不能从空日志单独证明每次短黑都由同一个 API 触发。

## test20：修正调用覆盖的候选

改在精确原厂 Layer::setFrameRate 的共同落点处理，不再使用 producer 虚表或跨请求 weakref 缓存。本机可定位到三个直接调用点：0x157048、0x191338、0x1b3428。使用原函数已经保存的该图层 FrameRate，仅把清除请求替换为该图层已有的有效 fixed-source FrameRate；保留全部 24 字节原始分数帧率、duration、兼容性及 seamlessness。新正数值、非视频/default/NoVote 请求、固定模式和关闭配置照常交给原函数。

安装期锁定原始 SF SHA256、平台依赖与加载器链；启动构造时校验原函数字节，只在 SF 主线程启动前安装固定 ARM64 入口跳转，重定位精确的 ADRP 指令。没有通用指令重写器、热更新、RWX 映射、每帧线程或 HWC 假成功。始终调用原函数一次并返回真实 changed/unchanged 值，保留原生层级传播与可见性规则。

同一个视频图层仅清空 rate 的请求无法在系统接口中区别“暂停”和“释放但暂留可见 Surface”。因此退出仍依赖原生隐藏／删除图层时停止采用旧投票，必须实际验收；不能提前宣称完整播放会话语义或所有 Exo 应用兼容。

## 检查与实机边界

- Android 隔离进程 110 项：实际原厂 ARM64 函数 prologue／重定位／返回路径，分数帧率，两个图层互不串用，新帧率更新，禁用／固定模式，原生返回值及新对象无遗留缓存。无显示 IO、无播放器输入。
- 实际 Mali 配套检查器 19 个隔离夹具：完整配套通过，混用库／缺失选择／旧启动收据等拒绝。
- 暂存整包的分项关闭→原加载器恢复→重新开启检查及原有配置／QMS／HWC／系统设置二进制不变检查随后记录。
- 实际显示器短黑、换片、退出仍待用户验收；上述检查不构成修复通过。

诊断仅在有限请求事件窗口记录，最多 96 条，无每帧轮询；采集结束关闭 debug.rkdecode.session_trace。保持原有 g18/KMD/DVFS、双 ABI Zygisk／LSPosed 与 Auto/HDR；不写 boot。关闭 display_session 后重启恢复旧提示处理，整包停用恢复原固件。
