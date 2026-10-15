#!/system/bin/sh
[ "$BOOTMODE" = true ] || abort "Install from Magisk on the running verified firmware."
[ "$API" = 32 ] && [ "$ARCH" = arm64 ] || abort "Requires the verified API32 ARM64/ARM32 stack."
[ "$(getprop ro.board.platform)" = rk3588 ] || abort "Requires RK3588."
[ "$(getenforce)" = Permissive ] || abort "This firmware's enforcing policy is untested; policy is never changed."
STACKROOT=$MODPATH
cd "$STACKROOT" || abort "Module directory unavailable."
sha256sum -c checksums.sha256 >/dev/null 2>&1 || abort "Unified payload checksum mismatch."
# Keep the existing firmware and payload checks of each tested component.
# All preflights finish before any previous module is disabled.
for component in mpp rga codec2 libyuv software hdr; do
 ui_print "Checking decode component: $component"
 (MODPATH="$STACKROOT/components/$component"; RK_DECODE_STAGE="$STACKROOT/components"; . "$MODPATH/customize.sh") || abort "Component preflight failed: $component"
done
cd "$STACKROOT" || abort "Module directory unavailable."
sha256sum -c files.sha256 >/dev/null 2>&1 || abort "Generated platform patches or component payload differ."
if [ -f /data/adb/modules/rk3588_decode_stack/config ]; then
 cp /data/adb/modules/rk3588_decode_stack/config "$STACKROOT/config" || abort "Cannot preserve component selection."
 grep -q '^software=' "$STACKROOT/config" || printf 'software=1\n' >> "$STACKROOT/config"
 grep -q '^hdr_static=' "$STACKROOT/config" || printf 'hdr_static=1\n' >> "$STACKROOT/config"
 grep -q '^hdr10plus=' "$STACKROOT/config" || printf 'hdr10plus=1\n' >> "$STACKROOT/config"
else
 printf 'mpp=1\nrga=1\ncodec2=1\nlibyuv=1\nsoftware=1\nhdr_static=1\nhdr10plus=1\n' > "$STACKROOT/config"
fi
sh "$STACKROOT/assemble.sh" || abort "Cannot prepare the selected overlays."
# Prevent overlapping mounts, including a previous update waiting for reboot.
# Original modules are retained disabled until migration tests pass.
for root in /data/adb/modules /data/adb/modules_update; do
 for id in rk3588_mpp_upgrade rk3588_rga_upgrade rk3588_codec2_upgrade rk3588_libyuv_upgrade; do
  [ ! -d "$root/$id" ] || touch "$root/$id/disable" || abort "Cannot disable overlapping module: $id"
 done
done
ui_print "Unified stack ready. Previous decode modules disabled; reboot to activate."
ui_print "Action toggles the whole stack. mode-control.sh provides component switches."
ui_print "Yamby, LSPosed, kernel and display configuration are retained."
