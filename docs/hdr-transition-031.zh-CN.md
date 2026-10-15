# 0.3.1 HDR10／HLG 切换修正

> 后续：[0.3.2 已补充原生 SDR 客户端输出状态修正](hdr-client-032.zh-CN.md)。
> 本文保留 0.3.1 的验证范围和当时限制。

范围仍为已校验原 HWC 哈希的 RK3588 Android 12/API32 固件和原 5.10.66 内核。
本次移除独立 Dolby 移植整理层、临时内核探针及其构建／校验工具和开发报告；
日常模块此前没有启用原生 Dolby 输出。MPP 原有元数据生命周期和兼容解码保留。
Yamby 原版组件、LSPosed 0.3.3、GPU、刷新率与 boot 均不调整。

## 原因与实现

原显示栈会缓存 HDR 是否开启，在同一 Surface 中 HDR10/PQ 与 HLG 互换时，
没有同步刷新输出 EOTF。0.3.0 与原 HWC 均能复现 HLG→PQ 仍输出 HLG。
只改校验层不足以修复，后续提交层也缓存了 HDR 开／关状态。

0.3.1 保留原 HWC 的 HDR 开／关、面积、合成、格式和 fence 策略：

- 记录标准 HWC2 校验结果，要求实际 DEVICE 图层的 HDR transfer 一致。
- 核对同一次 DRM atomic request 中有效 FB_ID、CRTC_ID 和 plane EOTF。
  精确匹配的原 HWC 对 PQ plane 写 2、HLG plane 写 0；connector 分别使用 2／3。
- 只在原 HWC 已选择 HDR 时校正 PQ／HLG；SDR、显式禁用、未知或混合路径透传。
- HLG 使用零母版／亮度值，PQ 使用当前静态元数据；TEST_ONLY 状态保留到正式提交，
  请求释放时清除。事务失败仍回退未修改的原请求。

不增加中间 SDR 显示帧，不修改内核对象或显示寄存器，不转换像素和降低位深。

## 验证与限制

原版对照已复现。25 项事务策略检查通过；候选版与安装重启后分别通过七段
同 Surface 顺序测试，整包回退再开启后又通过四段顺序测试，
涵盖 HLG↔PQ、重复切换、SDR 及退出；检查十位 BT.2020、DRM blob、完整 HDMI
静态 InfoFrame 字段和校验和，PQ 静态信息与 MediaCodec 字节一致，HLG 无亮度残留。
这些是寄存器与软件状态验证，不是 HDMI 分析仪线上测量。

**客户端合成路径仍有限制**：640×360 放大到 4K 的测试触发原 HWC 的八位 AB24
客户端合成，HLG→PQ 仍可能沿用旧 EOTF。修正对此主动跳过，不能把该路径写成
已修复或完整十位输出。该限制也不能靠更改 HDR 标识补齐。多屏、DRM／安全视频、
混合 HDR 图层、任意应用自建纹理及所有分辨率均没有全部验证。

14 项硬解、4 项软件解码播放通过暂停／跳转／EOS；14 张硬解画面与 0.3.0
逐像素一致。P010 的 4,147,200 个可见样本最大误差 0，其中 1,884,604 个
保留非零低两位。HDR 单独关闭核对 17 个挂载文件并恢复原 HWC，整包关闭
恢复 31 个原文件，再开启核对 19 个挂载文件。

完整安装、回退和回归记录以 `magisk-decode/VALIDATION.json` 的 0.3.1 项为准；
用户已确认 0.3.1 下原版 Yamby／HW+ 播放常用 HEVC 十位视频，画面、声音、
顶部灰线、暂停、拖动及退出重进均正常。这仅覆盖该次日常播放，不扩展为人工
HDR10／HLG 切换、客户端合成或任意媒体均通过。

## 开关与回退

修正沿用 `hdr_static` 开关，不增加空的 Dolby 开关：

```sh
sh /data/adb/modules/rk3588_decode_stack/mode-control.sh hdr_static off
sh /data/adb/modules/rk3588_decode_stack/mode-control.sh hdr_static on
```

修改后重启生效；关闭恢复原 HWC，开启恢复本修正。整包 Action／禁用／卸载后的
重启回退方式保持不变。其他 MPP、RGA、Codec2、libyuv、软件解码与 HDR10+ 桥保留。

事务策略检查源码为 `magisk-hdr/tests/transfer_test.cpp`，使用 C++17 编译执行。
