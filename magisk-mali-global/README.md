# Mali Global 0.3.0 / 全局 Mali 图形升级

Global ARM32/ARM64 g18p0 userspace with an **optional matched g18p0 kernel + CSF pair**.
The kernel option defaults to **original**. This is an experimental, exact-firmware upgrade
for the tested H96 Max V58 / RK3588, API32, original 5.10.66 build. It is not a generic
RK3588 driver installer. Kernel code fingerprints, import ABI, original CSF and platform
contracts must match; unknown builds are refused before the ownership transition.

全局 32／64 位 g18p0 用户态保持原有功能，新增配套内核／CSF 组合选择，默认使用原版。
不刷 boot、不写真实系统分区，不更改 Yamby、LSPosed、Decode Stack 或播放画质设置。

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

## Startup and rollback / 启动与回退

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
