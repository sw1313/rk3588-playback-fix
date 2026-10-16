#!/system/bin/sh
MODDIR=${0%/*}
STATE=/data/adb/rk3588-mali-global
recover() {
 echo off > "$STATE/mode"
 touch "$MODDIR/disable"
 resetprop -n vendor.rkmaliglobal.ready failed
 echo "FAIL: $1; global GPU disabled, rebooting once to originals." >> "$MODDIR/status.txt"
 sync;reboot;exit 1
}
: > "$MODDIR/status.txt"
n=0
while [ "$(getprop sys.boot_completed)" != 1 ] && [ "$n" -lt 75 ]; do sleep 2;n=$((n+1));done
[ "$(getprop sys.boot_completed)" = 1 ] || recover "boot did not complete"
if [ "$(cat "$MODDIR/active-mode")" = off ]; then
 sha256sum -c "$MODDIR/contracts/original-mali.sha256" >> "$MODDIR/status.txt" 2>&1 || recover "original GPU recovery differs"
 resetprop -n vendor.rkmaliglobal.ready stock
 rm -f "$STATE/boot-pending"
 cat /proc/sys/kernel/random/boot_id > "$MODDIR/active-boot-id"
 sed -n 's/^version=//p' "$MODDIR/module.prop" > "$MODDIR/active-version"
 resetprop -n vendor.rkmalikernel.ready stock
 echo "PASS original global GPU restored" >> "$MODDIR/status.txt"
 exit 0
fi
sha256sum -c "$MODDIR/active.sha256" >> "$MODDIR/status.txt" 2>&1 || recover "active GPU hashes differ"
sha256sum -c "$MODDIR/contracts/unchanged-platform.sha256" >> "$MODDIR/status.txt" 2>&1 || recover "original platform differs"
# Let the existing decode stack finish its own guarded HWC startup first.
decode=/data/adb/modules/rk3588_decode_stack
if [ -f "$decode/service.sh" ] && [ ! -f "$decode/disable" ]; then
 n=0
 while ! grep -q 'PASS unified decode stack' "$decode/status.txt" && [ "$n" -lt 45 ]; do sleep 1;n=$((n+1));done
 grep -q 'PASS unified decode stack' "$decode/status.txt" || recover "decode startup not verified"
fi
timeout 10 dumpsys SurfaceFlinger > "$MODDIR/compositor-status.txt" || recover "compositor unavailable"
if ! grep '^GLES:.*g18p0' "$MODDIR/compositor-status.txt" >/dev/null; then
 # Vendor display services may precede Magic Mount. Restart their consumers
 # once after both the decode and GPU overlays have been assembled.
 stop surfaceflinger;stop vendor.hwcomposer-2-1
 n=0
 while [ "$(getprop init.svc.vendor.hwcomposer-2-1)" != stopped ] && [ "$n" -lt 50 ]; do sleep 0.1;n=$((n+1));done
 start vendor.hwcomposer-2-1;start surfaceflinger
 setprop ctl.restart zygote;setprop ctl.restart zygote_secondary
 n=0
 while [ "$n" -lt 50 ]; do
  sleep 1;n=$((n+1))
  timeout 5 dumpsys SurfaceFlinger > "$MODDIR/compositor-status.txt" 2>/dev/null
  grep '^GLES:.*g18p0' "$MODDIR/compositor-status.txt" >/dev/null && break
 done
fi
grep '^GLES:.*g18p0' "$MODDIR/compositor-status.txt" >/dev/null || recover "compositor did not load global g18"
for abi in 32 64; do
 [ "$abi" = 64 ] && dir=lib64 || dir=lib
 for api in gl vk; do
  # The service's default linker namespace cannot directly dlopen vendor EGL
  # plugin paths. Test the identical verified payload through its data path.
  timeout 20 /system/bin/env LD_LIBRARY_PATH="$MODDIR/payload/$dir:/vendor/$dir:/system/$dir:/apex/com.android.vndk.v32/$dir" "$MODDIR/bin/graphics_probe$abi" "$MODDIR/payload/$dir/egl/libGLES_mali.so" "$api" >> "$MODDIR/status.txt" 2>&1 || recover "native graphics probe failed"
 done
done
rm -f "$STATE/boot-pending"
resetprop -n vendor.rkmaliglobal.ready active
echo "PASS global ARM32/ARM64 native Mali and compositor; app/video tests are separate." >> "$MODDIR/status.txt"

cat /proc/sys/kernel/random/boot_id > "$MODDIR/active-boot-id"
sed -n 's/^version=//p' "$MODDIR/module.prop" > "$MODDIR/active-version"
sh "$MODDIR/kernel-boot.sh"
