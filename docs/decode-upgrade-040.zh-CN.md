# 用户态解码驱动升级：decode-stack 0.4.0

与全局 Mali 类似，本次使用 Magisk 覆盖系统标准路径的用户态库，不依赖播放器
名单。范围仍限已验证的 RK3588 Android 12/API32 固件与原 5.10.66 内核。
调用这些系统解码入口的应用可使用升级；应用自行捆绑的私有解码库不一定经过该路径。

## 实际升级

MPP/VPU 保留 1.1.0 的已验证基础及旧版 HDR／JPEG 兼容补丁，再回移六个上游提交：

| 修正 | 目的 |
| --- | --- |
| VP9 概率更新缺少 `1+` | 修正概率值计算 |
| reset 后缓存并重放 extra data | 参数头已被消费后，重置仍能恢复配置 |
| 空 extra data 缓存保护 | 防止未分配缓存时的空指针解引用 |
| AV1 动态元数据缓冲容量 | 复用／扩大缓冲时按真实容量分配，限制异常 RPU 长度 |
| AV1 legacy HDR 元数据开关保护 | 仅在原功能已启用时填入 legacy 元数据 |
| HEVC 第一个硬件任务更新 PS/RPS | 防止早先帧被跳过后首次提交读取未初始化参数表 |

AV1 的缓冲修正处理原解析器已有输入，不启用 Dolby 输出或新增杜比功能。
未整批替换 develop 的新芯片、KMPP 编码或其他寄存器配置。补丁完整上游提交、
来源及差异随 `magisk-mpp/UPSTREAM.json` 与 `compat/007` 至 `012` 保存。

系统 ARM64 libyuv 从 `58d817ed81e8...` 更新到直接后继 `0b9e5c922dee...`，
处理缩放函数的有符号整数溢出；上游 API 版本仍为 1974。原有 710 个导出和
59 个被移除 API 的 Android 12 隔离兼容实现继续保留，不强制转为八位。

## 集成与回退

新版 MPP 仍成对提供 ARM32／ARM64 `libmpp.so`、`libvpu.so`，更新 Codec2 的
精确 MPP/RGA 依赖配置。Codec2 服务、组件库、RGA、软件解码器与 HWC 修正层
的运行二进制保留上一已验证版本；只增加新版依赖哈希识别。
安装器识别已验证的旧 MPP、VPU 和 libyuv，保留用户分项设置与全部回退开关。

独立预检先于系统覆盖。合并后继续使用整包与 `mpp`、`libyuv` 等分项开关，
重启生效；启动失败保留原有自动禁用／重启恢复机制。
全局 Mali 0.2.0 仍为独立模块，解码模块不会替换 GPU 或改变其开关。

## 验证记录

最终实机结果和本版本人工确认范围见 `magisk-decode/VALIDATION.json` 的
`decoder_040`，不将 0.3.2 的人工结果标为本版本确认。
专用 MPP 测试在双架构上一次提交 CSD，消费后连续两次 reset，不重新发送参数头；
三轮 H.264／HEVC 十位输出逐样本对比参考结果，并覆盖初始空 CSD。
libyuv 增加最大高度附近的 288 项平面／UV／ARGB 缩放与边界哨兵检查，
同时保留 227 项十位往返、旋转与 JPEG 回归。

本版本实机通过 36 项硬件 JPEG、14 项硬件播放、4 项软件播放、12 项 HDR／SDR
切换及 AVC／HEVC 编码检查。14 项硬件测试帧与原基线逐像素一致，P010 输出
逐字节一致且保留非零低两位。整包关闭重启恢复 31 份原解码文件，重新启用后
19 份覆盖哈希通过；全程保留全局 Mali g18p0。
2026-10-15 用户确认原版 Yamby HW+ 常用 HEVC 十位视频及另一段视频的暂停、
拖动、换片、退出重进均正常，画面、声音、顶部灰线和整屏黑屏检查通过。

不更换内核中的 VPU／RGA 驱动，也不通过修改能力位宣称新增硬件格式、VDPP、
Dolby Vision 或 HDR10+ HDMI 输出。完整新版 Codec2／gralloc／HWC 涉及另一个
平台 ABI，当前没有验证整套替换；软件 libvpx 后续补丁和可选 dav1d 也不计入本轮。
Yamby 原版组件、LSPosed、分辨率、码率和十位精度要求保持不变。

## 原始来源

- [Rockchip MPP 架构与驱动接口](https://github.com/rockchip-linux/mpp/blob/develop/doc/Rockchip_Developer_Guide_MPP_EN.md)
- [VP9 概率修正](https://github.com/rockchip-linux/mpp/commit/9db99ac77505ad745cb1f9cf3402cfe47a09b310)
- [reset 后重放参数头](https://github.com/rockchip-linux/mpp/commit/c5b8620973c7e22224fcc78ad5016740979c2e0a)
- [空参数头保护](https://github.com/rockchip-linux/mpp/commit/519ee751897a16a699d626759b2f4126c333c0ec)
- [AV1 缓冲容量修正](https://github.com/rockchip-linux/mpp/commit/c1ce7e1a612644e6684481afe8045d2bfa680aba)
- [AV1 legacy HDR 门控](https://github.com/rockchip-linux/mpp/commit/122235c79b60ad4d1f8c3a179384a841481fbe78)
- [HEVC 首个硬件任务参数更新](https://github.com/rockchip-linux/mpp/commit/8f88b50df0af9dc367bf2f9943808e82a8d14482)
- [libyuv 缩放溢出修正](https://chromium.googlesource.com/libyuv/libyuv/+/0b9e5c922deec7c9921ae385f6242a9a27754d6e)
