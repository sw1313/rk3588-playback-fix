# RK3588 Decode Stack 0.8.6

项目的系统播放与显示升级模块，负责 Codec2／MPP／RGA 配套、AFBC 与缓冲传递、十位 HDR 合成输出、HDMI 兼容和内容帧率管理。按固件精确适配、分项选择、重启应用及回退；GPU 驱动升级归 Mali Global，应用特定的格式／索引修复归 LSPosed 客户端。[完整安装与功能说明](../README.zh-CN.md)。

System playback/display upgrades: codec and buffer paths, AFBC, ten-bit HDR, HDMI compatibility and content-rate management. The installer checks the required system dependencies and ABI, with per-feature reboot rollback; root versions are not whitelisted. GPU drivers and app-specific repairs are managed by the separate Mali Global and LSPosed components. [Full English guide](../README.md).

按用途提供“播放基础、原生播放稳定性、AV1、十位画质与 HDR、显示兼容与合成”组合，也可展开逐项选择。保留既有配置，启用时补齐依赖，修改后重启。关闭分项恢复对应旧路径，关闭整包并重启恢复原固件。

本版在 Auto 下保留已匹配的片源帧率，避免同一视频暂停／恢复／快进时反复切回桌面刷新率。实现位于系统 SurfaceFlinger 图层入口，覆盖旧 Binder 与 BLAST 事务；不是某个播放器的 LSPosed 帧率钩子。指定 Exo 场景的实际命中和 HDMI 实屏已由用户确认。新的正数片源请求仍更新，隐藏或删除图层不再参与原生选模。[验收与适用范围](../docs/decode-refresh-session-086.zh-CN.md)。

QMS 是独立的 HDMI 同分辨率变速支持，需要接收端、Auto、内核和启动配套。帧率保留减少不必要的切换，QMS 处理真正的切换，两者不能互相替代。换分辨率、HDR 信号格式变化或不兼容中间设备仍可能黑场。Frame Guard 固定刷新率会阻止内容匹配，不能用它开启条件下的结果当成本修复通过。

Frame Guard 已退出当前推荐组合，当前已验收的帧率保留配置不再依赖它，新安装无需再装。若旧安装仍固定 60Hz，先恢复其保存的系统值再停用／移除；只禁用模块不会清除系统限制。[恢复方法](../README.zh-CN.md#关闭回退与故障处理)。

原生队列背压、暂停恢复、十位格式支持及指定精度／回退已有各自记录。完整 HDR 色彩校准、所有应用／片源兼容和每项性能净收益仍分别验收；客户端 HW/Main10 格式与 MKV 索引修复在独立 LSPosed 客户端。

面向已适配的 RK3588 Android 12/API32 系统，安装器核对组件实际依赖的系统文件及 ABI；同固件下 root 版本或 boot 打包差异不作为整包拒绝理由。公开包不分发原固件镜像、闭源 Mali 或私有 boot。首次绑定适配必须预先准备匹配设备和 root 的私有启动文件；普通 ZIP 安装不会替用户生成通用 boot。未配置的低层显示功能默认关闭，升级保留用户已有选择。

当前完整发布是项目 **v0.8.8**，本组件仍为 **0.8.6 原包**，本次无需重复安装。普通模块入口不按 root 品牌／版本拒绝，但需实际挂载与固件 ABI；Mali 图形服务前首次绑定的专用后端由 Mali 模块独立检查。[root 兼容与限制](../docs/root-compatibility.zh-CN.md)。

WebUI 中“当前”代表本次启动实际配置，“重启后”代表保存选择；因依赖关闭而灰显的项目保留选择。启动检查只代表加载与依赖通过，不等于所有视频均验收。

[总目标和剩余项](../docs/main-goals-priority.zh-CN.md) · [系统显示记录](../docs/system-hdmi-blackout-086.zh-CN.md) · [源码构建](../SOURCE_CODE.zh-CN.md)
