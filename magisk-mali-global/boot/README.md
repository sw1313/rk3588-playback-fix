# 图形服务前首次绑定 · Mali Global 0.4.4 / v0.8.8

首次绑定让匹配的 GPU 驱动在 allocator／HWC／SurfaceFlinger 首次启动前完成绑定，避免运行中切换驱动造成交接干扰。它独立于普通用户态升级，需要根据当前设备的原 boot、root 和服务定义准备私有启动适配。公开安装包不包含 boot 或 root 二进制。

Pre-graphics first binding uses a per-device private boot adaptation. Version 0.4.4 checks actual root interfaces and local layout, without a root-version or official binary-hash whitelist. Firmware/ABI and personal rollback identity remain checked. See the [English project guide](../../README.md) and [build guide](../../SOURCE_CODE.md).

## 当前验证边界

| 检查对象 | 作用 |
| --- | --- |
| 原厂内核、DTB、启动头、关键参数、fstab／属性与原 init | 确认支持的固件及启动契约 |
| 图形服务定义与库／内核 ABI | 确认升级组件和服务配套；Zygote 原定义保留 |
| 本机 root 命令及 ELF／overlay 布局 | 确认当前适配所需接口；不按 root 版本／官方文件哈希放行 |
| 个人原镜像、生成镜像及备份哈希 | 确保适配属于当前设备，防止错误备份或覆盖外部修改 |

当前早期适配使用 Magisk overlay 和启动回调。只读能力探测查询帮助与运行目录，不把触发启动回调当作测试。标准统一／分离、压缩／未压缩载荷及允许的额外配置字段按实际布局检查；原 CPIO 记录逐字节保留。KernelSU／APatch 专用早期后端尚未实现，不影响其普通模块入口的独立检查。

当前 Magisk 30.7 已实机安装重启通过；其他稳定版、测试版及本地构建仅完成离线回归。历史 0.4.3 的固定 Magisk 30700 要求不适用于 0.4.4。[完整 root 边界](../../docs/root-compatibility.zh-CN.md) · [正式版验证](../../docs/validation/release-0.8.8.json)。

## 哪些情况下需要重新准备

- 新设备首次使用时，必须使用自己的原 boot 和服务定义，不能复制别人的备份。
- root 工具重补 boot、内核／DTB／启动服务改变，或升级包的 keeper／KMD 配对改变时，需重新核对配套；已有备份仓不会自动替换。
- 本次 0.4.4 的 keeper 与已验收版本相同，不因版本更名要求重刷已匹配载体。已有状态仍须通过运行检查。

整份 boot 哈希不再作为跨设备兼容门槛；每个人自己的原镜像与生成镜像哈希仍用于归属和完整性确认。gzip 时间／压缩级别和支持范围内的 root 打包差异不应单独造成另一固件判定。

## 私有准备与暂存

1. 检出要使用的发布标签，解开匹配安装包，保留本机完整原 boot、SF／HWC／allocator／Zygote 服务定义；这些输入不进入公开源码或附件。
2. 使用准备工具。参数名和范围可只读查看：

   ```text
   python tools/cold-boot/prepare.py --help
   ```

   实际准备需提供 `--original`、`--loader`、`--module`、`--sf`、`--hwc`、`--allocator`、`--zygote`、`--out`。`--loader` 使用所选包的 `bin/cold_boot64`。输出仅为本机待验收载体，不代表已验证启动成功。
3. 独立核对输出后，放到设备固定暂存目录 `/data/local/tmp/rk3588-mali-cold-prepared`；以 root 执行暂存，两个参数分别取私有输出中已核对的生成镜像和 profile 摘要：

   ```sh
   sh /data/adb/modules/rk3588_mali_global/boot-stage.sh EXPECTED_CANDIDATE_SHA256 EXPECTED_PROFILE_SHA256
   ```

暂存只检查并保存到 root 拥有的备份仓 `/data/adb/rk3588-mali-global/boot`，不写 boot、不启用功能。原镜像须属于当前 boot 分区；已存在的仓不会被覆盖，失败只清理本次创建的暂存。

