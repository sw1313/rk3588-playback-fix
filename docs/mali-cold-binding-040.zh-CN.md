# Mali Global 0.4.0-cold1：图形服务前首次绑定候选

后续已执行用户授权的设备试启动；有限实测与未解决问题见
[设备验证补充](mali-cold-binding-device-040.zh-CN.md)。下文保留初次构建时的状态，
不能用它覆盖后续失败记录或当作稳定版声明。

状态：**已集成并构建候选，未刷 boot，未完成实机冷启动验收，不可作为稳定版发布。**
对应显示协调包为 Decode Stack 0.7.2-cold2。现有 g18 KMD、CSF、用户态和解码／HDR
二进制没有升级版本；本次改变的是启动绑定时序，不宣称新的上游驱动修复或帧率提升。

## 模块实现

Mali Global 新增默认关闭的首次绑定开关。仅接受已核对的 AGtv root boot，保留内核、
second、DTB 和原有 root cpio 记录；通过 cmdline 禁止旧内建 Mali 的初始化入口，
在 Magisk rootdir overlay 内放置独立原生加载／恢复程序和原服务定义的 pre-exec 门控。
分配器、HWC、SurfaceFlinger 与两个 Zygote 等待配套覆盖文件就绪后才执行。

post-fs-data 生成本次 boot UUID 的计划，实际挂载的 ARM32／64 用户态、CSF 和 KMD
均须匹配哈希；GPU 必须尚无绑定驱动。加载器核验五个暂停进程是未 exec 的 init 子进程，
首次绑定新版 KMD，再用 pidfd 放行这些初始进程。冷启动分支不运行旧交接控制器，
不 unbind GPU、不停止或重启图形／框架客户端，不改全局 firmware_class 搜索路径。

Decode Stack 等待首次放行回执，检查首个 HWC 加载的 HDR adapter；冷启动期间不会
因 adapter 尚未运行而重启显示链路。现有十位、HDR、AFBC、分辨率、帧率与 fence
处理代码不变；这些事实不能替代新版十位／HDR 实机验收。

## 开关、保护与边界

启用需先准备并校验私有 boot 及 root 原镜像备份，写入并读回校验唯一 boot 分区；
运行中的驱动不切换，重启生效。关闭该项恢复原 root boot，并选择原内核／CSF；
上级关闭、内核选原版和卸载也先恢复 boot。管理器整包停用／删除时，独立 ramdisk
程序在下一次启动恢复原 boot，可能再多重启一次。备份保留在模块目录之外。

加载限时、当前 UUID 回执、持久失败锁和图形／框架／音频／CSF 健康检查共同控制
资格确认。五个服务 PID 必须仍是原始放行进程。未确认的上次启动在下次进入恢复。
显式重试可清理旧启动的失败锁；冷启动写入失败不会暗中选择晚期交接。

原镜像恢复不依赖候选镜像仍然存在；拒绝覆盖无法识别的第三方 boot。写候选过程中
发现失败，会在仍持有操作锁时尝试恢复并校验原镜像。**硬挂死、写入中断、数据分区
不可读或备份损坏仍可能需要物理 Rockchip loader 恢复**，没有宣称硬件级救砖保证。

## 本次完成的验证

- 27 项原生隔离用例：实际执行 ARM64 程序，对普通测试文件模拟启动分区；包括写入
  中途失败后的原镜像恢复、拒绝第三方 boot、候选损坏／缺失时回退、依赖失配、
  已绑定 GPU、非 pre-exec 进程、模块停用／删除、旧启动锁与资格回执。
- 在盒子上用一次性子进程验证 pidfd 系统调用可用；不操作真实图形进程。
- 48 项 Android shell 状态检查，包含新开关依赖、写入失败不走旧交接、回退失败保留
  选择及参数边界；27 项浏览器界面检查，覆盖新开关禁用条件和中英文状态。
- 7 项 Decode 启动协调隔离检查：有效／延迟回执、过期／未放行回执、首个 HDR adapter
  缺失、HDR 关闭和原启动路径。185 个解码组件文件与上一候选逐字节一致。
- 候选由设备现有 MagiskBoot 独立解包；kernel／second／DTB 与原备份逐字节一致，
  22 个原 cpio 记录逐字节保留，五个 pre-exec 门控及加载程序／配置均匹配。
  MagiskBoot 仍识别 ramdisk 为已有 Magisk 修改状态。这不是实际 root 启动证明。

未完成：首次真实绑定、反复开机、实际开启／关闭／整包停用／卸载回退、完整十位 HDR
播放及游戏稳定性。旧版本验证只作为历史记录；不把它计入本候选验收。

## 来源与交付

使用 [AOSP Android 12 init](https://android.googlesource.com/platform/system/core/+/refs/heads/android12-gsi/init/)
的 sigstop、override、exec_background 机制，以及
[Magisk rootdir overlay](https://github.com/topjohnwu/Magisk/blob/master/docs/guides.md#root-directory-overlay-system)。
Radxa 的模块／overlay 启动选择提供参考思路，其 Debian 驱动包未直接移植。
原 g18 上游基线仍为 rockchip-linux/kernel
`7a76350c1e0df810d8c11eb0d18285da4e79e6ed`；本次启动适配为本地配套代码。

实际改动归入 Mali Global 和 Decode Stack 两个候选模块。私有 boot、root 配置、
设备地址和原始运行记录保存在忽略目录，不进入公开模块或源码包。
