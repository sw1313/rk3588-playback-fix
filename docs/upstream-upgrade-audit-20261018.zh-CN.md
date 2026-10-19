# RK3588 上游修复核查：2026-10-18

本次按具体源码提交筛选可回移修复，保留 Android 12/API32、精确固件/ABI 合约及模块回退。项目名称、分支版本或仓库最近更新时间不等于实机兼容性。

## 按最初建议顺序核对进度（2026-10-18 更正）

“已有局部交付”不等于完成整个方向。下表依据现有源码、模块说明与专项验收记录，
不以版本号、旧测试或旁支修复替代原目标。第 1～5 项各有局部实现，第 6 项未改内核，
第 7 项已有明确的用户态库升级。完整原目标没有按顺序全部完成。

| 顺位与原方向 | 实际已交付内容 | 尚未完成的原目标 | 进度判断 |
| --- | --- | --- | --- |
| 1 HWC2／VOP2 显示链路 | Decode Stack：HDR 状态及 0.7.2 blob 复用；0.7.5 缩放资格修正；0.7.6 HEVC 晚发现十位时保留 NV15 原生显示，限定场景 CLIENT→DEVICE；均有两级重启回退 | 完整最新版 HWC、通用图层规划与多图层优化；VOP2／DRM 内核；稳定显示延迟改善（本次 P50 仍为 32ms 桶） | 合成策略局部实机通过；延迟收益未验收，VOP2 未升级 |
| 2 Gralloc／DMA-BUF／AFBC 传递 | Decode Stack：受限条件下 AV1 十位显示／纹理 AFBC；0.7.3 mapper 句柄复用；0.7.6 为符合条件的 HEVC 原生显示消除完整帧 P010 转换拷贝 | 整套 Gralloc 升级、通用端到端零复制、其他路径复制消除；整机带宽收益实测 | 局部完成，已有实际缓冲分配节省 |
| 3 Codec2 异步队列与缓冲管理 | Decode Stack：SDK 核心路由、异步停止／空块保护、0.7.1 生命周期及 flush/work 修复、0.7.4 独立输出缓冲配置通知；0.7.7 HEVC EOS 非图像输入归还 | 全部 seek／EOS 及 UHD 取帧节奏问题解决；通用 seek 延迟／帧稳定性收益验收 | 局部完成，多项修复已集成 |
| 4 GPU DVFS／Mali 调度 | Mali Global：既有 g18 用户态／KMD／CSF 配对、首次绑定；0.3.1 的负载轮询 100→20ms | 调度器／抢占／多负载公平性等实际改进；进一步 DVFS 修复和实际负载性能收益 | DVFS 局部完成，进一步调度优化未完成 |
| 5 HDMI HDR／刷新率管理 | Decode Stack：限定路径 HDR 静态信息透传、PQ／HLG／SDR 切换；旧 Frame Guard 写 60Hz 设置并关闭自动内容匹配 | 按内容及显示能力匹配 23.976／24／25／50／59.94／60Hz，切换与恢复策略、HDMI 驱动／热插拔修复 | HDR 局部完成，自动刷新率管理未做 |
| 6 VPU 内核驱动补丁 | 用户态 MPP／libvpu 已有 reset/CSD、VP9、HEVC 等选择性修复 | RKVDEC／VPU 内核驱动补丁构建、集成及实机回退验收 | 内核项未做；用户态修复不计入 |
| 7 更高版本 libyuv／RGA | Decode Stack：librga 用户态 1.9.2_[1]→1.10.6_[3]；libyuv 1974 基线上回移缩放溢出修复，保留原 ABI | 进一步新版源码筛选与收益验收；RGA 内核仍为原版 | 已有用户态升级，未持续追到所有最新版 |

第二张内核组件表对应的实际状态：Mali KMD／CSF 已做配对更新和首次绑定；
VOP2／DRM、RKVDEC／VPU、RGA 内核、DDR Devfreq／DMC、HDMI 内核驱动均未升级。
GPU devfreq 的 20ms 轮询不属于 DDR／DMC 升级。修改 boot 启动载体也不等于升级这些内核组件。

0.4.2 的 `194a7d5a7567` 仅为 CSF fence 事件追踪初始化缺陷修复。
它不改 DVFS 或 GPU 队列调度性能，归为旁支稳定性修复，**不计为第 4 项新增性能进展或完成依据**。
原先把该补丁写成“顺序更新第四项”、继而直接推进第五项的表述不准确。

核对依据：[HWC](decode-hwc-reuse-072.zh-CN.md)、[AFBC](decode-texture-afbc-070.zh-CN.md)、
[Gralloc](decode-gralloc-transfer-073.zh-CN.md)、[Codec2 生命周期](decode-lifecycle-071.zh-CN.md)、
[Codec2 通知](decode-output-delay-074.zh-CN.md)、[DVFS](mali-dvfs-031.zh-CN.md)、
[HDR](hdr-metadata-030.zh-CN.md)、[用户态库升级](decode-upgrade-040.zh-CN.md)。