## 启用、启动确认与恢复

准备完成后，在 WebUI 开启首次绑定。**这一步会写入并验证 boot**；重启后首次绑定生效，不修改当前运行中的 GPU。关闭首次绑定会恢复已验证的个人原 boot，并选择原内核／CSF，重启后生效。上级关闭、选择原内核、整包停用／卸载也遵循相应恢复流程，独立 keeper 检测停用后恢复可能增加一次重启。

启动确认包含匹配的用户态／KMD／CSF、GPU 首次绑定、双 ABI GL／Vulkan 和所需图形／音频／框架状态。预先记录已开启的外部 Zygisk Next／LSPosed 需求，检查两个 ABI 的实际注入与 lspd；不能因它们意外掉线就视作无需求。原 Zygote 服务保留，不修改 root 数据库或外部模块选择。Frame Guard 不自动恢复开启。

未确认试启动或关键检查失败时按现有框架恢复原 boot；外部工具改写过的 boot 不被旧整镜像自动覆盖。本流程不实现从任意外部修改的镜像中自动剥离本项目改动，也不自动迁移旧备份仓。内核硬挂、写入中断、数据分区或备份损坏仍可能需要物理 Loader 恢复，不能保证网络 ADB 始终可用。

## 构建与来源

当前构建入口与所需 NDK、固定基包、`--qms-barrier --source-ref v0.8.8` 配套见[源码说明](../../SOURCE_CODE.zh-CN.md)。重建历史版本请检出相应标签，不在当前代码上套旧命令。

