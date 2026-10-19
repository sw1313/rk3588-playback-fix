# HWUI RGB10A2 映射：0.7.9-test4 的范围与实机结果

本轮已将 HWUI 格式 43（RGB10A2）的缺失映射集成到 **Decode Stack**，并在设备上安装
0.7.9-test4。它是限定固件的局部兼容修补，不是整套 HWUI 升级，也不代表显式 P010
的完整 HDMI HDR 链路已经修复。安装包仍为私有实验候选，未发布。

## 补了什么

实际 32/64 位 `libhwui.so` 的 `DeviceInfo::updateDisplayInfo()` 原来只接受 RGBA8888
和 FP16，收到 RGB10A2 时会以 `unsupported pixel format: 43` 中止 RenderThread。
新增 format 43 → `kRGBA_1010102_SkColorType`（本机 Skia 枚举值 7）的分支；
原有格式、字段写入、隔离进程处理及错误保护保留。

已核对 AOSP 12.1、13、14 的同一方法，均仍缺这个 case，因此这项是本地修补，
不能称为从新版 AOSP 回移的现成提交。依据及两 ABI 原始／生成哈希见
[配方契约](../magisk-hdr/contracts/hwui-rgb10.json)。汇编及逐字节配方随源码保留；
安装器只从设备上完整 SHA256 匹配的原库生成私有副本，不公开分发厂商库。

## 开关、依赖与回退

- 分项为 `hwui_rgb10`，默认关闭，依赖 `hdr_static`；本次实机最终状态为开启。
- 开关集成于现有 WebUI、配置迁移、启动挂载及重启状态显示。
- 关闭分项并重启恢复两 ABI 原 HWUI；整包停用并重启恢复原固件 HWC/HWUI。
- 既有 Frame Guard 为 0/60/60，未调整。未写 boot、未改 Mali、Zygisk 或游戏设置。

## 本次验证

| 检查 | 结果与边界 |
| --- | --- |
| 调用实际 HWUI 方法的隔离用例 | 每 ABI、每原始／候选版本各 69 项；RGBA8888/FP16 与错误分支保留，43 仅在候选映射为 7；不是完整绘制验收 |
| 格式 43 启动 | 两 ABI 修补后桌面恢复正常，未再出现原格式中止；不是所有广色域窗口的精度／稳定性验收 |
| 最终安装 | 0.7.9-test4 健康检查通过，32/64 位挂载哈希均为契约中的候选值 |
| 分项关闭后重启 | 两 ABI 原 HWUI SHA256 恢复，桌面及 Zygisk 健康 |
| 整包停用后重启 | 原 HWC 和两 ABI HWUI SHA256 恢复，桌面及 Zygisk 健康 |
| WebUI | HWUI 分项控制 14 项、HDR 控制及缺少 SF 前置门槛 16 项、浏览器交互 69 项通过；后者使用桥接 mock，Android shell 另验 |

最终安装 ZIP 的 SHA256 为
`410ee3cda61d5548734b0f9df80cc51fdbd066f9acc7762ab20f265caea7da9f`。
本记录是测试后的补充记录，没有修改已经安装验证的 ZIP。

## 为什么完整 HDR 仍关闭

实际 HDR 候选曾让 SF 选择 PQ、HDMI 报告 BT.2020/ST2084 和十位，
但真实合成／扫描缓冲仍是 **AB24 八位**，不能按 HDMI 状态标签认定完整十位通过。
当前 API32 `CompositionEngine::RenderSurface::initialize()` 硬编码设置 RGBA8888；
两个 SF composition-format 属性改为 43 也没有改变这块真实缓冲。

另外，HWUI `EglManager::createSurface()` 的通用广色域路径只为 FP16 特殊选择 EGL
配置；虽然显式 Hdr10 和 Skia 的 RGB10 分支已存在，仍需核对实际二进制及真实窗口的
RGB10 配置选择、色域／传递函数。补 DeviceInfo 映射不能代替这些配套。

`hwc_client_hdr10` 因缺少 `sf-target10-validated` 前置条件而被锁定为不生效；
本包不提供该标记，最终 requested/effective/active 均为 0。下一步需要物理 HDR
显示目标限定的 SF 配套，保留虚拟显示／投屏原格式，并验证真实十位像素、PQ/HLG、
SDR 退出、多图层和回退；不能用全局改成 43 或只强制 HDMI 元数据代替。

## 同轮其他修复的准确状态

HWC 的 `hwc_mode_cache` 已集成并启用，选择性回移 Rockchip
[`de31070c66c6`](https://github.com/rockchip-toybrick/rk_hardware_rk35_hwcomposer2/commit/de31070c66c61ee2bd90c42c6877d26c4c5d7c24)
的已上报模式表查找语义，不采用其模式计数变更。实机 ExoPlayer 2.19.1 窗口请求
24→60→30→60→24→60 均切换成功，渲染计数增长、丢帧计数为 0；
没有在该轮遇到两表失配，也没有完成屏幕像素验收，不能宣称历史黑屏彻底修复，
Frame Guard 保持原策略。

Yamby Main10 复制客户端已集成现有 LSPosed 的独立分项，不改 HW/HW+ 菜单。
源构建客户端的 90 帧、124,416,000 个 P010 样本与独立软件解码参考逐字节一致；
实际 Yamby 进程中加载也已确认。原播放器 HW 菜单的画面、seek 及关闭后恢复原库
仍须专项验收，不能把原生探针通过写成整个播放器绿屏已经修复。

## 后续纠正与新结果（2026-10-19）

以上为 0.7.9-test4 的历史记录。SF 初始化设 RGBA8888，并不等于后续请求格式
被永久锁死。0.8.1 已实证可以限定物理 PQ／HLG 改为 43，随后又发现原 `HW_FB`
分配分支会强制选择八位 AFBC。线性 RGB10 候选虽得到真实 AB30，但本机 HWC
Cluster 策略拒绝线性 RGB10，尚未通过显示验收；新的配套改为保留原 AFBC。
完整进展及失败边界见[0.8.1 记录](decode-physical-hdr-081.zh-CN.md)。

HW/Main10 原菜单、seek、最终客户端与已挂载框架的 4K／HLG 全字节比较及两级回退
后来已通过，见[缓冲池专项记录](decode-p010-pool-080.zh-CN.md)。Exo 新增跨分辨率
开／关对照和屏幕检查已通过，但历史故障仍未实机复现，Frame Guard 不变。
