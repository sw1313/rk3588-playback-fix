# Decode Stack 0.6.3：AV1 十位 AFBC 输出与交付

本次改动针对已匹配固件的 RK3588 / Android 12 API32 / 原厂 5.10.66，
继续使用 Codec2 1.14.16 和已有 MPP 1.1.0 兼容版本。没有更换内核、修改 boot、
超频、重编码、削减位深、降低分辨率或帧率，也没有修改 Yamby、LSPosed、Mali 或 Frame Guard。
随[完整项目 v0.6.3](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.3)发布，附件包含全部四个组件。

## 定位与社区核对

- [MPP 的纯外部缓冲区模式](https://github.com/rockchip-linux/mpp)是 Android 零拷贝交付的既有路径；本次继续遵守缓冲区所有权和 fence，不绕过显示端同步。
- [AOSP Codec2 BufferQueue 实现](https://android.googlesource.com/platform/frameworks/av/+/refs/tags/android-12.1.0_r1/media/codec2/vndk/platform/C2BqBuffer.cpp)与 [V4L2 Codec2 的 return-fence 改动](https://android.googlesource.com/platform/external/v4l2_codec2/+/15e95a26fd0517ad10b3a997cd4f69544fa37e10%5E2..15e95a26fd0517ad10b3a997cd4f69544fa37e10/)提示池等待可能有固定轮询延迟。已实际测试 return-fence、异步补充输出块、先批量交付后补充等候选，但它们没有改善本片源，未合入。
- [MPP #503](https://github.com/rockchip-linux/mpp/issues/503)报告硬件时间与 API 延迟不同，但不能直接认定同一根因。这里单独测量了输出、补充缓冲区、交付、消息等待及概率表读回；概率表可缓存候选虽通过精度测试，仍无播放收益，未合入。
- 新 SDK 的 RK3588 Codec2 压缩能力表没有列出 AV1，MPP 的 AV1 后处理器却已实现 AFBC。之前十位显示走线性 NV15；现在为已验证条件补齐 AFBC 能力选择。
- [MPP #935](https://github.com/rockchip-linux/mpp/issues/935)明确报告 AV1 AFBC 在宽度不是 64 倍数时异常，3840/1920 等宽度可工作。因此只给 64 对齐宽度启用，不能对所有分辨率强制开启。
- 之前核对过的颗粒、AFBC 非对齐宽度、PTS 重排、MP4/Opus 时钟问题均需分开处理。本次片源已用本地 WebM 原始 AV1/Opus 包复现；不依赖网络，也没有改 PTS 或播放器丢帧阈值。

有效修正是 MPP→Codec2→显示端的输出布局选择。AFBC 是无损压缩；显示端读取十位
AFBC 帧，HDMI 为十位 PQ/BT.2020。不能仅凭累计等待时间就把池等待认定为根因：
修正后解码器更快，反而会更频繁等待显示缓冲区归还，这是正常背压。
同源重启对照中，MPP `hw wait` 阶段平均从 15487µs 降至 12479µs。
该计时包含 HAL 等待与完成回调，不能冒充纯硬件周期；解析阶段平均值则约为
879/880µs，基本不变。本次对照不能独立量化后处理、内存总线与显示端各自贡献的比例。

## 功能边界

`av1_afbc=1` 只在 RK3588、native composer Surface、宽度为 64 倍数的 AV1 上
补充 AFBC，保持来源的八位/十位精度。继续尊重应用的 FBC-disable 请求。非对齐宽度、显式 P010、
Texture/CPU、裁剪输出和 tunnel 路径保留原有选择；不会用八位输出规避问题。

八位启动回归曾发现：初始化为保留精度先请求十位，而实际流被识别为八位时，
不应仅因位深改变就把压缩开关翻成线性，否则已排队 AFBC 帧与新缓冲区布局不一致。
修正后由真实帧格式选择八位/十位缓冲区，压缩选择独立于猜测位深；另做八位精度与启动测试。

AV1 后处理器可能因为滤波插入八行顶部填充。现在从每个完成帧读取实际偏移，
校验裁剪不超过缓冲区，再交给 Codec2；不再假定所有视频顶部偏移相同。

## 实际片源与精度

COSTA RICA 原始 AV1 3840×2160 / 60000÷1001 fps / 十位 PQ BT.2020，Opus 音频。
使用独立诊断应用加载盒子已安装 SmartTube 的原版 ExoPlayer 引擎播放本地文件。
测试引擎、缓冲策略、丢帧阈值和媒体包均未为 AFBC 改动。

| 同一 60 秒片源 | 渲染输出帧 | 播放器丢帧 | 跳到关键帧 |
| --- | ---: | ---: | ---: |
| 线性 NV15 首轮对照 | 1382 | 2215 | 3 |
| AFBC 首轮 | 3597 | 0 | 0 |
| AFBC 重进复测 | 3597 | 0 | 0 |
| 带保护条件版：独立开关关闭并重启 | 1163 | 2434 | 5 |
| 带保护条件版：独立开关开启并重启 | 3597 | 0 | 0 |

AFBC 原型在整段约 314 秒片源上得到 18808 输入、18808 渲染输出，播放器报告
零丢帧、零关键帧跳跃，正常 EOS；随后补充宽度保护与逐帧顶部裁剪。
最终 0.6.3 二进制在同一 60 秒片源上另外收到 3597 个呈现回调，解码 3597 帧、
零帧晚于目标 20ms，零个超过 25ms 的回调间隔，P50/P95/最大间隔均为 17ms
（框架回调时间戳的毫秒粒度）。这里没有把播放器交付计数当作物理显示器逐帧测量。
最终版重启开关对照、呈现回调和回退结果见
[结构化验证](validation/decode-0.6.3.json)。播放器计数和物理呈现回调分开记录。

独立 MPP 解码后由 RGA 解压 AFBC，十位平面逐像素比对软件解码参考：

| 样片 | 帧数 | 结果 |
| --- | ---: | --- |
| 1080p 十位，无颗粒 | 12 | 全样本完全一致 |
| 1080p 十位，胶片颗粒 | 12 | 全样本完全一致 |
| UHD 十位，胶片颗粒 | 6 | 全样本完全一致 |
| 实际 COSTA RICA 十位片源 | 6 | 全样本完全一致 |
| 4096×2160 八位 AV1 | 6 | 全样本完全一致 |

另测 Codec2 的 AVC、HEVC 十位 1080p/4K、VP9 十位、AV1 八位/十位基础输出，
全部与原参考精确一致。这里的逐像素验证针对表中样本，没有宣称整段 18808 帧都做了
软件逐像素比对。校验程序中的 RGA 十位展开输出使用 16 位样本及字节跨度，
不能按 MPP 的紧凑十位线性布局读取。

1080p 带滤波/颗粒的 240 帧循环片：AFBC，实际 crop top=8，零丢帧。
854 宽十位循环片：自动线性回退，240 帧、零丢帧。
4096×2160 八位启动回归修正后：单次初始化正常，360 输入、354 渲染、6 帧丢帧，
正常 EOS；没有把这段八位短片描述为零丢帧，也没有通过改阈值掩盖这些计数。

最终版还测暂停/继续、向前/向后 seek、EOS，以及 HDR10/HLG/SDR 切换。
主动 seek 测试会丢弃过时输出；这与连续播放丢帧分开记录。

## 开关与回退

在 root shell 中执行以下命令，重启后生效：

```sh
sh /data/adb/modules/rk3588_decode_stack/mode-control.sh av1_afbc off
sh /data/adb/modules/rk3588_decode_stack/mode-control.sh av1_afbc on
sh /data/adb/modules/rk3588_decode_stack/mode-control.sh status
```

关闭 `av1_afbc` 恢复此前线性十位显示路径；不关闭胶片颗粒或 HDR。
关闭 `av1_native`、`codec2_sdk_next` 时，依赖关系会抑制 AFBC，但保留请求选择。
整包 `all off` 或在 Magisk 停用模块并重启恢复原固件组件。不会热替换已加载的库。

非对齐宽度、Texture/CPU 输出的性能问题仍可能存在；本次没有把同一收益推广到所有
AV1、所有播放器、其他固件或其他芯片。网络加载、不同片源复杂度也不在本地对照保证之内。
