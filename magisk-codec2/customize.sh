#!/system/bin/sh
[ "$BOOTMODE" = true ] || abort "Install from Magisk on the running tested firmware."
[ "$API" = 32 ] && [ "$ARCH" = arm64 ] || abort "Requires the verified API32 ARM64/ARM32 stack."
[ "$(getprop ro.board.platform)" = rk3588 ] || abort "Requires RK3588."
[ "$(getenforce)" = Permissive ] || abort "This firmware's enforcing policy has not been verified; SELinux is never changed."
cd "$MODPATH" || abort "Module directory unavailable."
sha256sum -c checksums.sha256 >/dev/null 2>&1 || abort "Payload verification failed."
sha256sum -c contracts/platform.sha256 >/dev/null 2>&1 || abort "Untested Codec2/HIDL/gralloc ABI."
codec2_dependencies=/vendor/lib:/system/lib:/apex/com.android.vndk.v32/lib
if [ -n "$RK_DECODE_STAGE" ]; then
 sh "$MODPATH/check-runtime.sh" --staged "$RK_DECODE_STAGE" >/dev/null 2>&1 || abort "Unified MPP/RGA payload differs from the verified profile."
 codec2_dependencies="$RK_DECODE_STAGE/mpp/system/vendor/lib:$RK_DECODE_STAGE/rga/system/vendor/lib:$codec2_dependencies"
else
 sh "$MODPATH/check-runtime.sh" >/dev/null 2>&1 || abort "Requires the tested active MPP 1.1.0 and RGA 1.10.6 libraries."
fi
set_perm "$MODPATH/bin/patcher" 0 0 0755
sha() { value=$(sha256sum "$1") || return 1; echo "${value%% *}"; }
service_hash=$(sha /vendor/bin/hw/android.hardware.media.c2@1.1-service) || abort "Cannot read service."
payload_hash=$(sha "$MODPATH/system/vendor/bin/hw/android.hardware.media.c2@1.1-service")
[ "$service_hash" = 4c3586e5b54b67f98eb15772ef215380d51f34b52efd8dca80a2f17bad124746 ] || [ "$service_hash" = "$payload_hash" ] || [ "$service_hash" = 796d3ac07a1341e8bca0551132b0c844ee37572b9bbeb20571b12bcc5e58b9a3 ] || [ "$service_hash" = f07d9c70d10f2157aaef3db4ca6cf2858bfdeb8f286190467c0a639c4bd07d84 ] || abort "Unknown service baseline."
# The final alternative is our earlier locally tested service, permitting an
# upgrade without weakening the pinned original HIDL and platform ABI checks.
ui_print "Generating narrow P010 fixes from this device's verified original files..."
while read -r target destination original patched recipe; do
 [ -n "$target" ] || continue
 current=$(sha "$target") || abort "Cannot read $target"
 mkdir -p "$MODPATH/${destination%/*}" || abort "Cannot prepare destination."
 if [ "$current" = "$original" ]; then
  "$MODPATH/bin/patcher" binary "$target" "$MODPATH/$destination" "$MODPATH/$recipe" || abort "Exact byte check failed: $target"
 elif [ "$current" = "$patched" ]; then
  cp "$target" "$MODPATH/$destination" || abort "Cannot copy verified active fix."
 else
  abort "Unrecognized library: $target"
 fi
 [ "$(sha "$MODPATH/$destination")" = "$patched" ] || abort "Generated hash differs: $target"
done < "$MODPATH/contracts/patches.txt"
mkdir -p "$MODPATH/system/vendor/etc"
xml=/vendor/etc/media_codecs_c2_base.xml
current=$(sha "$xml")
if [ "$current" = e23172f51e609fc14335f7d6a8d999d29275c967ed06debaad8a5f0aa5f13f67 ]; then
 "$MODPATH/bin/patcher" xml "$xml" "$MODPATH/system/vendor/etc/media_codecs_c2_base.xml" "$MODPATH/contracts/extra-codecs.xml" || abort "Codec XML generation failed."
elif [ "$current" = 28aaefc10e5b5c270e2b1c464b8cf7fa5cf8b95417e97392543fe1f56ed0175e ]; then
 cp "$xml" "$MODPATH/system/vendor/etc/media_codecs_c2_base.xml" || abort "Cannot copy active codec XML."
else
 abort "Unknown codec XML baseline."
fi
[ "$(sha "$MODPATH/system/vendor/etc/media_codecs_c2_base.xml")" = 28aaefc10e5b5c270e2b1c464b8cf7fa5cf8b95417e97392543fe1f56ed0175e ] || abort "Codec XML hash mismatch."
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/bin/patcher" 0 0 0755
set_perm "$MODPATH/system/vendor/bin/hw/android.hardware.media.c2@1.1-service" 0 0 0755 u:object_r:mediacodec_exec:s0
ui_print "Checking installed HIDL ABI and all original/candidate interfaces..."
timeout 25 /system/bin/env LD_LIBRARY_PATH="$codec2_dependencies" RK_CODEC2_P010_LIBRARY="$MODPATH/system/vendor/lib/hw/libcodec2_rx_component.so" "$MODPATH/system/vendor/bin/hw/android.hardware.media.c2@1.1-service" --check-only > "$MODPATH/preflight.txt" 2>&1 || abort "HIDL/Codec2 preflight failed."
cat "$MODPATH/preflight.txt"
while read -r name; do
 grep -F "COMPONENT $name rank=" "$MODPATH/preflight.txt" >/dev/null || abort "Missing stock component: $name"
done < "$MODPATH/contracts/stock-components.txt"
echo quality > "$MODPATH/mode"
ui_print "Highest verified Surface quality enabled; legacy copy clients retain stock paths."
ui_print "Reboot to activate. Action toggles full rollback after reboot."
