# Exact-root cold binding (0.4.1-test2)

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