## 本轮直接采用的参考

[Lineage-Rockchip Codec2](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/tree/lineage-23.2) 的提交历史包含本项目已固定的 `e3c30156793f...` 基线，其后有一组由 Rockchip 作者署名的修复。候选保留 1.14.16_[0] 基线，选择性回移以下内容，不宣称升级为整套 1.14.18 或 Android 新版：

| 提交 | 修复内容 | 本次回移边界 |
| --- | --- | --- |
| [894f5b7ca773](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/894f5b7ca773c646758ba91fbdea6eb5370f17e4) | 输出图形缓冲所有权转移 | 用 `takeBlock()` 移交 Surface 输出块，避免 `getBlock()` 返回副本时的额外保留 |
| [e577f69d04d7](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/e577f69d04d7435a8e4a1088337ff2ed92a3a7e1) | 输入/输出 work 计数不一致 | 仅回移普通空输入的完成处理；其余初始化及 EOS pending 清理重构未一并引入 |
| [f5e25f2fbf34](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/f5e25f2fbf34ee41dbd083af6f58d6c6b22156cb) | flush 遗失 work 索引 | 仅回移 flush 标志与 pending 取出的队列锁排序；tunnel/drop 标志重构未引入 |
| [9581b4ac11fa](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/9581b4ac11fae9ee81467bec3cc542466593bee8) | release 前的 flushing 状态 | 在 handler 之前设置状态，返回后清理 |

[b033745ace1d](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/b033745ace1d5fde331a274c841f52c04a14ba7c) 的异步 EOS/OutBlock 竞争，已有同等的停止屏障修正，不再次计为新增升级。

另增加本地取消保护：缓冲池不断返回 `C2_BLOCKING` 时检查生命周期状态，遇到 flush/stop/release 退出重试；快速返回的池做最小退避。它不设置合法暂停超时，也不能中断卡在驱动内部永不返回的调用。这部分是本地配套实现，不标为上游原补丁。

以上候选集成到 Decode Stack 的独立 `codec2_lifecycle` 开关，默认关闭，依赖现有新版核心。模块验收状态以 [专项报告](decode-lifecycle-071.zh-CN.md) 为准。

## 后续参考项目及筛选结果

