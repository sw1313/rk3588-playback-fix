# Mali Global 0.4.1-test2：Zygisk 启动兼容性修复

2026-10-18。本次已安装并完成限定实机验证，仍为实验版本，不作为稳定版发布。
Decode Stack 保持 0.7.2-cold2，未改动其运行组件或设置。

## 原因和修改

此前 0.4.0-cold1 为抢在图形服务前绑定 GPU，覆盖了两个 Zygote 服务并加入
pre-exec SIGSTOP。这样既干扰 Zygisk Next 注入监测，又丢失 Magisk 注入的
Zygote onrestart 动作。旧资格检查漏验注入兼容性，该候选已撤回。

本机使用外置 Zygisk Next 1.5.2 和 LSPosed 2.2.1。Magisk 内置 zygisk=0
在此配置下正常；修复没有启用内置 Zygisk，没有改动 Magisk 数据库、外置注入器
开关、LSPosed 模块选择或应用作用域。

修复集成于 Mali Global：只暂停 allocator、HWC、SurfaceFlinger 三个原始图形
进程；完全保留 Magisk 处理后的两个 Zygote 定义，不暂停、包装或重启 Zygote。
在 Magisk 模块执行前检查回退条件，先完成 Magisk post-fs-data，再等待首次
GPU 绑定屏障，随后允许 init 执行原始 Zygote 启动动作。

资格检查新增当前启动的双 ABI 注入状态及 LSPosed 存活检查。启动前已启用的
注入器会记入本模块的 boot UUID 状态，注入器事后自动禁用不能绕过检查；用户
在开机前主动停用则予以尊重。失败时恢复原始带 root 的 boot 并重启。

本机 Magisk 为 API 30700 的 :R 变体。实现依据固定的
[Magisk v30.7 提交](https://github.com/topjohnwu/Magisk/tree/e8a58776f1d7bdf852072ad0baa6eceb9a1e4aac)
中 rootdir.cpp、rootdir.rs、bootstages.rs 的插入顺序与 PostFsDataDone 幂等
保护，并在目标机确认重复派发不重复执行脚本。未声称 :R 与上游源码完全相同。
来源及本地修改范围见
[ZYGISK_REPAIR_UPSTREAM.json](../magisk-mali-global/ZYGISK_REPAIR_UPSTREAM.json)。
这次是启动时序修复，没有新增上游 GPU 驱动补丁。

## 本次实机结果

- 先恢复原始带 root 的 boot，确认 Zygisk Next 与 LSPosed 注入恢复。
- 安装最终 0.4.1-test2 后，两次冷启动均通过双 ABI 注入与冷启动资格检查。
  最终启动中，首次绑定屏障在约 7.757 秒通过，两个原始 Zygote 在约
  7.916／7.920 秒启动。三个图形进程保持首次放行的 PID。
- Next 报告 inject_state=1、两个当前 Zygote 状态均为 3、modules_with_issue=0；
  modules64／modules32 均包含 zygisk_lsposed，lspd 存活。
- 最终启动实际重新打开 Yamby，观察到 RKPlaybackFix 的
  `Loaded Yamby settings adapter ... installed=true` 及 `HW+, HW, SW` 菜单 Hook。
  这验证了应用内 Hook，不只检查管理器图标。
- 实际停用整个 Mali Global 并重启：独立预检恢复原 root boot，原 GPU 用户态
  校验通过，双 ABI Zygisk 与 LSPosed 仍工作。随后重新启用并重启，冷绑定、
  新 KMD／CSF、双 ABI 注入及 20ms GPU 轮询均恢复，无待重启项。
- 最终设备运行文件与已安装 ZIP 的清单一致。Magisk 安装后移除的安装专用
  customize.sh／README.md 不列作运行文件缺失；ZIP 自身完整清单全部通过。
- 用设备 MagiskBoot 独立解包原始和候选 boot，kernel、second、DTB 逐字节一致；
  构建器另外验证原有 22 条 cpio 记录完整保留。root 继续可用。

离线检查：34 项原生加载器夹具、8 项注入资格策略夹具、48 项 Android WebUI／
状态检查通过。夹具不等于实机异常写入或硬挂恢复试验。

## 交付和限制

已安装且实测的安装包固定为 `rk3588-mali-global-0.4.1-test2.zip`，SHA256：

`2c910b965e386989501ead2e3d8d07578a187a5f9f488f75e3dc36ae86af3faf`

保留该 ZIP 原字节；包内的 pending 文档是验收前快照，本报告和
[机器摘要](validation/mali-zygisk-repair-041.json)补充本次结果，未改写原验收输入。
新的源码快照包含本次实现、来源和验收说明。私有 boot、设备配置、地址、
原始日志及签名材料均不在公开交付中。

验收范围为这台固定固件及当前 Magisk／Next／LSPosed 配置的启动兼容性，
不代表所有 Zygisk 模块已逐一验证。现有第三方 App Settings 在 system_server
内仍有 Computer 类找不到的兼容性报错；本次没有擅自改其配置。

未重新执行与本修复无关的 HDR／性能基线，未以旧十位或 HDR 检查冒充新版本
验收。旧 HEVC HLG seek／EOS 的一次 Codec2 队列超时仍未解决。没有新增帧率、
HDR 或长时间稳定性结论，也没有改变位深、分辨率、帧率、fence 或播放器统计。
单 boot 分区的写入中断、硬挂或备份损坏仍可能需要物理 Loader 恢复。
