# RK3588 Codec2 系统兼容模块 0.1.0

这是针对已验证 AGTV Android 12/API 32 固件的系统用户态适配模块。安装器检查原始 Codec2、HIDL、gralloc、框架库及服务的 SHA-256，任何未知版本都拒绝安装。它不是所有 RK3588 固件的通用驱动包。

## 修改范围

- 保留原厂 32/64 位 `libcodec2_rk_component.so`，增加基于公开 Android 12 Rockchip 源码回移修正的 ARM32 解码组件，并在原服务实例中选择路径。没有增加 VINTF HAL 实例，也没有升级 Android、内核、GPU 或 gralloc HAL。
- 支持的普通 AVC/HEVC/VP9/AV1 Surface 解码使用候选组件；显式请求 P010 的复制输出使用完整 10 位转换。没有添加 10→8 位转换、降低分辨率或码率。
- 旧式 ByteBuffer 客户端未请求 P010 时保留原组件。原版 Yamby 的 FFmpeg 复制路径不接受 P010，因此这个模块不能保证它的 HEVC Main10 **HW** 模式正常；其 **HW+** 仍是已支持的完整精度播放路径。
- 正确处理 AFBC 裁剪、AV1 线性 10 位 Surface、P010 色彩范围和 HDR 静态元数据，并在创建解码器时固定策略，避免播放中切换实现。
- 六处平台兼容补丁只处理 P010 字节序描述及无损复制输出。补丁在安装时由设备原库生成，逐项检查原文件、原字节及结果哈希。发行包不含固件原库或修改后的固件库。
- 安全解码、编码器及其余解码类型保留原组件。DRM、安全解码、相机、录像没有作为新增支持范围。
- 完整保留原厂工厂的 21 个组件入口，并在安装、启动时逐项检查；新增四个候选名称与它们分开枚举。VP8、MPEG-2、H.263 播放及 H.264/HEVC 编码对照通过。原固件 XML 未开放硬件 MPEG-4，测试延续其软件 MPEG-4 路径，没有擅自开放这个未验证的硬件入口。
- Yamby APK、mpv、FFmpeg 和现有 LSPosed 模块完全保持原样。

## 前提与开关

需要已验证的 MPP 1.1.0 与 RGA 1.10.6 库。测试固件原本为 SELinux Permissive；安装器拒绝尚未验证的 Enforcing 环境，不修改 SELinux。安装时和启动时都检查依赖哈希；启动自检失败会写入 `disable` 并重启一次恢复原库，该标志阻止反复恢复重启。完整停用回退已实测；故意改变模块自身依赖校验清单的故障注入也已验证自动停用、恢复原服务及完成启动，随后还原了清单。没有为了故障注入替换系统依赖库。

在 Magisk 安装 ZIP 后重启。默认 `quality` 模式选择最高已验证精度的兼容路径。设置保存于 `/data/adb/modules/rk3588_codec2_upgrade/mode`。

如需临时恢复原组件选择策略，以 root 运行：

```sh
sh /data/adb/modules/rk3588_codec2_upgrade/mode-control.sh compatibility
```

恢复优先最高画质：

```sh
sh /data/adb/modules/rk3588_codec2_upgrade/mode-control.sh quality
```

模式切换需要退出后重新打开视频；不改变正在工作的解码器。`compatibility` 保留平台 P010 补丁和服务适配层，只恢复普通组件选择策略；要**完整恢复全部原文件**，在 Magisk 中停用模块并重启。模块“操作”按钮也可以写入/取消停用标志，重启生效。卸载同样需要重启。

## 验证边界

`VALIDATION.json` 记录实际测试。最终 14 组合成样片覆盖 AVC/HEVC 8 位、HEVC/VP9/AV1 10 位、AV1 8 位、AV1 全范围、HEVC BT.2020/PQ HDR、VP8/MPEG-2/H.263 及原有软件 MPEG-4，尺寸包含 854×480、720p、1080p、4K。每组 Surface 测试包含暂停、恢复、flush/拖动和 EOS，并检查画面与颜色；不能仅凭有帧输出判定正确。

P010 精度使用原始 Y/U/V 数据与软件参考逐样本比较；8 位显示截图不能证明 10 位精度。另有同一解码器分辨率变化测试。HDR 元数据通过不等于所有显示器的 HDR 握手、动态 HDR、HDMI 10 位输出已经验证。长时间播放、所有文件、所有应用、8K、杜比视界、HDR10+ 尚未全部验证，不承诺零新问题。


用户于 2026-10-15 确认：使用原版 Yamby、HW+ 和常用渲染设置播放常看 HEVC 10 位及另一段视频，画面、声音、灰线和暂停/拖动/退出重进均正常。未确认具体 gpu/gpu-next 选择，不据此声称两种渲染器都已分别验证。

## 构建与来源

`tools/build_codec2.py` 使用 NDK 29、平台 VNDK32 头文件及准备好的 API32 SDK。Codec2 基线为 Khadas Android12 修订 `1e9d80a10b09c9a6e0bf5cc1dbe4b6d3e66dbc2b`，补丁在 `patches/codec2-p010.patch`，服务适配层在 `service-src/`，安装时补丁工具在 `src/patcher.c`。

服务复用此固件原 HIDL 1.1 实现；100 字节 ARM32 服务对象大小及导出接口受原库哈希约束。这一私有 ABI 依赖禁止将模块直接移植到不同 ROM。私有固件库仅作为本地链接输入，不进入 ZIP。公开源码适用 Apache-2.0，来源及固定版本见 `UPSTREAM.json`。
