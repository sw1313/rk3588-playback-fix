# Decode Stack 0.7.0-test2：AV1 纹理 AFBC

这版把可选 AV1 GPU 纹理 AFBC 输出集成进现有 Decode Stack，包括 WebUI、依赖补齐、启动属性及重启回退。安装包默认关闭该项，保留实验标记。它不修改播放器、原版应用库、GPU 时钟或 boot。

适用范围为已匹配固件的 RK3588 / Android 12 API32，GPU-only 纹理消费者、宽度为 64 的倍数，并启用原有 AV1 native / AFBC 及新版 Codec2 核心。CPU、编码器、显式 P010、tunnel、输出 crop 请求及禁用压缩的客户端继续使用既有路径。动态尺寸或压缩布局不再符合条件时，在 info-change 阶段切到完整精度内部拷贝路径。

## 新路径实测

| 项目 | 结果 | 范围 |
| --- | --- | --- |
| 3840×2160 十位输出分配 | P010 24,883,200 → AFBC 17,362,944 字节 | 单个实际 DMA-BUF，减少 7,520,256 字节，即 30.222% |
| 1080p 十位含胶片颗粒 | 12 帧，37,324,800 样本完全一致 | 其中 27,977,201 个参考样本低两位非零 |
| UHD PQ HDR 十位 | 6 帧，74,649,600 样本完全一致 | GPU-only AImageReader 实际 AFBC 缓冲经 RGA 解压后对比软件参考 |
| 4096×2160 八位 | 3 帧，39,813,120 样本完全一致 | 八位值扩为 uint16 对比，非字节重解释 |
| UHD 60 秒独立 TextureView | 3597 帧、EOS、正常停止，交付晚于期限 20ms 的帧为 0 | 仅解码交付，不能推导物理显示零丢帧 |
| 1920 初始配置 → 实际 854×480 | 240 帧、EOS，日志明确进入完整精度拷贝 | 验证动态不对齐回退 |
| 独立八位纹理启动 | 360 帧、EOS、正常停止 | 输出 crop 正确 |
| 关闭开关并重启 | 原 P010 0x36 恢复 | 同一核心二进制，无需替换库 |
| 再开启并重启 | AFBC 0x1b 恢复 | 与保留的 Mali 配对内核一起完成启动资格检查 |

精度共计 21 帧、151,787,520 个样本完全一致。诊断程序按 PTS 匹配逐缓冲 Codec2 crop；API32 AImageReader 的零起点 crop 不足以处理实际顶部填充。测试保持源位深、分辨率和帧率，不忽略 fence。

测试图形组合是 Mali Global 0.3.0 的 g18 用户态与配对 next 内核/CSF。其他图形组合尚未验证。启动播放前同时等待 `vendor.rkmaliglobal.ready=active` 和 `vendor.rkmalikernel.ready=active`；仅用户态 ready 不表示内核交接已经完成。

## 播放器边界及未解决项

原版播放器的 1080p 胶片颗粒短片进入了新 AFBC 路径并播放到 EOS，保留其原渲染器及既有裁剪修正。新的 UHD HDR 文件确认从 0 开始、使用硬件解码和 GPU 纹理导入，但约 20 秒采样中仍出现 AImageReader 取帧超时及显示间隔波动，不能标为 UHD 流畅性验收通过。启动期间上一文件的 seek/软件解码错误不能归到新文件；新文件自身的取帧超时则保留为未解决项。

最初旧路径与升级后的独立纹理测试存在投屏数量、温度差异，不能作为严格性能 A/B，不据此宣称帧率提升。SurfaceFlinger 的重叠采样窗口不是完整帧普查，渲染回调也可能回显计划时间。缓冲节省不等于整机内存节省、实测总线带宽下降或通用 FPS 提升。解码样本正确不等于播放器色调映射和 HDMI 全链路正确；没有新增外部 HDMI 仪器验证。

## 开关与回退

在模块 WebUI 勾选“AV1 纹理 AFBC”会补齐依赖，保存后重启。关闭上级会保留该项选择，并暂时抑制其生效。命令方式：

```sh
sh /data/adb/modules/rk3588_decode_stack/mode-control.sh linked av1_texture_afbc on
# 重启后启用
sh /data/adb/modules/rk3588_decode_stack/mode-control.sh av1_texture_afbc off
# 重启后恢复既有 P010 纹理策略
```

关闭该项不撤销已有 composer AFBC 或其他组件。整包停用/卸载并重启恢复原固件。开关不会热替换正在使用的库。

本地候选核心 SHA256：`a4190727f6cbe0a867d05f45c52cb8ef08c99da63bd11dc3fc53678de6012f74`。结构化结果见 [验证记录](validation/decode-texture-afbc-070.json)。源码与本地候选包尚未发布为正式版本，私人日志、媒体、固件原库、设备地址及签名密钥不进入交付包。
