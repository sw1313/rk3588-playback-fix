#!/system/bin/sh
[ "$BOOTMODE" = true ] || abort "Install from Magisk on a running Android system."
[ "$API" = 32 ] || abort "This build requires Android API 32."
[ "$ARCH" = arm64 ] || abort "This build requires ARM64 with ARM32 vendor libraries."
[ "$(getprop ro.board.platform)" = rk3588 ] || abort "This build requires RK3588."
cd "$MODPATH" || abort "Module directory unavailable."
ui_print "Checking payload and the verified Codec2/gralloc/vendor ABI..."
sha256sum -c checksums.sha256 >/dev/null 2>&1 || abort "Payload checksums failed."
sha256sum -c contracts/vendor.sha256 >/dev/null 2>&1 || abort "Vendor stack differs from the tested firmware."
check_baseline() {
    current=$(sha256sum "$1") || return 1
    current=${current%% *}
    candidate=$(sha256sum "$2") || return 1
    candidate=${candidate%% *}
    [ "$current" = "$3" ] || [ "$current" = "$candidate" ]
}
# Baseline hashes are generated from the verified device by the build tool.
. "$MODPATH/contracts/baseline.sh"
check_baseline /vendor/lib64/librga.so "$MODPATH/system/vendor/lib64/librga.so" "$RGA64_BASELINE" || abort "Unknown ARM64 librga."
check_baseline /vendor/lib/librga.so "$MODPATH/system/vendor/lib/librga.so" "$RGA32_BASELINE" || abort "Unknown ARM32 librga."
driver=$(cat /sys/kernel/debug/rkrga/driver_version 2>/dev/null)
case "$driver" in *v1.2.15*) ;; *) abort "RGA kernel driver differs from the tested v1.2.15." ;; esac
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/bin/probe64" 0 0 0755
set_perm "$MODPATH/bin/probe32" 0 0 0755
ui_print "Testing both ABIs, Codec2 loading, and actual RGA jobs..."
timeout 45 /system/bin/env LD_LIBRARY_PATH=/vendor/lib64:/system/lib64 "$MODPATH/bin/probe64" "$MODPATH/system/vendor/lib64/librga.so" "$MODPATH/contracts/arm64-symbols.txt" pixels '1.10.6_[3]' component || abort "ARM64 preflight failed."
timeout 45 /system/bin/env LD_LIBRARY_PATH=/vendor/lib:/system/lib "$MODPATH/bin/probe32" "$MODPATH/system/vendor/lib/librga.so" "$MODPATH/contracts/arm-symbols.txt" pixels '1.10.6_[3]' component || abort "ARM32 preflight failed."
ui_print "Ready. Reboot to activate RGA 1.10.6 for both ABIs."
ui_print "Action toggles this module for rollback after reboot."
