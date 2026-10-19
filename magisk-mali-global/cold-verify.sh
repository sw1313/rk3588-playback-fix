#!/system/bin/sh
set -eu
MODDIR=${0%/*}
. "$MODDIR/kernel-state.sh"
. "$MODDIR/cold-state.sh"
. "$MODDIR/fence-state.sh"
exec >> "$MODDIR/status.txt" 2>&1
cold_booting && cold_receipt
[ "$(kernel_active)" = next ]
[ "$(fence_active)" = "$(read_fence)" ]
grep -q g18p0 /sys/module/rk_mali_next/version
[ "$(getprop vendor.rkmaliglobal.ready)" = active ]
cd "$MODDIR"
sha256sum -c kernel.sha256
# Observe the same framework/audio/CSF health policy, without a handover guard.
"$MODDIR/bin/kernel_health" watch "$RUN/health" "$RUN/health.restored" 90
count=0
while read -r service pid;do
 case "$service" in vendor.gralloc-4-0|vendor.hwcomposer-2-1|surfaceflinger) ;;*) exit 1;;esac
 [ "$(getprop "init.svc_debug_pid.$service")" = "$pid" ]
 count=$((count+1))
done < "$STATE/cold-pids"
[ "$count" = 3 ]
sh "$MODDIR/zygisk-health.sh"
echo COLD_AUTOMATED_PASS > "$RUN/result"
"$MODDIR/bin/cold_boot64" qualify
atomic_state kernel-active-boot "$(cat /proc/sys/kernel/random/boot_id)"
rm -f "$STATE/kernel-pending" "$STATE/kernel-recovery"
resetprop -n vendor.rkmalikernel.ready active
echo 'PASS cold first binding; no runtime driver handover requested. App/HDR validation is separate.'
