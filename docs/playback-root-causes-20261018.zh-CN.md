# P010 HDR、HW 绿屏与 Exo 模式切换：根因及修复范围

> 本文保留安装前的根因审计状态。随后已完成的模块集成、HWUI 映射、实机启动／回退，
> 以及仍未通过的 HDR／播放器验收，见 [0.7.9 后续记录](decode-hwui-rgb10-079.zh-CN.md)。
> 下文“未安装／尚未接入”等只描述该审计阶段，不代表当前设备状态。

本轮是故障排查和源码候选验证，**没有安装新模块、切换刷新率、重启或修改 Frame Guard**。
Decode Stack 仍为 0.7.8-test1，Mali Global 仍为 0.4.2-test1。
下列隔离测试不是播放器或 HDMI 整条链路验收，也不代表候选可发布。

| 问题 | 已证实的断点 | 能否开始根修复 | 本轮结果／仍缺少的证据 |
| --- | --- | --- | --- |
| 显式 P010 → HDMI HDR | P010 解码及元数据正确；CLIENT 合成使用 NATIVE／八位 AB24、UNKNOWN 目标，输出已为 SDR | 可以，需完整 HDR CLIENT 链路，不能只强制 HDMI 标记 | 新查到 VOP2 有 RGB 十位格式、EGL 有 10/10/10/2 和 FP16 配置；尚无真正 HDR 合成及输出验证 |
| HW／HEVC Main10 绿屏 | 原厂 Main10 复制输出曾全零；新系统 P010 输出可用，但当前播放器 MediaCodec 复制格式映射没有 P010 | 可以，必须补齐客户端与系统接口，单换内核／RGA 不够 | 已核对当前 APK 的 ABI split、实际 libmpv 依赖和 libavcodec 映射表；未替换应用库、未宣称当前播放器已修复 |
| Exo 帧率／显示模式切换 BadConfig、黑屏 | 原厂向 SF 缓存并上报 sf_modes_，查询和切换却查可变化的 connector modes；缺少具体上游修复 | 最适合先做，已有范围很小的源码候选 | 原厂及现用缩放修补 HWC 各通过 15 个缺陷／对照检查、9,808 个候选隔离用例；还未捕获原故障时的两表快照并完成真实切换验收 |

## 显式 P010 的 HDMI HDR

“显式”表示客户端要求 P010 输出，不能偷偷改成另一种格式。已有 0.7.8 开／关对照
显示解码十位样本和 PQ／HLG 元数据未丢；两个状态均为 CLIENT 合成后 SDR HDMI，
不是该版 NEON 解包优化引入的问题。原生 NV15 的 DEVICE HDR 路径另行验证过可用。

本轮重新读取系统状态：色彩管理开启，但主显示只公布 NATIVE color mode；
CLIENT 目标为八位 RGBA／UNKNOWN。已有 HDR 适配层清理错误源 HDR 标记，是使 HDMI
状态匹配实际 SDR 像素的保护。删除该保护会把 SDR 当作 PQ／HLG 发送，不是恢复 HDR。

新增只读能力检查发现：

- VOP2 多个平面枚举 XR30／AR30／XB30／AB30，另一些平面支持 NV15／NV20／NV30。
  没有枚举标准 DRM P010；不能直接把 P010 格式值当作已支持的扫描输出格式。
- 当前 EGL 25 个配置中有一个 10/10/10/2 和一个 16/16/16/16，均带 WINDOW／PBUFFER。
  EGL 未公布 BT.2020 PQ／HLG colorspace 扩展。这只限定 EGL 窗口接口能力，
  不等于证明 Android RenderEngine 一定不能用 shader 和标准 dataspace 合成 HDR。
- 枚举能力不证明 gralloc、HWC 平面选择、DRM modifier、色彩转换和 HDMI 输出已经配套。

因此不应先盲换 VOP2 内核。优先核对／补齐 HWC 的 HDR color modes、client-target
格式和 dataspace、SF／RenderEngine 十位目标选择、gralloc／EGL 实际分配导入，最后
确认真实 RGB 十位或其他完整精度扫描输出、EOTF、BT.2020 和 HDMI 元数据一致。
PQ／HLG、SDR 退出、多图层以及全部十位样本都必须重新验收。相关实现归 Decode Stack。

## HW 绿屏

当前播放器为 Yamby 2.1.0.11，arm64。实际 `libmpv.so` 动态依赖独立的
`libavcodec.so`／`libavutil.so` 等；不能把 APK 中另一套 JNI FFmpeg 当成它的解码库。
本轮读取到的解码库版本为 `N-122558-gbf4d5037b4`，Lavc 62.23.102。

不仅核对了同修订的源码，还在当前解码库的实际常量表确认 MediaCodec 格式映射仅有
YUV420P、NV12 和原有 QCOM／TI 格式，后接结束项，没有 P010／54。
libavutil 有通用 `p010le` 像素格式，不能据此推断 MediaCodec copy-back 也支持它。
对应源码的复制 switch 同样没有 P010 分支；本轮获取的 FFmpeg master 也未补齐此处。

