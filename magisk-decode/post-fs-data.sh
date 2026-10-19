#!/system/bin/sh
MODROOT=${0%/*}
sh "$MODROOT/refresh-session-state.sh" begin || { touch "$MODROOT/skip_mount"; exit 1; }
sh "$MODROOT/ddc-state.sh" begin || resetprop -n vendor.rkdecode.ddc failed
sh "$MODROOT/display-policy-state.sh" begin || { touch "$MODROOT/skip_mount"; exit 1; }
if sh "$MODROOT/assemble.sh"; then
 if ! sh "$MODROOT/display-policy-state.sh" restore-hdr; then
  # A setting replay failure withdraws this system-policy candidate only.
  sh "$MODROOT/display-policy-state.sh" failed || exit 1
  sh "$MODROOT/assemble.sh" || { touch "$MODROOT/skip_mount"; exit 1; }
 fi
 value=$(sed -n 's/^codec2=//p' "$MODROOT/active-config")
 resetprop -n debug.rkcodec2.surface_upgrade "$value"
 resetprop -n debug.rkcodec2.native_backpressure "$(sed -n 's/^codec2_native_backpressure=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkcodec2.native_surface_upgrade "$(sed -n 's/^codec2_surface=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkcodec2.av1_afbc "$(sed -n 's/^av1_afbc=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkcodec2.av1_texture_afbc "$(sed -n 's/^av1_texture_afbc=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkcodec2.lifecycle_fix "$(sed -n 's/^codec2_lifecycle=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkcodec2.av1_grain "$(sed -n 's/^av1_grain=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkcodec2.av1_direct "$(sed -n 's/^av1_native=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkcodec2.p010_fast "$(sed -n 's/^p010_fast=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkcodec2.buffer_upgrade "$(sed -n 's/^codec2_buffer=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkcodec2.hdr_static_buffer "$(sed -n 's/^hdr_static=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkcodec2.p010_pairs "$(sed -n 's/^p010_pairs=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkcodec2.p010_pool "$(sed -n 's/^p010_copy_pool=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkcodec2.hevc_eos_pending "$(sed -n 's/^hevc_eos_pending=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkcodec2.hevc_native_surface "$(sed -n 's/^hevc_native_surface=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkcodec2.delay_notify "$(sed -n 's/^codec2_delay_notify=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkcodec2.gralloc_hdr_lease "$(sed -n 's/^gralloc_hdr_lease=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkhdr.client_hdr10 "$(sed -n 's/^hwc_client_hdr10=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkdecode.refresh_session "$(sed -n 's/^display_session=//p' "$MODROOT/active-config")"
 if [ "$(sed -n 's/^hwc_client_hdr10=//p' "$MODROOT/active-config")" = 1 ]; then
  # Volatile boot properties only. Disabling the item and rebooting restores
  # firmware properties naturally; no build.prop or persistent property edit.
  resetprop -n ro.surface_flinger.has_wide_color_display true
 fi
 resetprop -n debug.rkhdr.mode_cache "$(sed -n 's/^hwc_mode_cache=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkhdr.hdmi_audio_guard "$(sed -n 's/^hdmi_audio_guard=//p' "$MODROOT/active-config")"
 resetprop -n vendor.rkhdr.hdmi_event pending
 resetprop -n debug.rkhdr.metadata_reuse "$(sed -n 's/^hwc_metadata_reuse=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkhdr.static "$(sed -n 's/^hdr_static=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkcodec2.hdr10plus "$(sed -n 's/^hdr10plus=//p' "$MODROOT/active-config")"
 resetprop -n vendor.rkhdr.mode_cache pending
 resetprop -n vendor.rkhdr.sf_target10 pending
 resetprop -n vendor.rkhdr.sf_target10_error 0
 resetprop -n vendor.rkhdr.alloc10 pending
 resetprop -n vendor.rkhdr.target_plan pending
 resetprop -n vendor.rkhdr.target_plan_error 0
 resetprop -n vendor.rkhdr.adapter pending
 # The kernel probed this direct sink before Magisk fixed segmented EDID.
 # Under the existing cold graphics barrier, initialize the corrected native
 # link before QMS qualifies its first baseline and before allocator/SF bind.
 if grep -qw 'initcall_blacklist=kbase_platform_driver_init' /proc/cmdline &&
    [ "$(sed -n 's/^display_ddc=//p' "$MODROOT/active-config")" = 1 ]; then
  sh "$MODROOT/ddc-state.sh" recover || echo 'Early native HDMI initialization not qualified; no repeated modeset.' > /data/adb/rk3588-decode-ddc/early-failure
 fi
 # Only boot execution writes this receipt; installer assembly is not activation.
 (umask 077; cat /proc/sys/kernel/random/boot_id > "$MODROOT/active-boot-id.next") &&
  chmod 0600 "$MODROOT/active-boot-id.next" &&
  mv "$MODROOT/active-boot-id.next" "$MODROOT/active-boot-id" || exit 1
 if [ "$(sed -n 's/^display_qms=//p' "$MODROOT/active-config")" = 1 ]; then
  qstate=/data/adb/rk3588-decode-qms
  mkdir -p "$qstate"; chmod 0700 "$qstate"
  boot=$(cat /proc/sys/kernel/random/boot_id)
  profile=$(sed -n 's/^profile_id //p' "$MODROOT/components/qms/profile")
  (umask 077; printf '%s %s prepared\n' "$boot" "$profile" > "$qstate/plan.next")
  mv "$qstate/plan.next" "$qstate/plan"
 fi
 sed -n 's/^version=//p' "$MODROOT/module.prop" > "$MODROOT/active-version"
else
 touch "$MODROOT/disable" "$MODROOT/skip_mount"
 resetprop -n debug.rkcodec2.surface_upgrade 0
 resetprop -n debug.rkcodec2.native_surface_upgrade 0
 resetprop -n debug.rkcodec2.native_backpressure 0
 resetprop -n debug.rkcodec2.av1_afbc 0
 resetprop -n debug.rkcodec2.av1_texture_afbc 0
 resetprop -n debug.rkcodec2.lifecycle_fix 0
 resetprop -n debug.rkcodec2.av1_grain 0
 resetprop -n debug.rkcodec2.av1_direct 0
 resetprop -n debug.rkcodec2.buffer_upgrade 0
 resetprop -n debug.rkcodec2.p010_fast 0
 resetprop -n debug.rkcodec2.hdr_static_buffer 0
 resetprop -n debug.rkcodec2.delay_notify 0
 resetprop -n debug.rkcodec2.hevc_native_surface 0
 resetprop -n debug.rkcodec2.hevc_eos_pending 0
 resetprop -n debug.rkcodec2.p010_pairs 0
 resetprop -n debug.rkcodec2.p010_pool 0
 resetprop -n debug.rkcodec2.gralloc_hdr_lease 0
 resetprop -n debug.rkhdr.client_hdr10 0
 resetprop -n debug.rkdecode.refresh_session 0
 resetprop -n debug.rkhdr.mode_cache 0
 resetprop -n debug.rkhdr.hdmi_audio_guard 0
 resetprop -n debug.rkhdr.metadata_reuse 0
 resetprop -n debug.rkhdr.static 0
 resetprop -n debug.rkcodec2.hdr10plus 0
 echo "FAIL before mounting; no decode overlays enabled." > "$MODROOT/status.txt"
fi
