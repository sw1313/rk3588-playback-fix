# HW/Main10：P010 缓冲根因修复与实机结果

Decode Stack **0.8.0-test1** 的 `p010_copy_pool` 与现有 LSPosed
**0.3.6-test6** 的 `hw_main10_copy` 已配套安装，限定当前固件和播放器 ABI。
原 Yamby HW 菜单下的 Main10 播放、暂停／快进／恢复和两级重启回退通过。
这是限定 Main10 复制路径的修复，不外推到所有绿屏或完整 HDMI HDR。

## 根因及选择性回移

原客户端缺少 MediaCodec P010/54 映射和十六位行复制；框架同时把 P010 的
字节行距、UV 偏移和总大小重复乘位深，`AllocateEmpty` 却按八位 YUV420
分配。实际 720p 曾反复出现需求 5,529,600 字节、缓冲仅 1,382,400 字节的循环。
只修客户端不能解决框架池容量和布局错误。

框架选择性回移 AOSP 的
[`08a8a2b6402e`](https://github.com/LineageOS/android_frameworks_av/commit/08a8a2b6402e4492f1b5aad7615b83b96883d5d3)
（`codec2: Add flexible 10-bit YUV support`），仅取 P010 字节布局、单次位深计算及
24bpp 池分配语义，不取其他 Android 13 格式协商改动。当前私有二进制的配套是
三条精确指令修改和源构建的 `libcp10.so`；只适用于 SHA256 匹配的 API32 ARM64。
原 `libcutils` 依赖通过适配库保留，原所有权、fence、尺寸和统计不变。

客户端仍走原 Java MediaCodec HW copy-back，输出 P010LE。临时尝试的 NDK 强制切换
和 flush 强制重建均已撤销。未修改原 APK／库文件，未把 HW 改成 HW+，未降位深。

## 实际验收及边界

| 新增检查 | 结果 |
| --- | --- |
| 调用真实 `AllocateEmpty` | 原／候选各 36 项，覆盖非 P010、奇数尺寸、720p 与 4K |
| 原 HW 菜单实播 | 补丁库实际加载，日志为 `mediacodec-copy` / P010；画面有内容，未回退软件解码 |
| 暂停、快进、恢复 | UI 约 00:04→00:25，画面仍正常，未出现容量不足循环 |
| 当前最终客户端与已挂载框架 | 4K 三帧 37,324,800 样本及 HLG 720p 三帧 4,147,200 样本，每个可见 P010 字节等于软件参考；HLG 元数据保留 |
| LSPosed 配套保护 | test6 校验系统分项属性、实际框架和适配库哈希及四个原应用库哈希，不满足时使用原 loader |
| 分项关闭并重启 | 框架恢复 `88b7…`，客户端自动恢复原 loader；其他 Decode 分项保留 |
| 整包停用并重启 | 框架恢复原固件，客户端自动恢复原 loader |
| 重新启用并重启 | `f9e0…` 框架与 `dd02…` 适配库恢复，客户端重新加载 |

以上回退期间 Mali、Zygisk 双 ABI 和 LSPosed 健康，Frame Guard 保持 0/60/60。
三帧精度验证的程序已正确等待异步 EOS；没有把尚未排空的输出计为通过。
早期私有试验客户端的 90 帧结果与当前最终客户端的验收分别记录，不混为同一版本。

## 交付与回退

系统修复归 Decode Stack，客户端修复归现有 LSPosed。系统分项默认关闭，依赖
`codec2_sdk_next` 和 `codec2_buffer`；LSPosed 客户端分项也默认关闭。
关闭系统分项或整包需重启；关闭客户端分项后重启应用恢复原库。
安装时从设备生成私有框架副本，公开包不包含完整原固件、应用、影片、密钥或日志。

0.8.0-test1 已测试 ZIP 保持原字节，SHA256：
`40b715a9a82f85c73248ce60e9d16e8c91e9ede50b92bd6874c2483c0c0942f8`。
test6 APK SHA256：`46cbb35d9b023dbcdd64372dd193e54f13033d2ad3fe7e66a32f368ad77a9285`。
两者均为私有实验交付，未公开发布。
