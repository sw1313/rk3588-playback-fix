#!/system/bin/sh
set -eu
MODDIR=${0%/*}
. "$MODDIR/kernel-state.sh"
T=$RUN
exec > "$T/verify.log" 2>&1
[ "$(cat /proc/sys/kernel/random/boot_id)" = "$(cat "$T/trial-boot")" ]
[ "$(readlink /sys/bus/platform/devices/fb000000.gpu/driver)" = ../../../bus/platform/drivers/rk_next_mali ]
grep -q g18p0 /sys/module/rk_mali_next/version
[ "$(getprop vendor.rkmaliglobal.ready)" = active ]
M=/data/adb/modules/rk3588_mali_global
for abi in 32 64; do
 if [ "$abi" = 64 ]; then d=lib64; else d=lib; fi
 for api in gl vk; do
  timeout 12 /system/bin/env LD_LIBRARY_PATH="$M/payload/$d:/vendor/$d:/system/$d:/apex/com.android.vndk.v32/$d" "$M/bin/graphics_probe$abi" "$M/payload/$d/egl/libGLES_mali.so" "$api"
 done
done
[ "$(cat /sys/module/rk_mali_next/parameters/trial_fw_size)" = 266240 ]
[ "$(cat /sys/module/rk_mali_next/parameters/trial_fw_hi)" = 2619577260 ]
[ "$(cat /sys/module/rk_mali_next/parameters/trial_fw_lo)" = 3266300732 ]
dumpsys -t 4 SurfaceFlinger > "$T/surfaceflinger.txt"
grep '^GLES:.*g18p0' "$T/surfaceflinger.txt"
IFS= read -r original_path < "$T/original-firmware-path"
printf '%s' "$original_path" > /sys/module/firmware_class/parameters/path

i=0
while ! echo 1 > /sys/module/rk_mali_resource_guard/parameters/release_bridge; do
 i=$((i+1)); [ "$i" -lt 40 ]; sleep 0.1
done
[ "$(cat /sys/module/rk_mali_resource_guard/parameters/bridge_held_count)" = 0 ]
[ "$(cat /sys/module/rk_mali_resource_guard/parameters/hooks_installed)" = 0 ]
[ "$(cat /sys/module/rk_mali_resource_guard/parameters/handover_done)" = 1 ]
echo AUTOMATED_PASS > "$T/result"
