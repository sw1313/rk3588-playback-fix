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
for sdk_component in codec2_sdk codec2_sdk_next; do
if [ -d "$STACKROOT/components/$sdk_component" ]; then
 ui_print "Checking SDK14 Codec2 on the existing API32 service and MPP profile"
 c="$STACKROOT/components/$sdk_component"
 (cd "$c" && sha256sum -c checksums.sha256 >/dev/null 2>&1) || abort "SDK Codec2 payload differs."
 sha256sum -c "$c/contracts/platform.sha256" >/dev/null 2>&1 || abort "SDK Codec2 additional API32 dependencies differ."
 timeout 25 /system/bin/env LD_LIBRARY_PATH="$STACKROOT/components/mpp/system/vendor/lib:$STACKROOT/components/rga/system/vendor/lib:/vendor/lib:/system/lib:/apex/com.android.vndk.v32/lib" RK_CODEC2_P010_LIBRARY="$c/system/vendor/lib/hw/libcodec2_rx_component.so" "$STACKROOT/components/codec2/system/vendor/bin/hw/android.hardware.media.c2@1.1-service" --check-only > "$c/preflight.txt" 2>&1 || abort "SDK Codec2 interface/HIDL preflight failed."
 grep -q HIDL_CONSTRUCTION_PASS "$c/preflight.txt" || abort "SDK HIDL contract check failed."
fi
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
if [ -d "$STACKROOT/components/codec2_sdk" ]; then default_sdk=1; else default_sdk=0; fi
grep -q '^codec2_sdk=' "$STACKROOT/config" || printf 'codec2_sdk=%s\n' "$default_sdk" >> "$STACKROOT/config"
grep -q '^codec2_buffer=' "$STACKROOT/config" || printf 'codec2_buffer=1\n' >> "$STACKROOT/config"
grep -q '^p010_fast=' "$STACKROOT/config" || printf 'p010_fast=1\n' >> "$STACKROOT/config"
if [ -d "$STACKROOT/components/codec2_sdk_next" ]; then default_next=1; else default_next=0; fi
grep -q '^codec2_sdk_next=' "$STACKROOT/config" || printf 'codec2_sdk_next=%s\n' "$default_next" >> "$STACKROOT/config"
grep -q '^av1_native=' "$STACKROOT/config" || printf 'av1_native=1\n' >> "$STACKROOT/config"
grep -q '^av1_grain=' "$STACKROOT/config" || printf 'av1_grain=1\n' >> "$STACKROOT/config"
grep -q '^av1_afbc=' "$STACKROOT/config" || printf 'av1_afbc=1\n' >> "$STACKROOT/config"
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
