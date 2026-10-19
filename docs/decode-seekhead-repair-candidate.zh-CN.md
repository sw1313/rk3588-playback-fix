# 快进返回片头：递归 SeekHead 根修范围

本项由用户重新明确要求修复，当前尚未修复。私有片源索引检查已发现第一 SeekHead 指向尾部第二 SeekHead，Cues 仅在后者，合法 Cues 与 HTTP Range 已核对。原厂 Codec2 及 MPP/RGA-only 对照仍回片头，不能把它归入新版解码器队列错误。

[Media3 #1143](https://github.com/androidx/media/issues/1143) 及 [Jellyfin Android TV #5221](https://github.com/jellyfin/jellyfin-androidtv/issues/5221) 记录同类文件返回零点的原因。[Cloudstream #1604](https://github.com/recloudstream/cloudstream/pull/1604) 与 [Media3 #2268](https://github.com/androidx/media/pull/2268) 提供具体改动参考；后者未合并，初版在三重 SeekHead 上丢失样本，后续仍有 unknown-length / IO retry 的 seekable 不一致。不能原样回移为稳定修复。

当前 APK 的 MatroskaExtractor 已定位，字段与方法契约已离线核对；候选已集成并安装到现有 LSPosed 客户端。解析修复可独立关闭，使用播放器既有数据源及真实 Cues，保证有界多级引用／循环检测、偏移溢出检查、可恢复网络异常以及最终回到原 Cluster 位置。不伪造 isSeekable，不修改 seek 请求、统计、片源或容器，不用重封装替代模块交付。

需要覆盖单／双／三重及分支 SeekHead、循环／越界／缺失 Cues、unknown length、短读取和中途 IO 重试，再验证真实用户入口及分项回退。候选不能改变既有 HW Main10/P010 和原生解码选择。


## 0.3.7-test2 本地适配候选

已集成到现有 LSPosed 工程，独立默认关闭的 `exo_seekhead`，修改后重启播放器／设备生效。匹配当前 APK 的 SHA256 及反射字段／方法签名，不修改原播放器 APK。关闭分项或 LSPosed 模块并重启即恢复原提取器。

在首次 Cluster 前借用原生 Cues 的 seek transport，在播放器既有 ExtractorInput 上只做有界 peek，查找尾部 SeekHead；获得并校验真实 Cues ID 后仍由原生 MatroskaExtractor 解析真实 Cues、生成 per-track SeekMap、返回原 Cluster。没有修改快进目标或伪造 seekable。

查找最多 64 个头、每头 64KiB、总计 1MiB；引用去重，偏移溢出／段边界／流长度校验。IO 失败不提交 visited 或解析游标，显式把 IOException 交回 loader；从零重启时清除本次遍历状态。畸形／循环／耗尽索引回到原始不可拖动路径，不继续误读视频为索引。上游旧补丁未直接照搬。

纯 Java 解析器的 8 项测试（含全部截断位置、分支引用、未知长度、越界／溢出、无效 vint、重复字段、非索引元素及重读）通过；现有裁剪 4 项也通过。尚未完成真实用户快进、网络重试、多级遍历和分项回退实机验收，不能标记已修复。完整 traversal 与播放器交互的验证不能由这些解析单元测试代替。

已保留当前安装的 0.3.6-test6 全部五个 P010 原生资源原字节；本候选不启用此前失败的帧率保持钩子。Frame Guard 继续关闭，短黑问题仍未修复。


## 本轮快进反馈与开关生效断点

用户反馈仍回片头；同次播放器进程启动日志明确记录 `Recursive SeekHead fix off; original extractor retained`。检查发现应用数据目录的配置已有 `exo_seekhead=true`，但 LSPosed 实际重定向的共享配置没有该键，默认 false。因此本次反馈记录为原解析路径仍失败，不能计为候选算法已经运行且失败。

已备份实际共享配置并补入该键，保留其余键值、所有者、权限和 SELinux 上下文；重启清除旧进程缓存。此前只写入应用数据目录的试验配置步骤遗漏了 LSPosed 重定向位置。设置界面原有 MODE_WORLD_READABLE／LSPosed 通道不需改成无条件启用；候选仍默认关闭。后续必须以新进程 `candidate armed`、真实 Cues 解析和用户实际快进三项证据验收，不能仅凭 XML 中值为 true 宣称已生效／已修复。

本次不更换 APK、Codec2 或短黑候选。Frame Guard 的 disable、外部 paused 标记及内容匹配策略单独记录；保持关闭。

## 本轮真实入口快进验收

配置修正并重启后，新播放器进程日志确认 candidate armed，随后记录 resolved to real Cues; heads=1。用户用原入口同一片源反馈“快进正常了”。本轮快进通过；分项关闭后的实机回退、网络异常重试、其他容器及长期 HDR 兼容性仍未完成，不据此标记可发布。
