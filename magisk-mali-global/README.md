# Mali Global 0.4.4 · GPU 驱动与游戏图形升级

**对应项目正式版 [v0.8.8](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.8.8)。** 提供全局 ARM32／ARM64 Mali g18p0 用户态、匹配内核／CSF、图形服务前首次绑定、DVFS 响应和 CSF 同步稳定性，分项选择、重启生效及回退。系统解码、HDMI／HDR 和帧率保留归 Decode Stack。

Global GPU upgrades for the matched RK3588 firmware: ARM32/ARM64 g18p0 userspace, paired kernel/CSF, pre-graphics first binding, DVFS response and fence stability. Version 0.4.4 belongs to formal project release v0.8.8. [English project guide](../README.md).

## 0.4.4 改了什么

root 兼容按真实接口和本机启动布局判断，**不再按 Magisk 版本号或官方 root 二进制哈希名单放行**。稳定版、测试版、Canary 和本地构建使用同一检查，不必登记版本。固件／ABI、原厂 init、个人原 boot 备份及回退归属保护继续保留。

当前 Magisk 30.7 已完成完整安装、重启、首次绑定及两个 ABI 的 GLES／Vulkan 验证，Zygisk Next／LSPosed 和原有选项保留。其他 root 版本及变体仅有离线回归；接口通过不等于全部版本均已实机启动。GPU／KMD／CSF、keeper 及运行脚本与已验收接口策略一致，本次不增加游戏性能声明。[验证范围](../docs/validation/release-0.8.8.json)。

## 谁可以使用

目标为 **H96 Max V58／RK3588、Android 12／API32、契约中指定的 5.10.66 固件**。同型号和内核版本只是起点，安装时仍检查真实系统文件与 ABI。

| 环境／差异 | 当前处理 |
| --- | --- |
| 同设备、同固件，root 工具版本不同 | 不按版本名单拒绝；按实际接口、布局和依赖检查 |
| Magisk 稳定版／测试版／本地构建 | 普通模块不设版本门槛；首次绑定使用现有 Magisk 适配并检查实际能力 |
| KernelSU／APatch | 普通入口不因品牌拒绝，仍需实际挂载与固件 ABI；专用首次绑定后端尚未实现 |
| 更换内核、DTB 或关键系统组件 | 需重新核对契约，未知 ABI 不能当成相同固件放行 |
| 每人自己修补的 boot | 不要求等于作者整份 boot 的哈希；私有适配和备份属于当前设备 |

固件哈希确认系统文件是否属于支持版本；个人镜像哈希保护备份和回退，两者均不同于已移除的 root 文件白名单。[详细兼容说明](../docs/root-compatibility.zh-CN.md)。

## 开关分别做什么

| 功能 | 作用与依赖 | 关闭并重启 |
| --- | --- | --- |
| 全局用户态 | 使用双 ABI g18p0 图形库；上级选择保留下级配置 | 恢复原厂用户态；依赖它的升级停止生效 |
| 内核／CSF 组合 | 选择原版或精确配套的新版组合 | 选原版恢复原 KMD／CSF |
| 图形服务前首次绑定 | 让配套驱动在图形服务首次启动前接管，需本机私有 boot 适配 | 恢复个人原 boot 并选回原内核组合，不转为运行中交接 |
| GPU 响应／DVFS | 负载采样周期 100ms→20ms，保留 OPP、电压、频率边界及温控，依赖新版组合 | 恢复原响应策略 |
| CSF 同步稳定性 | 修正 fence metadata 引用顺序，依赖配套内核和首次绑定 | 恢复原顺序 |

DVFS 采样周期与曾被否决的 CSF 调度 tick 方案不同。本项目没有交付后者，也未将 g25 列为当前主线。《传奇四叶草》及 Eden 的持续帧时间、温度、功耗和原厂 g13 对照仍需分场景验收；启动成功不能证明所有游戏更快。

## 安装、更新与状态

1. 从[下载页](../DOWNLOADS.md)取得 `rk3588-mali-global-0.4.4.zip` 并核对 SHA256，在兼容模块管理器中覆盖安装。
2. 安装器从固定上游地址下载并校验闭源 Mali／CSF；公共 ZIP 不包含这些闭源载荷。已有校验通过的缓存可复用。
3. 在 WebUI 查看保存配置与实际状态。更新保留分项选择，不自动全开；依赖联动及灰显不会删除原选择。
4. 重启后确认实际版本、启动结果和“待重启”状态。普通选择不热替换运行中的图形库。

从 0.4.3 更新无需重复安装 Decode／客户端，也不因版本更名要求重刷已匹配的私有 boot。未准备首次绑定的设备不能只装公开 ZIP 就获得通用启动适配。

