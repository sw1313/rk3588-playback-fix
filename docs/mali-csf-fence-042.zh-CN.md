# Mali Global 0.4.2-test1：CSF fence 初始化修复

本项为 CSF 事件追踪的旁支稳定性修复，不计为原优先级 4“GPU DVFS／Mali 调度”
的新增性能优化或完成依据。此前把它表述为按原顺序推进不准确。选择性回移 Rockchip
[`194a7d5a7567395d37e11ffa9014589b83ba8aad`](https://github.com/rockchip-linux/kernel/commit/194a7d5a7567395d37e11ffa9014589b83ba8aad)。
当前冻结的 g18 源码尚未包含该修复：`dma_fence_init()` 之后才赋值
KCPU fence 的 metadata，而该函数的追踪回调可能立即查询 timeline 名称。
启用相关事件追踪时可能空指针崩溃。修复提前取得 metadata 引用；保留
sync_file 创建失败、FD 分配失败和正常释放路径，以及全部 fence 等待。

该修复直接适用于当前 CSF 路径，不需要升级 g21／g25 用户态、固件或 ABI。
本地配套是默认关闭、只读的 `csf_fence_safe` 内核参数，以及 Mali Global
中独立的“CSF 同步稳定性”开关。首次绑定载体在图形服务执行前读取 root
私有状态并传参；保存不会改变运行中的驱动。关闭并重启恢复原初始化顺序。
公共包不包含私有 boot，现有设备必须重新准备匹配新 KMD／载体哈希的启动配置。

其他上游候选的筛选结果：`9e16faf57bec` quick-reset 锁修复位于
`!MALI_USE_CSF` 路径，对当前 CSF 不生效；`6b494eb5a1af` 恢复 ondemand
私有数据的行为当前已有；`eaf544c6d909` 临时禁用 tiler reclaim 还涉及
指定固件组合，没有一并回移。`73b5dde37095` 涉及 simple-power-model
初始化依赖，本轮没有把它冒充当前设备已发生并修好的故障。

本项不修改 DVFS 的 OPP、轮询周期、温控和电压，也不宣称 FPS 提升。
原有 20ms 动态响应属于历史改动，不计为本轮收益。当前候选的安装前
状态为未验证；实机结果在同名验证记录／验收补充文件中单独记录。

```sh
sh /data/adb/modules/rk3588_mali_global/mode-control.sh fence on
# 重启后生效；需要已经准备匹配当前模块的私有首次绑定 boot
sh /data/adb/modules/rk3588_mali_global/mode-control.sh fence off
# 重启后恢复原初始化顺序
```

验证工具 `tools/gpu-dvfs/csf_fence.c` 使用当前 UAPI 创建 KCPU 队列、请求
SIGNAL fence、等待真实同步完成，再删除队列并检查 FD 持有的 metadata。
修复开启后可在独立 trace instance 中观测 `dma_fence_init`；原路径只进行
未启用追踪的对照，不故意使设备崩溃。工具不代替模块交付。


## 本轮实机验收补充

本机已安装并重新启用 **0.4.2-test1**，独立修复开关、首次绑定与原有
20ms 动态响应均开启，内核健康通过，无待重启项。Decode Stack 保持
0.7.4-test1，健康通过。全部下列结果来自本次新内核运行。

| 新版本检查 | 结果 |
| --- | --- |
| 开启修复，32／64 位真实 KCPU fence | 1,024 次完成同步；另执行 128 次 FD 分配失败清理 |
| 实际 `dma_fence_init` 追踪回调 | 1,152 次，timeline metadata 均有效，独立 trace instance 已清理 |
| 关闭修复并重启 | 参数为 N；512 次同步和 64 次 FD 分配失败路径完成，全程未开启该事件追踪 |
| 元数据生命周期 | 删除 KCPU 队列后，fence FD 仍能正确查询同步结果与 timeline |
| 新内核原生绘制 | 两个 ABI 的 GLES 精确 RGBA、Vulkan 精确 4,096 字节回读通过 |
| 八位／十位纹理像素 | 2 帧 3840×2160 十位、1 帧 4096×2160 八位，共 38,154,240 样本与独立软件参考完全一致 |
| 开启修复 HDR | PQ→HLG→SDR，报告 390 帧，暂停／seek／EOS 与退出 SDR 通过 |
| 关闭修复 HDR | PQ→SDR，报告 200 帧，相同元数据与退出检查通过 |
| 控制层 | 37 项原生启动隔离检查、59 项 Android shell 检查、39 项浏览器交互检查通过 |

HDR 同时核对 DRM 元数据 blob、HDMI 驱动十位／BT.2020 状态和包寄存器：
PQ 静态字段与解码输出相同，HLG 静态字段清零。本轮没有进行光学 HDMI 测量。
纹理精度检查保留真实输出尺寸和全部有效位，没有通过降低位深或工作量获得结果。

分项关闭后，仍使用配套 g18 内核、用户态及 CSF，仅恢复原 fence 初始化顺序。
整包停用并重启后，独立载体恢复原 root boot，next KMD 不再加载，原内建
GPU 重新绑定；两个原 Mali 用户态及 12 项原平台哈希一致，轮询恢复 100ms。
随后重新启用并重启，修复参数 Y、首次绑定和 20ms 动态响应均恢复。
原备份与上一版启动配置仍保存在私有回退目录，没有覆盖。

Zygisk Next 双 ABI 注入和 LSPosed 在开启、分项关闭、整包停用及重新启用后
均通过检查。保留先前修好的 Zygote 定义和图形启动顺序，本次未修改注入设置。
最终三个初始图形服务 PID 与首次释放记录一致；当前启动的相关内核故障扫描为零。
这些检查不构成所有应用／长时间运行无故障的保证。

安装 ZIP 的 SHA256 为
`5e91a0a549a4b635357c2f4d05cf609c0a0ff25c0ed4df75fae7810d7e205925`。
验收后保留该 ZIP 原字节；包内 `VALIDATION.json` 是安装前的未验证快照，
本节和 [结构化补充](validation/mali-csf-fence-042.json) 记录后续验收。
ZIP 内 117 项清单全部通过；Magisk 正常删除安装专用的 `customize.sh` 和
`README.md`，核对实际安装器后，对保留的 115 项重新完整校验通过，16 项内核
清单及所有活动挂载也通过。首次把这两个已删除文件算入安装后检查的尝试无效，
没有将其当作通过证据。

本项保持实验版：收益是消除已知 CSF 同步事件追踪的初始化缺陷，不宣称
游戏 FPS 或普遍播放延迟改善。没有故意运行会崩溃的原版带追踪路径；也没有
强制内核 OOM、长时间泄漏、全套 CTS／VTS 或所有播放器验收。历史 UHD 取帧
节奏及单次 HEVC HLG seek／EOS 队列超时仍保留为未解决问题。
