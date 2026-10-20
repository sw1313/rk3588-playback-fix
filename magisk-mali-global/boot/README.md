# Mali Global 0.4.4 / v0.8.8: root interface compatibility release

No root version or official payload hash whitelist is used. The host preserves this device's original root records and checks firmware identity, original init, root payload ELF/overlay/command prerequisites and conflicting early hooks. Runtime probes only command usage and the live root temporary directory, never dispatching a boot stage as a test. Config additions and supported compressed/raw, unified/split root layouts are retained.

These are prerequisites, not proof that arbitrary future boot callbacks behave identically. First-boot qualification and original per-user rollback guards remain. Ordinary features do not depend on this root gate. Other root frameworks still need a qualified early backend for first binding; the existing Magisk adapter is not falsely reported as KernelSU/APatch support. See `docs/root-compatibility.zh-CN.md`.

The 0.4.3 single-version restriction below describes the historical published package, not the current release. The existing Magisk 30.7 target passed full installation and reboot with the interface policy; other variants have offline coverage only. The formal package retains identical runtime files and keeper bytes from that tested policy; see `docs/validation/release-0.8.8.json`.

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
