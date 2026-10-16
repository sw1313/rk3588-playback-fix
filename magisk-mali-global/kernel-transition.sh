#!/system/bin/sh
set -eu
MODDIR=${0%/*}
. "$MODDIR/kernel-state.sh"
T=$RUN
exec > "$T/hot-transition.log" 2>&1
echo BEGIN_ACTUAL_QUIESCED_KMD_CSF_TRIAL
cat /proc/uptime
cat /proc/sys/kernel/random/boot_id > "$T/trial-boot"
[ "$(cat "$STATE/kernel-pending")" = "$(cat /proc/sys/kernel/random/boot_id)" ]
for s in surfaceflinger zygote zygote_secondary vendor.hwcomposer-2-1; do
 [ "$(getprop init.svc.$s)" = stopped ]
done
for d in /sys/kernel/debug/mali0/ctx/*; do
 if [ -d "$d" ] && [ "${d##*/}" != defaults ]; then exit 40; fi
done
"$MODDIR/bin/kernel_scan"
insmod "$MODDIR/kernel/rk_mali_resource_guard.ko" allow_initialized=1 journal_path="$T/resource-handover-boot.log"
echo 1 > /sys/module/rk_mali_resource_guard/parameters/arm
[ "$(cat /sys/module/rk_mali_resource_guard/parameters/armed)" = 1 ]
sleep 1
"$MODDIR/bin/kernel_scan"
for d in /sys/kernel/debug/mali0/ctx/*; do
 if [ -d "$d" ] && [ "${d##*/}" != defaults ]; then exit 41; fi
done
echo native-power-preparation > "$T/phase"
sync
echo 1 > /sys/module/rk_mali_resource_guard/parameters/prepare_power
[ "$(cat /sys/module/rk_mali_resource_guard/parameters/power_prepared)" = 1 ]
[ "$(cat /sys/module/rk_mali_resource_guard/parameters/bridge_held_count)" = 2 ]
[ "$(cat /sys/module/rk_mali_resource_guard/parameters/hooks_installed)" = 1 ]
echo NATIVE_POWER_AND_RESOURCE_HANDOVER_PREPARED
echo original-unbind-begin > "$T/phase"
sync
echo fb000000.gpu > /sys/bus/platform/drivers/mali/unbind
[ ! -L /sys/bus/platform/devices/fb000000.gpu/driver ]
[ "$(cat /sys/module/rk_mali_resource_guard/parameters/handover_done)" = 1 ]
[ "$(cat /sys/module/rk_mali_resource_guard/parameters/hooks_installed)" = 0 ]
echo original-unbind-complete > "$T/phase"
sync
cat /sys/module/firmware_class/parameters/path > "$T/original-firmware-path"
FW=/data/local/tmp/rk3588-mali-firmware
printf '%s' "$FW" > /sys/module/firmware_class/parameters/path
printf '%s\n' "$FW" > "$T/expected-firmware-path"
cat /sys/module/firmware_class/parameters/path > "$T/actual-firmware-path"
cmp -s "$T/expected-firmware-path" "$T/actual-firmware-path"

echo candidate-probe-begin > "$T/phase"
sync
insmod "$MODDIR/kernel/rk_mali_next.ko" oneboot_trial=1
[ "$(readlink /sys/bus/platform/devices/fb000000.gpu/driver)" = ../../../bus/platform/drivers/rk_next_mali ]
[ -r /sys/class/misc/mali0/dev ]
echo candidate-bound > "$T/phase"
sync
echo CANDIDATE_ACTUALLY_BOUND
cat /proc/uptime
