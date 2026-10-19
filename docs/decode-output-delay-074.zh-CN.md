# Decode Stack 0.7.4-test1：输出缓冲配置通知

本轮按顺序推进 Codec2 异步队列与缓冲管理，新增默认关闭的独立
`codec2_delay_notify` 开关，依赖 `codec2_sdk_next`，重启生效。最终验收状态以
`docs/validation/decode-output-delay-074.json` 为准。仍为限定固件实验版本，
不作为稳定版发布，不代表所有 seek／EOS 问题已修复。

## 修复范围和具体来源

现有 `configOutputDelay()` 可以在没有输入 work 的初始化／info-change 阶段
更新接口的缓冲需求，但旧通知只附着于输入 work，没有在这些阶段独立发送。
新路径单独发送配置 work，让框架及时获知需求变化。这里的 output delay 表示
流水线缓冲需求，不是毫秒延迟，也不是播放器帧率。

采用 Rockchip 作者在 Lineage-Rockchip Codec2 的两项配套提交：

- [2a4d75b7138f697b12ccef869cce45ec904bed50](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/2a4d75b7138f697b12ccef869cce45ec904bed50)：
  独立发送输出缓冲配置更新。本地只采用通知机制，不回移该提交的缓冲上限和
  低内存参数调整，保留当前参考缓冲及输出数量策略。
- [34dd42c01675eeb92bbe0f2ed0b689f72c636a69](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/34dd42c01675eeb92bbe0f2ed0b689f72c636a69)：
  为配置 work 设置保留索引与 `FLAG_INCOMPLETE`。两项一起采用，配置通知没有
  视频缓冲，不作为真实帧或输入完成，避免旧 CCodec 的索引／完成计数检查误判。

本地配套：对配置 work 的 ordinal 清零，保留关闭开关时的原输入 work 通知
路径；只有启用独立通知时才记录空 work 的已通知状态。没有替换整个新 SDK，
核心版本仍为原固定 1.14.16_[0] 加选择性补丁。具体范围和作者信息在
`magisk-codec2-sdk-next/UPSTREAM.json` 的 `output_delay_notification` 中。

关闭本项并重启恢复原通知路径。关闭新版核心会保留下级选择但抑制生效；
整包停用并重启恢复原固件。现有固件／ABI／HIDL／payload 校验和自动回退保留。
此前 Gralloc HDR 复用、AV1 AFBC、生命周期开关及 Mali 启动路径不因此撤销。

## 新版本专项证据

在目标设备以真实 C2Component／Listener 启动四种解码器，尚未提交任何视频输入：

| 解码器 | 接口查询的缓冲需求，开／关相同 | 关闭时独立配置通知 | 开启时独立配置通知 |
| --- | --- | --- | --- |
| AVC | 21 | 0 | 1 |
| HEVC | 16 | 0 | 1 |
| VP9 | 16 | 0 | 1 |
| AV1 | 10 | 0 | 1 |

通知值与接口查询一致，配置 work 的保留索引、INCOMPLETE、无图像缓冲及无输入
完成均通过实际 listener 检查。各项的真实帧数／输入完成数均为 0，未修改播放器
或其统计。这直接验证了原本缺失的通知，不将其转换成未经测量的 FPS 收益。

真实 MediaCodec 从配置尺寸 4096×2160 进入源内容 1280×720 的 HEVC 场景，日志
确认缓冲需求 6→16 在没有输入 work 的 info-change 阶段独立通知。没有缩放源
图像；最终解码尺寸为 1280×720。开／关两种模式均在先输出 12 帧后 flush／seek，
再完成 60 个不重复时间戳的真实输出、EOS 和停止，服务未崩溃。

开启时完成同 Surface 六段 HDR／SDR、暂停、seek、EOS 检查，共报告 690 帧，
覆盖 1000 nit／4000 nit PQ、无母版信息 PQ、HLG、SDR、重入 PQ。关闭并重启后
完成 PQ→SDR 对照。HDR 配置和 InfoFrame 仍符合十位／BT.2020 及各段字段要求，
没有外部光学 HDMI 测量结论。

新编译核心的实际 GPU AFBC 缓冲经同步 RGA 解压，UHD PQ 十位 6 帧及
4096×2160 八位 3 帧，共 114,462,720 个样本与独立软件参考逐样本相同。
保留源位深、低位、crop、分辨率和 fence；这是本次新生成的输出，不复用旧版
验收结果。独立控制夹具 15 项、浏览器交互 55 项检查通过。

异步 TextureView 使用独立 ffprobe 核对过的原文件：60.010 秒、3597 个视频包，
设备文件与本地源 SHA256 相同。首轮诊断将片段名中的 60 误作帧数，并使用了
过短的等待期限，原始失败／未完成记录保留，不计验收。正确预期值来自独立
输入计数，未修改旧报告的 complete 字段；重新运行时保留所有原始时间戳。

正确参数的两轮新采样均完成 3597 个输入包／3597 帧、EOS 和正常停止：关闭时
约 60.795 秒，重新启用后约 60.871 秒。两轮共享 Codec2 HAL 的 PID 均保持不变。
这些时长反映原时间戳的约一分钟播放，不作为性能提升数据；诊断等待期限为
95 秒。没有将框架输出格式中的默认帧率字段当作源文件的真实帧率。

## 回退、完整性与最终状态

关闭本项并重启后，四种解码器恢复 0 次首输入前通知，接口缓冲需求数值保持
21／16／16／10；新核心及此前 Gralloc／AFBC／生命周期功能保留。新版整包停用
后，原 HWC 与六项原平台库哈希恢复，注入的 Codec2 插件／HDR companion 消失，
新属性清除。Mali 首次绑定及双 ABI Zygisk／LSPosed 检查仍正常。

重新启用并重启后，再次通过四种解码器通知和上述完整异步播放。最终
`codec2_delay_notify=1`，Decode Stack 为 0.7.4-test1，启动检查通过、无待重启。
Mali 选择、20ms 轮询及 boot 字节保持原状态。设备活动挂载、模块文件清单、
安装包和内层组件清单均核验通过。

实测安装包 `rk3588-decode-stack-0.7.4-test1.zip` SHA256：

`6a0a73af4806289cd7f8316f2872139c26dc74ccc6490708abb5a05c6751a1ca`

安装包保留原字节；其内 pending 状态是安装前快照，本报告与机器摘要补充最终
结果，未改写原验收输入。最新源码快照包含两项回移、独立开关、诊断来源和
最终文档。临时诊断 APK 与签名密钥不随模块分发。

## 限制

本次修复配置通知缺口，没有量化 seek 耗时、总开机耗时或游戏／视频帧率提升。
历史 UHD 播放器取帧超时、HEVC HLG seek／EOS 队列超时和进程级 ashmem 留存
仍不视为解决。未验证全部应用、固件、长期负载、物理热插拔或 HDMI 光学输出。
私有日志、媒体、原固件、设备地址和诊断签名密钥不进入交付包。