设计参考：[AnyKernel3](https://github.com/osm0sis/AnyKernel3)、[Magisk overlay／启动脚本](https://topjohnwu.github.io/Magisk/guides.html)、[Android 内核模块 ABI](https://source.android.com/docs/core/architecture/kernel/loadable-kernel-modules)。参考启动选择与本机内容保留机制，不代表直接套用其他项目驱动或新增上游 GPU 修复。

<details>
<summary>历史启动集成与版本限制（仅对应当时实现）</summary>

下文原样保留历史记录。旧版固定 Magisk 版本、新 keeper 要求和“实验候选”不作为当前 0.4.4 的安装说明。

# Firmware-qualified cold binding

## 0.4.3: per-user boot compatibility release

The new source no longer requires one user's complete rooted boot SHA256.
It pins the firmware kernel, second stage, DTB, fixed boot-header fields and
critical command-line values. The host additionally checks the ramdisk fstab,
firmware properties, supported Magisk 30700 executables and original init,
plus normalized allocator/HWC/SF/Zygote definitions. Original CPIO records are
preserved byte for byte. Gzip timestamps/levels, raw newc ramdisks, supported
Magisk configuration values and unrelated extra files may differ. Tool-generated
images may omit the unused zero tail: preparation pads to the known partition
size, verifies all components, and staging must match the full live partition.

Each user's complete original SHA256 is recorded in their private profile;
candidate identity, verified backup and foreign-boot write protection remain.
The native keeper rechecks firmware components independently when verifying
the backup/candidate. Staging checks the live graphics contract, root API,
caller-supplied profile digest and that the original/candidate belongs to the
current boot. It does not write boot. Failed new staging removes only its own
files; an existing backup store is never silently replaced.

Custom kernels/DTBs, unknown root implementations, recovery/vendor_boot root,
unrecognized boot arguments, signed/footer layouts and conflicting early init
hooks still require separate adaptation. No generic KernelSU/APatch support is
implied. Repacking a boot *after* activation still changes its ownership hash:
automatic rollback will not overwrite it with an older full backup. This
release does not implement semantic removal of modifications from a foreign
boot, nor automatic migration/replacement of an existing private boot store.

The loader changed, so a newly prepared private boot is required before using
this release for first binding. Do not mix it with an old embedded keeper or
reuse the old QMS startup pairing without requalification. Published 0.4.2 assets remain unchanged.

Targeted checks completed: 37 real-image host cases, 13 independent native
SHA-256 vectors, 39 inert native restoration cases, 9 actual staging-script
cases with the native firmware reader, and 21 inert QMS rendezvous cases.
The matched device additionally passed real full installation, independent
embedded keeper/profile verification, pre-graphics first binding, whole-module
disable restoration of the exact original rooted boot, and re-enable. Both-ABI
Zygisk Next/LSPosed were retained. Frame Guard stays disabled/paused.
Different-root packaging cases remain offline/isolated validation, not physical
certification of other devices. Existing GPU/KMD/CSF bytes are unchanged; no new
game, HDR calibration or HDMI blackout result is claimed by this boot-policy update.
Machine-readable scope: `docs/validation/mali-boot-compat-043.json`.

`tools/build_mali_boot_policy.py --base <public-0.4.2.zip> --ndk <ndk> --work <fresh-dir> --qms-barrier --source-ref v0.8.7`
reuses only the exact published base ZIP's unchanged GPU/kernel/interface bytes,
rebuilds the keeper and stages the new policy/source with explicit release provenance.
The formal package includes `--qms-barrier` for the existing Decode pre-graphics QMS coordination. Omit both this flag and `--source-ref` only for an unreleased non-QMS build requiring separate acceptance. The
full `build_mali_global.py` builder supports the same flag. Preparation requires
the keeper to match the chosen package; generic and QMS keepers are not mixed.

Design references, not directly copied code: [AnyKernel3 local ramdisk handling](https://github.com/osm0sis/AnyKernel3/tree/020dfeccf9d7e962a48400fc94d3e451df92eead),
[KernelSU local backup/profile handling](https://github.com/tiann/KernelSU/blob/4663610011d03771a64e7c36a2aa65bd1794022d/userspace/ksud/src/boot_patch.rs),
and [Android module ABI limits](https://source.android.com/docs/core/architecture/kernel/loadable-kernel-modules).
The vendor firmware/ABI/private-layout restrictions remain independent of the
ramdisk packaging changes; no GKI-wide compatibility is claimed.

中文边界：严格验证固件组件，允许已支持 root 环境内的本机打包差异；按每人的原 boot
生成适配和回退备份。不是取消校验，也不支持未知内核或任意启动钩子。本版已在匹配目标上完成写入、首次绑定、整包回退和重新启用；其他 root 打包变体仅通过离线／隔离检查，不代表其他固件兼容。

## Historical startup integration (0.4.1-test2 / 0.4.2)

This is an **experimental candidate**, disabled by default. On the target,
0.4.1-test2 passed cold startup, both-ABI external injection, app-hook and
whole-module-disable/re-enable checks. HDR and long-duration validation were
not repeated for this sequencing repair. See `docs/mali-zygisk-repair-041.zh-CN.md`
in the source snapshot for scope and limits. This directory holds
documentation only. Original/candidate boot images and device profiles must never
be included in an installer or source archive.

The g18 CSF/userspace pair is retained. Version 0.4.2 additionally backports
Rockchip 194a7d5a7567 behind a default-off read-only KMD parameter. A new private
profile is required whenever KMD or keeper hashes change. The backend changes first binding,
not driver version: blacklist the built-in g12 initcall, gate allocator/HWC/SF before exec, mount the matched userspace/CSF, load the unbound GPU,
then resume those initial processes. No GPU unbind, client stop/restart, resource
guard or firmware search-path rewrite is used in the cold path. Original service
options, sockets, capabilities and restart actions are retained.

Prepare privately on the host using `tools/cold-boot/prepare.py --help`, the full
known rooted 48 MiB boot, the package's `bin/cold_boot64`, package directory, and
exact current SF/HWC/allocator/zygote service definitions (Zygote is fingerprinted
only, never copied into the generated rc). The builder refuses
unknown root boot, modified kernel or nonzero trailing signature data. It checks
the boot SHA1, preserves every existing cpio record and keeps kernel/second/DTB
identical. The output is not an installer and cannot replace Mali Global.

Install the matched Mali Global and Decode Stack candidates through the normal
module manager. Copy the private prepared directory to the fixed device path
`/data/local/tmp/rk3588-mali-cold-prepared`, then invoke as root:

```sh
sh /data/adb/modules/rk3588_mali_global/boot-stage.sh EXPECTED_CANDIDATE_SHA256 EXPECTED_PROFILE_SHA256
```

Both hashes come from the host's verified private output. Staging does not write
boot or enable anything. The root-owned backup store is outside the module, at
`/data/adb/rk3588-mali-global/boot`. It is retained on uninstall. Existing stores
are not overwritten by the staging command.

Use the existing WebUI's cold switch, or `mode-control.sh cold on`, to select it.
This **writes and verifies the sole boot partition**, but never changes the live
GPU. Reboot to try the cold path. `cold off` writes the verified original root
boot and selects original KMD/CSF for the next reboot. Parent off, kernel stock,
and uninstall restore boot too. Manager disable/removal is detected by the
independent ramdisk keeper on the next boot; restoration may add one reboot.

The keeper requires an exact current-boot overlay receipt, hashes for ARM32/64,
CSF and KMD, three paused graphics init children, and a GPU with no bound driver. Module
loading is bounded to 15 seconds. A persistent trial latch is cleared only after
the native GL/Vulkan probes and existing local framework/audio/CSF health policy
pass. A missing plan, failed bind, incomplete previous trial or health timeout
restores original boot before rebooting. Restoration does not need the candidate
image, but it needs the original backup and trusted candidate fingerprint. A
foreign boot is never overwritten automatically.

There is no hardware recovery guarantee: a hard kernel hang, interrupted boot
write, unreadable `/data`, or damaged backup can require physical Rockchip loader
recovery with the retained original boot. Verify that recovery route before the
first real trial. Native fixtures, static image checks and historical g18 tests
do not establish cold boot, HDR, game stability or module rollback acceptance.

Upstream mechanisms: [AOSP Android 12 init sigstop](https://android.googlesource.com/platform/system/core/+/refs/heads/android12-gsi/init/),
[Magisk root-directory overlays](https://github.com/topjohnwu/Magisk/blob/master/docs/guides.md#root-directory-overlay-system).
Radxa's module/overlay selection supplies the startup-selection approach, not a
binary drop-in or an Android bug-fix commit. This boot integration is local code;
no new Mali upstream driver fix is claimed.

## Zygisk repair and dispatch ordering

The old cold1 Zygote overrides are withdrawn. They bypassed Magisk's patched
Zygote onrestart definition and exposed pre-exec SIGSTOP to an external injector.
The current cold barrier preserves both original Zygote services entirely.

Magisk 30.7 appends custom rc before its own post-fs-data action. The custom
action first resolves disabled/failed-trial/static-payload rollback before an
external injector starts, then explicitly executes Magisk's post-fs-data stage. Its
BootState::PostFsDataDone guard makes the later stock dispatcher call a no-op;
this was also checked on this target. After all module scripts/mounts finish,
init starts the three gated graphics services, starts the independent keeper,
and exec-waits for a bounded release barrier. AOSP init continues handling
control messages during exec, so the keeper can clear their sigstop flags.
Only then can init advance to the original Zygote start actions.

This route requires Magisk API 30700. It does not edit the Magisk database,
Zygisk Next flags or LSPosed selection. Enabled Zygisk Next must report successful
injection in both current Zygotes and no module issues. Enabled LSPosed must be
listed for both ABIs with lspd running; failure restores the original boot.

Sources: Magisk tag v30.7 `native/src/init/rootdir.cpp`, `rootdir.rs`, and
`native/src/core/bootstages.rs`; AOSP Android12 `init/init.cpp` main event loop.
This is a local sequencing fix, not an upstream GPU driver update.

The preflight records enabled external injection requirements in this module's own state, tied to the boot UUID. An injector that subsequently auto-disables
cannot bypass qualification. No external module flag or Magisk DB setting is written.

</details>
