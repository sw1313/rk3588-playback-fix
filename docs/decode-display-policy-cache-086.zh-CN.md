# 系统分辨率／HDR 设置修复：ART 缓存启动故障

2026-10-19。test11 已通过两次真实启动，display_policy=1 保留，用户已确认 Auto／设置修复开关正常；QMS 播放排查已恢复，HDMI 实屏短黑仍未修复。

## 具体故障与上游依据

之前 jar-only 候选留下了 `/data/dalvik-cache/arm64` 下的 services 编译缓存。它的编译模式是 `verify`，但 VDEX 仍包含提取出的 DEX。system_server 在 fork 后启用可信 OAT 检查时因此 abort；随后 Decode 启动保护停用整包，未确认的 Mali 冷启动又恢复原 boot。这不是 WebUI 单纯显示错误。

Android 12 [OatFileAssistant::GetBestInfo](https://android.googlesource.com/platform/art/+/refs/heads/android12-release/runtime/oat_file_assistant.cc) 对不可写系统目录先选可用的数据分区缓存，再选预编译系统 OAT。因此 test10 虽已提供匹配的系统 OAT，旧缓存仍被优先选中。[OatFileManager::SetOnlyUseTrustedOatFiles](https://android.googlesource.com/platform/art/+/refs/heads/android12-release/runtime/oat_file_manager.cc) 对不可信位置的 AOT 或 DEX 都会拒绝，不能以 `verify` 模式推断安全。

也查阅了 ATV 修改 services.jar 时清理旧 odex/vdex 的经验，但不同系统版本的产物和信任策略不同，不能整包照搬。这里依据当前 API32 源码和精确原固件的真实缓存选择来修复，不修改 ART 的信任检查、应用签名检查或 Zygisk 选择。

## 隔离实机对照

在单独 mount namespace 中设置递归 private，绑定私有缓存副本、候选 JAR 和配套系统 OAT。只加载类，不构造系统服务、不注册 Binder、不操作播放器。离开子进程后真实系统文件和真实缓存的 SHA-256 均未变化。

| 候选容器 | 私有数据缓存 | 实际 services 加载来源 |
| --- | --- | --- |
| 原压缩候选 | 原缓存副本 | 数据分区缓存 |
| 未压缩且对齐 | 原缓存副本 | 数据分区缓存 |
| 原压缩候选 | 空私有缓存 | 配套系统 OAT |
| 未压缩且对齐 | 空私有缓存 | 配套系统 OAT |

因此改变压缩／对齐本身不足以修复本次故障。第五次对照执行模块实际使用的归档脚本，结果同样转为系统 OAT，未再映射该数据缓存。候选保留原 JAR 字节与配套编译产物，只增加有证据支持的缓存处理。

## test11 集成与回退

- 保留 test10 的精确 services.jar、services.odex/vdex/art，以及依赖 services 校验值的原 Ethernet 服务配套 odex/vdex；Java 行为修改范围不增加。
- `display_policy` 启用且配套产物／编译输入检查通过后，在 post-fs-data 阶段归档并移走两组精确缓存：services、ethernet-service 的 arm64 `classes.dex/vdex/art`。不清空 dalvik-cache，不动应用缓存。只有缓存实际存在时才建立私有备份目录。
- 非普通文件或符号链接拒绝处理；归档失败只撤回该设置项。备份保存在私有模块状态目录，后续启动不会删除。
- 启动确认仍要求真实 JAR／五份产物哈希相符、system_server 实际映射系统 OAT、系统完成启动。旧失败 pending 回退时归档，下一次明确启用须重新确认。
- 关闭此项并重启移除对应 JAR／OAT 覆盖，恢复原厂路径。缓存备份保留，原厂所需缓存由 ART 按原校验值选择或重建。
- 28 项 Android 隔离回退测试通过，包括旧缓存归档、不动其他应用缓存、错误 OAT／缺少实际映射不确认、失败后重新启用。它们不能替代真实重启验收。

当前 HDMI 音频事件适配保持关闭，其独立启动问题尚未解决。Frame Guard 保持 disabled＋paused；保留 Mali g18、DVFS、双 ABI Zygisk Next／LSPosed 和已有播放修复。

## 真实验收

首次启动和第二次重启均完成系统 OAT 映射与启动确认，display_policy=1 保留；实际设置接口读回 Auto 和 HDR 菜单开启，用户随后确认 Auto／修复开关正常。Mali g18／内核／DVFS 与双 ABI Zygisk Next／LSPosed 保留。固定模式持续锁定、HDR10 显式选择跨重启保存、完整 HDR 输出和 HDMI 实屏短黑分别验收，不由本次缓存修复自动计为通过。