WebUI 的“当前”表示本次启动结果，“重启后”表示保存选择；两者不一致时先看依赖及回退原因。只读状态命令：

```sh
sh /data/adb/modules/rk3588_mali_global/mode-control.sh status
sh /data/adb/modules/rk3588_mali_global/webui.sh status
```

## 首次绑定与回退

首次绑定的启用／恢复会写入并验证 boot，随后重启生效，不进行实时 GPU 解绑。具体准备、校验及启停步骤见[启动适配说明](boot/README.md)。

原 boot 与备份仓位于模块目录之外；整包停用／卸载后，独立恢复机制按归属检查恢复原路径。若其他工具在启用后再次修改 boot，拒绝用旧整镜像覆盖，需先核对新的 boot。不能用删除备份仓或绕过哈希检查来强行回退。

本模块不修改 root 数据库或外部 Zygisk／LSPosed 选择。相关注入是首次绑定启动验收的一部分；Frame Guard 保持用户关闭状态，不由回退流程自动开启。

[启动适配](boot/README.md) · [完整中文说明](../README.zh-CN.md) · [源码构建](../SOURCE_CODE.zh-CN.md) · [DVFS 验证](../docs/mali-dvfs-031.zh-CN.md) · [CSF 修复来源](../docs/mali-csf-fence-042.zh-CN.md)

<details>
<summary>历史版本与研发记录（不代表当前 0.4.4 的准入规则）</summary>

以下保留当时版本的原始说明。旧“实验／候选”、固定 root 版本及旧 keeper 要求仅适用于各自阶段；当前安装与兼容规则以上方正式版说明为准。

# Mali Global 0.4.3 · GPU 驱动与游戏图形升级

**0.4.3 正式版：严格验证固件，允许已支持的本机 root 打包差异。** 首次绑定不再要求整份 boot 等于单台设备的哈希，改为校验内核、DTB、启动参数、固件文件与关键服务。保留每台设备自己的 root、原 boot 备份和完整性哈希。匹配目标已完成实际安装、图形服务前首次绑定、整包停用恢复原 boot、重新启用及双 ABI Zygisk／LSPosed 验证。

仍需按本机原 boot 重新生成私有启动适配，不能把别人备份或旧 keeper 配套直接拿来使用；现有备份仓不会自动覆盖。启用后若其他工具再次修改 boot，自动回退拒绝用旧整镜像覆盖。GPU／KMD／CSF 执行代码沿用 0.4.2，本次不宣称新增游戏性能收益。[具体允许／拒绝边界与构建](boot/README.md) · [实机验收](../docs/validation/mali-boot-compat-043.json)。

项目的全局 GPU 升级模块，负责 32／64 位 Mali g18p0 用户态、匹配内核／CSF、图形服务前首次绑定、DVFS 响应和 CSF 同步稳定性。严格匹配固件并保留分项控制与重启回退。系统解码、HDR 和 HDMI 修复归 Decode Stack；游戏收益按具体场景验收。[完整安装与功能说明](../README.zh-CN.md)。

Global GPU upgrades: ARM32/ARM64 Mali g18p0 userspace, paired kernel/CSF, pre-graphics first binding, DVFS response and synchronization stability. Exact firmware required, with independent controls and reboot rollback. Per-game performance remains separately measured; Decode Stack manages playback/display repairs. [Full English guide](../README.md).

## 当前功能与验证范围

当前设备保持 g18 用户态／配套内核与 CSF，首次绑定在图形服务前完成；DVFS、Zygisk Next 和 LSPosed 双 ABI 正常。新版 WebUI 说明已按实际证据更新，不再用笼统“实验”代替范围：首次绑定有匹配固件启动／回退记录，CSF fence 有追踪与回退记录，DVFS 有有界 GLES 负载和回退记录。《传奇四叶草》与 Eden 的持续帧时间、功耗和温度仍需实际对照，不能承诺所有游戏提升。

用户态、内核／CSF 组合、DVFS 和 CSF 稳定性保留独立回退；涉及 boot 的首次绑定单独提示，不随普通分组开关隐式写入 boot。当前不启用 g25，不修改原 root 或外部 Zygisk 设置。Decode 的 QMS 配套校验与 HDMI 无黑验收是两回事。

以下为历史版本记录，以各记录的限定范围为准。

# 0.4.2-test1：CSF fence 选择性修复

