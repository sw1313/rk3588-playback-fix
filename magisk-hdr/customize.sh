#!/system/bin/sh
[ "$BOOTMODE" = true ] && [ "$API" = 32 ] && [ "$ARCH" = arm64 ] || abort "Requires the verified running API32 ARM64 firmware."
[ "$(getprop ro.board.platform)" = rk3588 ] || abort "Requires RK3588."
cd "$MODPATH" || abort "HDR component unavailable."
sha256sum -c checksums.sha256 >/dev/null 2>&1 || abort "HDR component payload differs."
sha256sum -c contracts/platform.sha256 >/dev/null 2>&1 || abort "Original HWC/gralloc/DRM dependency ABI differs."
original_hash=ec3e42d172af843a74bad42674d6e5932f0070873c7bb73cf681a285ada44478
original=/vendor/lib64/hw/hwcomposer.rk30board.so
previous=/data/adb/modules/rk3588_decode_stack/components/hdr/system/vendor/lib64/hw/hwcomposer.rk30board.rkhdr.so
sha() { value=$(sha256sum "$1") || return 1; echo "${value%% *}"; }
if [ "$(sha "$original")" != "$original_hash" ]; then
 [ -f "$previous" ] && [ "$(sha "$previous")" = "$original_hash" ] || abort "Unrecognized original HWC."
 original=$previous
fi
destination=system/vendor/lib64/hw/hwcomposer.rk30board.rkhdr.so
cp "$original" "$destination" || abort "Cannot retain original HWC companion."
[ "$(sha "$destination")" = "$original_hash" ] || abort "Original HWC companion hash differs."
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/bin/check64" 0 0 0755
set_perm "$MODPATH/bin/packet_probe64" 0 0 0755
timeout 10 /system/bin/env LD_LIBRARY_PATH=/vendor/lib64:/system/lib64:/apex/com.android.vndk.v32/lib64 "$MODPATH/bin/check64" "$MODPATH/system/vendor/lib64/hw/hwcomposer.rk30board.so" "$MODPATH/$destination" || abort "Original HWC adapter preflight failed."
ui_print "HDR10 metadata adapter verified. Original compositor, pixel buffers and HDR enable policy retained; guarded PQ/HLG and native client output repairs included."
