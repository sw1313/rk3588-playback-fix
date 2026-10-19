# Decode Stack 0.7.7-test1：HEVC EOS 输入归还

本项针对已经交给解码器、没有图像输出的 HEVC 输入 work，在真实解码 EOS 后仍未归还的问题。
独立开关 `hevc_eos_pending` 默认关闭，依赖新版 `codec2_sdk_next`，启用和关闭均需重启。
最终专项状态见 [验收记录](validation/decode-hevc-eos-077.json)。

采用 Lineage-Rockchip 的具体提交
[e577f69d04d7435a8e4a1088337ff2ed92a3a7e1](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/e577f69d04d7435a8e4a1088337ff2ed92a3a7e1)
中 EOS 清理 pending work 的部分。0.7.1 曾选择性采用该提交的普通空输入处理，
未包含此次 EOS 清理；本次没有把整条提交重新计为新增。

本地适配只覆盖 HEVC：实例创建时读取开关，真实 decoder EOS 时取出 pending work，
保留输入 ordinal，归还零图像缓冲的输入所有权；不生成视频帧。
先释放队列锁和状态锁，再批量调用客户端 listener，flush 已在进行时不抢占归还。
未采用该提交的其他 process/init/stop 变更，也不改变输出/参考缓冲、位深、格式或 fence。

## 实际复现与采用边界

直接加载模块实际挂载的 Codec2 核心，通过真实 C2Component、Listener 和缓冲池提交
原 HEVC 压缩包；前置 8 个合法 HEVC AUD 非图像输入。旧 0.7.6 正常输出 60 帧、
收到 EOS，仍有 8 个输入 work 未归还；不加 AUD 时没有遗留。
计数在 stop 前采集，避免 stop/flush 的归还掩盖 EOS 缺口。

这是输入所有权和生命周期修复，收益是 EOS 后不再遗留这类输入工作及其缓冲引用。
不据此宣称播放 FPS、游戏帧率或通用 seek 延迟改善，也不认定历史 UHD 取帧超时
或其他路径 HLG seek/EOS 超时已经修复。不是完整 Codec2 SDK、HWC 或内核升级。

## 回退和交付

关闭分项并重启恢复此前 EOS 处理路径；其他已选择的分项保留。
关闭新版核心时保留本项选择但抑制生效。整包停用并重启恢复原固件路径。
保留现有固件/ABI、payload 清单、依赖和健康检查。本轮不改 Mali、boot 或 Zygisk 设置。

安装 ZIP 保持实际安装验证的字节，包内 VALIDATION 是安装前候选快照；
与源码及最终验收附录一并使用。交付不含私有地址、原始日志、影片、APK、
签名密钥、boot 或原厂二进制库，不作为公开稳定版发布。

## 新版本专项结果

| 新核心实际重启状态 | 轮数 | 每轮图片/时间戳 | AUD=8 时 stop 前未归还输入 |
| --- | --- | --- | --- |
| 开启 | 4 轮无 AUD + 4 轮 AUD | 60 帧，PTS 集合完全一致 | 0 |
| 关闭 | 3 轮无 AUD + 3 轮 AUD | 60 帧，PTS 集合完全一致 | 8 |
| 再开启 | 3 轮无 AUD + 3 轮 AUD | 60 帧，PTS 集合完全一致 | 0 |

共 20 次新核心实例解出 1,200 帧，无重复输入完成或 listener 错误，stop/release
均为 C2_OK。开启时 56 次 AUD 回调内重新进入 queue_nb 成功，原 ordinal 保持，
归还没有图像缓冲。关闭态的同一复现重新留下 8 个输入，证明开关恢复旧代码路径。

新版本原生 NV15 的 UHD 梯度及普通 UHD、GPU 纹理 P010 和显式 P010 各 3 帧，
合计 149,299,200 个十位样本与独立软件参考完全一致；91,749,915 个样本低两位非零。
保留 acquire fence 等待和 DMA-BUF 读同步。PQ/HLG/SDR 分别完成 100/190/100 帧，
含暂停、seek/flush、EOS；DRM HDR blob、HDMI 寄存器报文和校验、HDR 十位/BT2020
及退出恢复 SDR 通过。这些是新核心的新运行，未把旧版结果复用为新验收。

分项关闭保留其他选择；整包停用并重启后原 HWC 和六项平台哈希恢复，新增伴随库
和 Codec2 插件消失，新增属性清空；重新启用后归还修复恢复。boot、Mali 0.4.2、
现有 DVFS 和外部 Zygisk Next/LSPosed 保持正常。安装 ZIP、八个嵌套组件清单、
安装后保留文件、生成载荷和挂载路径校验通过。设备最终保持本项开启，新安装仍默认关闭。

此结果是限定 HEVC EOS 输入所有权修复的实机通过，不等同于通用播放器性能、
所有 seek/EOS 故障或长期稳定性验收。实机安装包 SHA256：
`50a56590ddca3c413ed426faab0855be3c024e1f3de6d6eedecdc537eeae3ec5`。