完整修复至少需要：Main10 ByteBuffer 路径请求 P010／54；映射为 AV_PIX_FMT_P010LE；
按 16 位存储正确处理 Y／UV 行距、slice-height、crop、buffer offset／size 和边界；
保留颜色／HDR 元数据；再与原 mpv／FFmpeg ABI 配对验证。系统 CompatRouter 已允许
显式 P010 进入升级核心，不能让未适配的客户端误收 P010，也不能谎报成 NV12。

先前要求保留原播放器库仍有效，本轮未替换它们。后续若做客户端适配，需纳入
Decode Stack 的限定应用／库哈希分项、可恢复原库的部署方式，而非散装替换 APK。
HW+ 是另一条硬件解码缓冲路径，现有可用性不等于 HW copy-back 已修复。
历史普通 HEVC 绿屏日志没有独立证明都是 Main10；非 Main10 的绿屏仍须按具体格式
和当前原播放器复现，不能将这一个根因外推成所有绿屏的解释。

## Exo 的源码候选

具体上游为 Rockchip [de31070c66c61ee2bd90c42c6877d26c4c5d7c24](https://github.com/rockchip-toybrick/rk_hardware_rk35_hwcomposer2/commit/de31070c66c61ee2bd90c42c6877d26c4c5d7c24)，
2023-02-28，`Fix bStandardSwitchResolution mode bug`，Defect #398663。
核对原厂 1.3.8 的实际 ARM64 方法后确认：该查找修复尚未包含。

本地候选 [mode_cache.h](../magisk-hdr/src/mode_cache.h) 只选择性回移以下语义：

- GetDisplayAttribute 和 SetActiveConfig 都查找已向 SF 上报的 sf_modes_。
- 不采用该提交中单独改变 GetDisplayConfigs 计数的部分；计数和调用方容量语义需另验。
- 保留原 best/current mode 更新、客户端几何尺寸、显示更新顺序及返回语义。
  非标准／crop 路径直接调用原方法，未知／未上报配置仍返回 BadConfig。
- 使用已核对的原厂私有布局和方法回调；没有交换活跃 connector 的 vector，
  没有修改代码洞、fence、VOP2 原子提交、HDR 策略或 Frame Guard。

候选**尚未接入 hwc_bridge.cpp 或模块启动路径**，没有可用的运行开关或新版安装包。
私有布局源文件不得脱离原厂哈希和 ABI 检查直接应用到其他 HWC。

[隔离测试](../tools/hdr-probes/mode_cache_probe.c) dlopen 私有原厂副本，只建立测试
display／connector／mode 对象；mode setter 和 DRM 更新在测试进程内替换成记录函数，
不打开 HWC、不连接 compositor 设备、不写显示属性。
原厂与现用缩放修补副本各完成：

- 15 个原厂缺陷／对照检查。缓存存在 ID 11、原始列表只有 33／44 时，两个原方法
  确实返回 BadConfig；对照改为测试对象的同一列表后，原方法正常。
- 9,808 个候选用例，覆盖尺寸、刷新率、物理 DPI、分数刷新、无效属性、
  整个 display 对象写入及调用顺序等价，以及失配列表、未上报 ID、空／坏 vector、
  非标准路径及 crop 透传。

这些证明缺陷和修复语义在指定二进制上成立，**不能证明历史黑屏全部由此造成**。
历史 Exo 日志包含 `Could not find active mode for 11/8`、BadConfig 和
`initiateModeChange failed`，但没有出错瞬间两份模式表的完整快照。
硬件连接重训、hotplug 以及上层模式映射仍需区分。

下一步集成门槛：给 Decode Stack 增加默认关闭的独立分项，校验两种获准 HWC 哈希和
符号／导入槽 ABI，在启动前选择候选；关闭并重启恢复原方法。先记录实际请求 ID 与
两表对应关系，再在有界测试中验证原 Exo 场景、24／30／60Hz 来回、恢复桌面、
PQ／HLG／SDR 及失败恢复。实机未通过之前，保留原固定 60Hz Frame Guard。

## 本轮边界与来源

新增的是根因证据、Decode Stack 所属 HWC 源码候选和隔离回归，**不是已交付升级**。
没有拿旧版本通过记录冒充新候选验证，没有改应用、boot、Zygisk 或 Mali 设置。
原始 APK／库、设备快照、日志和运行脚本仅留在忽略发布的私有 dist 目录。

- [FFmpeg 对应修订的 MediaCodec 映射及复制](https://github.com/FFmpeg/FFmpeg/blob/bf4d5037b4/libavcodec/mediacodecdec_common.c)
- [原 CLIENT HDR 保护及 AOSP 依据](hdr-client-032.zh-CN.md)
- [0.7.8 的验证边界](decode-p010-pairs-078.zh-CN.md)
- [本轮机器可读检查记录](validation/playback-root-causes-20261018.json)
