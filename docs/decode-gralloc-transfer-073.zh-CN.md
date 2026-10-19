# Decode Stack 0.7.3-test1：Gralloc HDR 传递复用

本轮按顺序推进第二项 Gralloc／DMA-BUF／AFBC 传递。已有 AV1 AFBC 输出保持不变；
新增的是 HDR 传递的句柄所有权优化，不是整套 Gralloc 替换或新的压缩格式。
实机验收状态和交付哈希以随附的 `docs/validation/decode-gralloc-transfer-073.json`
为准；本版本始终保持限定固件实验标记，不作为稳定版发布。

## 改动与回退边界

旧路径在将图形缓冲登记到解码器时已完成一次 mapper import，但 HDR 桥逐帧
写入元数据时还会再次导入同一缓冲并释放。新路径将登记阶段已经验证的导入
句柄交给当前 OutBuffer 管理，HDR 桥临时借用它，省去桥内的第二次 import/free。

它只在当前 C2GraphicBlock 对应的句柄仍匹配时借用；每次缓冲重新登记都会替换
旧句柄。flush、停止和缓冲清理沿用原有路径，释放对应导入。没有按整数 FD、
低位 buffer ID 或原始指针建立跨缓冲／跨实例全局缓存。借用方不释放拥有方的
句柄；内部拷贝、tunnel、缺少匹配句柄的路径继续逐帧导入。

HDR 数据不做缓存，SMPTE2086／CTA861.3 仍逐帧编码并写入，包括用于清除旧 HDR
状态的显式零值。HDR10+ 仍走原 C2Buffer／BufferQueue 路径。AFBC 布局、像素、
位深、crop、分辨率、帧率、同步 fence、MPP 登记与播放器统计均保持原样。
该改动减少 mapper 导入工作，不减少本来就不存在的额外像素复制，也没有实测
整机带宽或通用 FPS 收益。

独立开关 `gralloc_hdr_lease` 默认关闭，依赖 `hdr_static` 与 `codec2_sdk_next`，
包含中英文 WebUI、依赖补齐、当前／重启后状态、启动属性和失败清理。
关闭上级保留选择并抑制生效。关闭本项并重启，恢复桥内逐帧导入；关闭整个
Decode Stack 并重启则恢复原固件路径。Mali Global 的 boot、驱动与 Zygote 流程
没有改动。

## 上游核对与本地实现

- 固定基线仍为 Khadas Codec2 `e3c30156793f778541b6eaec54ac2a1ac0f6a70c`
  的 1.14.16_[0]，保留既有 API32、完整精度、AFBC 与生命周期补丁。
- [Rockchip mapper 重构 85ed287e8554](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/85ed287e855429aac08899a148fe0514440894e2)
  倾向使用通用 GraphicBufferMapper；其中 Android16／Gralloc5 重构不直接回移。
- [828d00b3a4a3](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/828d00b3a4a398deba0489e593817e3b2eec57af)
  曾取消 flush 后缓冲重新导入，但已被
  [1fb8bc6048b2](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/1fb8bc6048b23fdf14c603474ac7ad0bdd5c9e31)
  撤销。本轮不采用该补丁，不删除 flush 清理。
- 所有权遵循固定 [AOSP API32 GraphicBufferMapper 合约](https://android.googlesource.com/platform/frameworks/native/+/3e846409f064570d19769748a3050882c29fa999/libs/ui/include/ui/GraphicBufferMapper.h)：
  导入句柄在不再使用时 freeBuffer。新补丁是本地实现，不冒称上游原修复。

具体来源和选择范围见 `magisk-codec2-sdk-next/UPSTREAM.json` 的
`gralloc_hdr_transfer`，实现见 `patches/gralloc-hdr-lease.patch`。

## 本次专项检查

新编译 ARM32 核心在既有 API32 vendor HIDL 服务上通过模块安装的 ABI／平台／
完整性检查；控制层 17 项隔离 Android shell 检查和 53 项浏览器检查通过。
浏览器桥为模拟回调，和真实 Android 状态桥检查分别记录。

开启和关闭分项后的两次六段同 Surface 检查，覆盖 1000 nit PQ、4000 nit PQ、
无母版信息 PQ、HLG、SDR、重入 PQ，包含暂停、seek、EOS 及退出 SDR。每种模式
共报告 690 帧。两种模式的 HDMI 元数据字段一致；HDR 配置保持 10-bit／BT.2020。
这是系统／InfoFrame 观测，不是外部光学 HDMI 认证。

开启启动中抓到的 908 条传递记录全部借用当前导入句柄，桥内额外导入为 0；
关闭并重启后抓到 719 条记录均恢复额外导入，借用为 0。日志是有限采样，
不是完整帧普查；登记阶段的 import 仍保留，不能据此声称所有导入或延迟降低 100%。

新核心实际 GPU 消费端 AFBC 缓冲经同步 RGA 解压后，UHD PQ 十位 6 帧、
4096×2160 八位 3 帧，合计 114,462,720 个样本与独立软件参考逐样本一致。
按 PTS 匹配输出 crop，保留十位低位；八位与 uint16 数值比较，没有字节重解释。
这是本版重新生成的输出，不将旧版精度结果冒充新验收。

另外完成 flush 后重新送入 60 帧／EOS，以及实际持有输出缓冲再停止的六轮检查。
关闭本项后也完成持帧停止对照。服务存活和资源观察仅属有界检查，不作为长期
无泄漏证明；两种模式都有进程级 ashmem FD 留存，未将其归因于该优化或标为修复。

初次采样使用了旧 `hdr4000` 输入，没有取得有效 4000 nit 观测；该次不计验收。
随后明确使用 `hdr4000-direct` 和 `pq-no-master-direct` 完成上述两轮，并保留
初次失败记录，未通过放宽 HDR 断言获得通过。

旧播放器 UHD 取帧超时及历史 HEVC HLG seek／EOS 队列超时不因本次局部修改而
视为解决。未验证所有应用、动态多屏、长期负载或物理 HDMI 输出。
原始日志、影片、设备地址及固件输入只保存在私有忽略目录。

## 整包回退与最终状态

新版整包停用并重启后，原 HWC 和六项原平台库哈希核验通过，注入的 HDR
companion／Codec2 插件消失，新增属性清除。Mali 冷绑定与双 ABI Zygisk／LSPosed
检查仍通过。重新启用并重启后，Decode Stack 启动检查通过，新分项实际开启；
再次完成 PQ→SDR／seek／EOS，并抓到 209 条借用记录、0 条额外导入记录。
最终无待重启，Mali 首次绑定、配对 KMD／CSF 和 20ms 轮询保持原选择。

实测安装包 `rk3588-decode-stack-0.7.3-test1.zip` SHA256：

`2449a55cdc596210ccc5baead650942dd8c2a4da04aa610e07337a44b62261cc`

保留该安装包原字节；其内验收状态是安装前候选快照，本报告与机器摘要补充
最终结果，未改写原验收输入。最新源码快照包含本次补丁、固定来源和最终文档。