默认关闭的独立“CSF 同步稳定性”开关回移 Rockchip `194a7d5a7567`，在
`dma_fence_init` 的事件回调执行前取得 metadata 引用。仅适用于当前配对
的 g18 CSF 路径，保持全部同步和原 20ms DVFS 选项，不宣称帧率提升。
需要重新准备匹配新 KMD／keeper 哈希的私有首次绑定 boot；公共安装包不含
设备镜像。关闭并重启恢复原顺序；整包停用保留独立载体的原 boot 回退。
详细范围与独立验收补充见 [0.4.2 报告](../docs/mali-csf-fence-042.zh-CN.md)。
下文保留先前启动修复及原版本的历史说明。

# Mali Global / 全局 Mali 图形升级

0.4.0-cold1 is withdrawn for a Zygisk Next regression. 0.4.1-test2 removes all
Zygote overrides/SIGSTOP and adds an init action barrier after idempotent Magisk
post-fs-data. Original Magisk-patched Zygote definitions are preserved. External
Zygisk and configured LSPosed now qualify before accepting the cold trial.
0.4.1-test2 now has bounded on-device verification: both-ABI Zygisk Next,
LSPosed, a real Yamby app hook, whole-module-disable rollback with injection
retained, and re-enable all passed. It remains experimental. See
[repair results and limits](../docs/mali-zygisk-repair-041.zh-CN.md).
Boot details: `boot/README.md`; old cold1 results below are historical.

Historical cold1 addendum (2026-10-18, withdrawn): controlled first binding, cold-switch
rollback and whole-module-disable restoration passed with the approved cold1
runtime bytes. One Codec2 seek/EOS queue timeout remains unresolved; this is
still experimental. See [bounded results and limits](../docs/mali-cold-binding-device-040.zh-CN.md).
Earlier pending statements below describe the initial candidate build.

Global ARM32/ARM64 g18p0 userspace with an **optional matched g18p0 kernel + CSF pair**.
The kernel option defaults to **original**. This is an experimental, exact-firmware upgrade
for the tested H96 Max V58 / RK3588, API32, original 5.10.66 build. It is not a generic
RK3588 driver installer. Kernel code fingerprints, import ABI, original CSF and platform
contracts must match; unknown builds are refused before the ownership transition.

全局 32／64 位 g18p0 用户态保持原有功能，新增配套内核／CSF 组合选择，默认使用原版。
冷启动候选会适配 boot，保留现有 root；显示协调改动归入 Decode Stack。不开启该项时继续使用原实验后端。

## WebUI and switches / 开关

Open this module in KsuWebUI or a compatible manager. The **English / 中文** button
remembers language locally. The parent switch selects global/original userspace.
The nested selector chooses the original kernel/CSF or the matched newer pair.
Turning the parent off retains the child selection and shows it as gray/inactive;
selecting the new pair enables its userspace dependency. Current and after-reboot
states, staged updates, startup checks and automatic rollback are shown separately.
Saving never switches live drivers. Reboot from the system/manager to apply.

在模块的 WebUI 中选择组合，保存后从系统或管理器重启。页面区分当前、重启后、
待安装更新、检查状态和回退结果；上级关闭时保留下级选择并灰显。选择新版组合
会补齐全局用户态依赖。原有 Action 仍切换整套原版／升级，不丢失下级选择。

```sh
sh /data/adb/modules/rk3588_mali_global/mode-control.sh status
sh /data/adb/modules/rk3588_mali_global/mode-control.sh kernel next
sh /data/adb/modules/rk3588_mali_global/mode-control.sh kernel stock
sh /data/adb/modules/rk3588_mali_global/mode-control.sh off
```

## 0.4.1-test2: first binding before graphics / 图形服务前首次绑定

**Experimental: injection/startup/disable rollback verified on the target;
new HDR and long-duration acceptance are outside this repair.** The
new, default-off switch requires a private exact-root boot prepared by the host.
Enabling writes and verifies boot, then applies on reboot. The independent
ramdisk keeper holds only allocator/HWC/SF before exec, loads g18
onto an unbound GPU, and resumes those initial processes. An init action barrier
delays the original Zygote start actions without overriding or pausing Zygote.
The cold path does
not call the legacy handover controller or restart framework consumers.

`cold off` restores original rooted boot and selects original KMD/CSF for the
next reboot. Whole-module disable/removal is handled by the ramdisk keeper,
which survives module disable. Backup remains outside the module; a hard hang
or interrupted boot write may still require physical loader recovery.

The matched Decode Stack candidate waits for this first release and checks the
initial HDR adapter without restarting HWC. Existing bit depth, HDR, AFBC and
fence behavior are preserved in code; historical tests are not new acceptance.

See [private preparation, switch semantics and recovery limits](boot/README.md).

## Legacy startup and rollback / 旧实验后端启动与回退

