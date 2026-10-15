#!/system/bin/sh
[ "$BOOTMODE" = true ] || abort "Install from Magisk on the running tested firmware."
[ "$API" = 32 ] && [ "$ARCH" = arm64 ] || abort "Requires API32 ARM64."
[ "$(getprop ro.board.platform)" = rk3588 ] || abort "Requires the verified RK3588 device."
cd "$MODPATH" || abort "Module directory unavailable."
sha256sum -c checksums.sha256 >/dev/null 2>&1 || abort "Payload checksum failed."
sha256sum -c contracts/platform.sha256 >/dev/null 2>&1 || abort "JPEG dependency ABI differs."
current=$(sha256sum /system/lib64/libyuv.so) || abort "Original libyuv unavailable."
current=${current%% *}
candidate=$(sha256sum "$MODPATH/system/lib64/libyuv.so"); candidate=${candidate%% *}
[ "$current" = 22358071bb74ad19fbe4b2f5fcba4d4130f680deddf7df02e635674b9824233e ] || [ "$current" = "$candidate" ] || grep -Fx "$current  /system/lib64/libyuv.so" contracts/prior-active.sha256 >/dev/null || abort "Unknown libyuv baseline."
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/bin/quality_probe64" 0 0 0755
timeout 15 "$MODPATH/bin/quality_probe64" "$MODPATH/system/lib64/libyuv.so" > "$MODPATH/status.txt" 2>&1 || abort "Full precision libyuv preflight failed."
ui_print "System libyuv validated. Reboot to activate; Action toggles rollback after reboot."
