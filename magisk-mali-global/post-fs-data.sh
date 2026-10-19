#!/system/bin/sh
MODDIR=${0%/*}
. "$MODDIR/kernel-state.sh"
. "$MODDIR/dvfs-state.sh"
. "$MODDIR/cold-state.sh"
if cold_booting;then
 cold_plan || { atomic_state kernel-boot-plan stock;atomic_state kernel-recovery cold_plan_failed; }
elif [ "$(read_cold)" = on ];then
 # A requested cold backend must never fall through to late handover.
 atomic_state kernel-boot-plan stock
else
 kernel_boot_plan || { atomic_state kernel-boot-plan stock;atomic_state kernel-recovery boot_plan_failed; }
fi
plan=off
[ "$(read_mode)" != on ] || [ "$(read_kernel)" != next ] || [ "$(read_dvfs)" != on ] || plan=on
printf '%s\n' "$plan" > "$MODDIR/active-dvfs-plan"
resetprop -n vendor.rkmalidvfs.ready pending
if sh "$MODDIR/assemble.sh"; then
 if [ "$(cat "$STATE/kernel-boot-plan")" = cold ];then
  atomic_state cold-plan "$(cat /proc/sys/kernel/random/boot_id)"
 fi
 resetprop -n vendor.rkmaliglobal.ready pending
else
 touch "$MODDIR/skip_mount" "$MODDIR/disable"
 echo "FAIL before mounting; global GPU disabled." > "$MODDIR/status.txt"
 resetprop -n vendor.rkmaliglobal.ready failed
fi