The original built-in kernel starts normally. After global userspace and existing
decode initialization pass, the module briefly reloads the desktop using the verified
client/HWC recovery order and takes over the GPU. Native audio is preserved. A local
observer requires the new KMD/CSF, stable framework and working graphics/audio IPC;
no PC connection, ADB or internet connectivity is needed. Both architectures run
OpenGL shader/pixel and Vulkan GPU/readback tests. Temporary supply references and
driver callbacks are released, and the original firmware search path is restored.

The attempt marker is synced **before** any transition. Success clears it. Failure
selects the original kernel for the next boot and requests at most one ordinary reboot.
An interrupted attempt/power loss is detected on the following boot and skipped.
The userspace upgrade remains enabled when only the kernel attempt fails. Turn the
kernel selector to original and reboot to roll back kernel/CSF. Disable/uninstall the
whole module and reboot to restore original userspace too. **A kernel hard hang may
still require a power cycle**; a userspace supervisor cannot guarantee hardware-level
hang recovery. There is no hidden-reference-drain proof or arbitrary-device support.

切换前持久保存失败标记，成功后才清除。失败或断电后的下一次启动会跳过新版内核，
保留全局用户态；关闭内核选项并重启即可恢复原内核／CSF。禁用整个模块并重启则
恢复全部原版。内核完全卡死时仍可能需要断电，普通用户态守护进程不能保证硬件级恢复。
启动时桌面会短暂重新加载。没有宣称完整 CTS、所有应用或长期功耗验证。

## Sources, binaries and scope / 来源与范围

The installer downloads separately licensed Mali userspace **and CSF firmware** from
fixed Khadas HTTPS URLs, checking size/SHA256. Neither closed component is bundled.
Verified local cache supports reinstalling offline. No account, address, identifier or
log is uploaded. The two kernel modules are GPL source builds; corresponding source,
compatibility headers, matching generated configuration, guards and build recipes
are in `mali-kernel/`. The project MIT license does not replace their GPL terms.

See [source/build guide](../SOURCE_CODE.md), [third-party notices](../THIRD_PARTY_NOTICES.md)
and [validation](VALIDATION.json). Decoder, plugin and Frame Guard installers are
carried unchanged in project v0.6.8. Existing high-bit-depth, HDR and film-grain
settings are retained; this is not a claim that every display path is ten-bit.
# 0.3.1-test1: optional GPU dynamic response

This experimental, default-off selection reduces simple_ondemand load polling
from 100ms to 20ms after the paired next kernel/CSF has fully qualified. It keeps
the existing governor, OPP table, minimum/maximum frequencies and thermal
protection. It is a one-shot boot policy with no tuning daemon. Select it in the
existing WebUI; enabling fills its kernel/userspace dependencies. Disabling a
parent preserves the saved choice. Save and reboot to apply or roll back.

This is a response-policy change, not a newer Mali driver build. It does not
establish game FPS improvements or solve player image acquisition timeouts.
Bounded installed-module validation and off/reboot rollback passed: the tested
24-burst workload took 181.57ms versus 97.24ms median draw+finish sum with the
option off/on, with the same readback hash. First-frame timing was unchanged;
outliers remain. This is a synthetic workload, not a game FPS claim. See
[results and limits](../docs/mali-dvfs-031.zh-CN.md), `VALIDATION.json` and the
packaged `GPU_RESPONSE_VALIDATION.zh-CN.md`. Previous 0.3.0 tests remain historical
evidence for the unchanged driver pair.

```sh
sh /data/adb/modules/rk3588_mali_global/mode-control.sh dvfs on
# Reboot to apply
sh /data/adb/modules/rk3588_mali_global/mode-control.sh dvfs off
# Reboot to restore driver-default polling
```


## Mali Global 0.4.2-test9 pair policy — post-install evidence

The unchanged tested installers now have bounded device evidence in [the current validation record](../docs/validation/decode-physical-hdr-081.json) and [scope/limits](../docs/decode-physical-hdr-081.zh-CN.md). Primary PQ/HLG explicit P01054 reaches real 4K AB30 AFBC scanout; actual queued ten-bit target, mixed SDR UI/virtual output and item/whole reboot rollback passed. RGB gray-reference differences remain up to 3/3/4 code values, so this is not bit-exact RGB or full colorimetric acceptance. Mali test9 changes only the exact pairing policy, with GPU/KMD/CSF binaries unchanged. Frame Guard remains the default workaround; the newly reproduced original Exo queue timeout/black screen is still under investigation. Historical sections above and embedded installer validation are build-time snapshots, not broader release acceptance.

</details>