| 来源 | 可借鉴内容 | 当前处理 |
| --- | --- | --- |
| [Rockchip MPP](https://github.com/rockchip-linux/mpp) | 解析、reset、缓冲及时间戳修正 | 本轮基线到 develop 比较得到 57 个提交。既有 HDR/VP9/reset/HEVC 修正已经回移；新增 [MPEG-2 大跨度 seek 时间戳溢出修正](https://github.com/rockchip-linux/mpp/commit/b7a06290dcd8) 可继续评估，尚未安装 |
| [Khadas Codec2](https://github.com/khadas/android_vendor_rockchip_hardware_interfaces_codec2) | 与既有 Android HIDL 核心同源的实现 | 当前固定 Edge 2L Android14 分支；另一个 Edge2 Android14 头落后于该基线，不按分支名直接替换 |
| [TinkerBoard Codec2](https://github.com/TinkerBoard-Android/rockchip-android-vendor-rockchip-hardware-codec2) | Android12 的 flush、低延迟及 Surface 兼容实现 | 供实现对照；不把较旧同类代码再次计为升级 |
| [ffmpeg-rockchip](https://github.com/nyanmisaka/ffmpeg-rockchip) | MPP/RGA 异步与零复制缓冲传递 | 适合参考 DMA-BUF/AFBC 的格式与生命周期处理；不替换原版播放器的 FFmpeg 库 |
| [Rockchip BSP kernel](https://github.com/rockchip-linux/kernel) | VOP2、HDMI、VPU、RGA 和 DMC 驱动修正 | 需按设备 5.10.66 的结构、符号及设备树筛选，尚未整套替换 |
| [Armbian 构建与补丁](https://github.com/armbian/build) | RK3588 Linux 内核补丁及板级修正 | 可作为线索，逐项追溯原始提交；Linux 显示/V4L2 接口不能直接当作 Android HWC2/MPP ABI |

最新 Lineage 分支的 AI-PQ/xpp 尺寸修正依赖对应输出路径；本固件未验证该功能，因此不为升级版本号引入它。输出延迟独立通知还需要与旧 CCodec 交互验证，未混入本轮小范围修复。

这里只记录代码依据及明确的集成边界；没有通过实机验收的项不写成已修复或可发布。原始 API 响应、设备日志、媒体和私有链接输入保存在忽略目录中。

## 顺序更新第三项：Codec2 配置通知（0.7.4-test1）

将此前待验证的 `2a4d75b7138f697b12ccef869cce45ec904bed50` 独立输出配置通知与
`34dd42c01675eeb92bbe0f2ed0b689f72c636a69` 保留索引／INCOMPLETE 修正一起回移。
不引入其缓冲上限及低内存参数变化，不更换整个 SDK。集成在 Decode Stack 的
独立默认关闭 `codec2_delay_notify`，依赖最新核心，保留原通知路径的重启回退。
具体来源、真实 listener 的零帧通知检查、尺寸变更、完整精度及异步／回退边界见
[0.7.4 专项报告](decode-output-delay-074.zh-CN.md)。

## 顺序更新第二项：Gralloc／DMA-BUF／AFBC 传递（0.7.3-test1）

筛选 Rockchip mapper 重构及缓冲重导入历史，明确拒绝已被上游撤销的 flush
取消重导入补丁。采用本地当前 OutBuffer 所有权转移，复用登记阶段已验证的
mapper 导入供 HDR 桥借用；每帧元数据更新、flush 清理、像素和 fence 保留。
集成在 Decode Stack 的默认关闭独立 `gralloc_hdr_lease` 项，具体来源、运行样本、
新版本八／十位精度和分项／整包回退结果见
[0.7.3 专项报告](decode-gralloc-transfer-073.zh-CN.md)。没有按项目版本号替换整个 Gralloc。


## 顺序更新第一项：HWC2 / VOP2 链路（0.7.2-test1）

本轮查阅 TinkerBoard Android12 HWC 的完整 HDR 相关历史及 AOSP 的具体元数据修复。`8719dc99b24751706c137df07c9244ec78f2d996` 的同事务路径已存在，`7009cc1909804ef707376441b5b3f986b492d66a` 的通用类/EDID 更改不适用于原厂 ABI，未重复移植。新增本地“重新读取当前 blob、完整字节相同才借用 ID”的优化已集成到 Decode Stack，独立默认关闭；保留原始提交和回退。完整边界、新版本实测及一次独立 Mali 守护失败记录见 [0.7.2 专项报告](decode-hwc-reuse-072.zh-CN.md)。VOP2 为内建驱动，本轮没有修改 boot 或把它标成可替换的 `.ko` 升级。


## 旁支稳定性修复：CSF 事件追踪（0.4.2-test1；不计原第 4 项完成）

当前 20ms DVFS 动态响应已存在，没有再计一次性能升级。核对 Rockchip
BSP 的具体后续提交后，选择回移 `194a7d5a7567395d37e11ffa9014589b83ba8aad`：
在 `dma_fence_init` 的追踪回调可能访问 timeline 前建立 KCPU metadata 引用。
新增 Mali Global 默认关闭的独立 `fence` 开关，由匹配新哈希的首次绑定载体
在加载时锁定。没有更换 CSF／用户态版本、放弃同步或关闭内存回收。
未选提交及依赖、新版本带追踪的实际同步／错误清理、十位精度、HDR、分项及
整包回退见 [0.4.2 专项报告](mali-csf-fence-042.zh-CN.md)。不作 FPS 提升承诺。


## 第一项明确目标补充：0.7.5 缩放图层合成策略

限定原厂 ABI 回移 Rockchip `e3e3d168e524da25620ef55da734e48c8e56afe6` 的两条过时资格限制移除。实机同一 641×360 动态线性图层放大至 4K60，由 CLIENT 转为 Esmart DEVICE；采样 CLIENT 合成 488/488→0/490，无 missed frame。post→present 中位数仍为 32ms 桶，不宣称显示延迟降低。十位/HDR 路径及分项/整包重启回退通过，完整最新版 HWC 与 VOP2 内核仍未交付。见 [0.7.5 新版本验收](decode-hwc-overlay-075.zh-CN.md)。


## 按收益优先推进：0.7.6 HEVC 原生显示

当前已交付的局部收益是 late-profile／非 AFBC 的原生 HEVC 显示消除全帧 P010 CPU 拷贝和 CLIENT GPU 合成；纹理、显式 P010、CPU 等请求保留原路径。限定 12 秒 360 帧负载 CPU 服务用时开启 0.28／0.27 秒、关闭 4.21 秒；194,918,400 个新版本十位样本一致，HDR 及两级重启回退通过。它是本地 API32 条件修正，未把已有 upstream NV15 路径或版本号重复计为新升级。详见 [0.7.6 验收](decode-hevc-native-076.zh-CN.md)。完整 SDK/HWC/VOP2、通用零复制及原播放器 GPU-only UHD 超时仍未完成。


0.7.7 选择性回移 e577f69d04d7 的剩余 EOS pending 清理，HEVC 实际输入遗留 8→0，未增加图片帧，独立开关、十位/HDR 和两级重启回退通过；详见 [专项记录](decode-hevc-eos-077.zh-CN.md)。这是 P3 具体所有权修复，不代替通用 seek 延迟或历史 UHD/HLG 超时验收。


0.7.8 新增本地 NV15→P010 NEON 成对解包优化；无新增直接上游提交，固定 Codec2 上游和既有补丁保留。当前实际必需转换路径有 CPU 工作量净下降，位深、像素、帧数、拷贝和 fence 不变，两级回退通过。详见 [专项范围与 HDR 限制](decode-p010-pairs-078.zh-CN.md)。不重复计入已经省掉的原生显示转换，不计作更新整个 libyuv/RGA/SDK。
