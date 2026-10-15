# 0.3.2 客户端合成 HDR 状态修正

范围是已校验哈希的原 RK3588 Android 12/API32 显示栈。Yamby、LSPosed 0.3.3、
解码后端、内核与 boot 均保持原状。沿用统一模块 `hdr_static` 开关和重启回退。

## 为什么客户端画面不能直接沿用源视频 HDR 标记

原系统仅向 SurfaceFlinger 提供 NATIVE 色彩模式。实机显示管理已开启，但客户端
目标是 `AB24` 八位缓冲、`UNKNOWN` dataspace。对应 AOSP 的受管理 RenderEngine
会在这条路径把 HDR 视频映射到 sRGB；HWC 却继续依据源图层及缓冲用途保留 HLG／PQ，
可能使 HDMI 的 HDR 状态与已经合成的画面不一致。0.3.1 为避免错误标记而跳过该路径。

对照复现：HLG 直接硬解后播放 640×360 PQ 视频，原版本保持 HLG；另一段无母版 PQ
客户端视频也保持先前 PQ。单独修改 HWC 收到的 dataspace 不足以纠正用途标记判断。

## 本版如何处理

适配层在原 HWC 完成真实平面规划后读取实际合成类型。仅同时满足以下条件才介入：

- 原 SurfaceFlinger 精确哈希、色彩管理开启、主显示且只有一个活动输出。
- 实际客户端目标为已验证的八位 AB24／UNKNOWN 原生 SDR 路径。
- HDR 源图层均由 CLIENT 合成，没有 DEVICE HDR 或未知合成类型。

随后只调用一次原 `SwitchHdrMode`，在该调用的当前线程内，将其读取自身
`vendor.hwc.hdr_force_disable` 的结果设为 1。没有写入全局属性；其他属性读取和
调用范围外读取均透传。原 HWC 自己清理 HDR 缓存、HDMI EOTF 与 colorspace，
切回可以 DEVICE 直出的视频时，原规划器和 HDR 判断自然恢复。

适配层共替换本进程五个导入数据指针，包含原有 DRM 事务接口及上述两个判断接口；
数据页写权限随后恢复，不改可执行指令、私有对象布局或内核。
安装前独立进程检查五个槽的写入／恢复，以及限定读取、其他键透传和离开范围后的
真实属性值。失败不安装；原子元数据提交失败仍沿用已有的原请求回退。

不改应用／源图层 dataspace，不增加第二次校验，不创建新的像素转换或中间 SDR 帧。
已有十位 DEVICE 路径继续优先。空客户端缓冲提交按原 HWC 规则保留先前有效目标，避免状态误清。
明确 HDR／十位客户端目标、未知缓冲、多屏和混合
DEVICE HDR 都跳过这条规则。

## 验证与仍有的边界

候选版六段直接／客户端切换、另六段含客户端 HLG 的切换，以及软件 AV1 的 PQ、HLG、
SDR 三段均通过暂停、seek／flush 和 EOS。六张对照 Surface 画面与 0.3.1 逐像素相同。
安装后 14 项硬解和 4 项软件播放通过；14 张硬解画面与旧版逐像素一致，
4,147,200 个可见 P010 样本最大误差为 0，1,884,604 个样本保留非零低两位。
1000／4000 nit、无母版信息 PQ、HLG 与退出 SDR 的直接路径元数据检查通过，
HDR10+ 三个非空载荷逐字节匹配，九帧及后续普通视频不残留动态信息。
21 项客户端策略测试与原 25 项事务策略测试通过。HDR 关闭重启核验 17 个挂载
文件并恢复原 HWC，整包关闭重启恢复 31 个原文件；最终构建重新启用后 19 个挂载
文件哈希一致，六段客户端／硬解切换再次通过。最终回归以统一模块的
`VALIDATION.json` 为准，不将候选测试或旧版本用户确认代替最终人工验收。

2026-10-15 用户确认：0.3.2 下原版 Yamby／HW+ 播放常用 HEVC 十位视频，
暂停、拖动、换片及退出重进均正常，画面、声音正常，无顶部灰线或整屏黑屏。
此确认限于上述日常播放检查，不扩展为所有编码、两种渲染器或 HDR10／HLG
客户端切换的人工验收。

**原系统八位 GPU 合成的精度限制仍然存在**。本修正使输出状态符合已经生成的画面，
没有恢复这条路径之前已损失的十位精度，也没有把原来可十位直出的路径强制转为 SDR。
后续若增加真正的十位 HDR 客户端目标，还需要独立验证 RenderEngine、gralloc、
平面格式、色彩管理和 HDR 输出的完整配套。

HDR 输出通过 DRM 状态及 HDMI InfoFrame 寄存器检查；没有外置 HDMI 分析仪或
面板光学测量。混合图层和多屏目前有软件策略检查，未全部完成实机覆盖。

## 开关

```sh
sh /data/adb/modules/rk3588_decode_stack/mode-control.sh hdr_static off
sh /data/adb/modules/rk3588_decode_stack/mode-control.sh hdr_static on
```

改后重启生效。关闭恢复原 HWC，其他解码组件和标准 HDR10+ 桥可继续启用；
整包 Action／禁用／卸载后的重启回退方式不变。

## 源码依据

- [AOSP API32 客户端输出 dataspace 选择](https://android.googlesource.com/platform/frameworks/native/+/android-12.1.0_r11/services/surfaceflinger/CompositionEngine/src/Output.cpp)
- [AOSP Skia HDR 色调映射与未指定传递函数处理](https://android.googlesource.com/platform/frameworks/native/+/android-12.1.0_r11/libs/renderengine/skia/SkiaGLRenderEngine.cpp)
- [后续解码组件升级核查](decode-upgrade-audit-032.zh-CN.md)
