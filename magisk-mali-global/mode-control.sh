#!/system/bin/sh
MODDIR=${0%/*}
. "$MODDIR/kernel-state.sh"
. "$MODDIR/dvfs-state.sh"
. "$MODDIR/cold-state.sh"
. "$MODDIR/fence-state.sh"
case "$1" in
 fence) [ "$#" = 2 ] || exit 2;case "$2" in on|off) ;;*) exit 2;;esac
  if [ "$2" = on ];then
   fence_supported || exit 4
   # Apply and verify the matching boot before saving the enabled leaf.
   "$MODDIR/bin/cold_boot64" apply || exit 5
   atomic_state mode on && atomic_state kernel-mode next && atomic_state cold-mode on || exit 1
   rm -f "$STATE/boot-pending" "$STATE/kernel-recovery"
  fi
  atomic_state fence-mode "$2" || exit 1
  echo 'Saved CSF fence ordering selection; reboot to apply.';;
 status) echo "Userspace requested: $(read_mode)";echo "Kernel requested: $(read_kernel)";echo "Kernel active: $(kernel_active)";echo "GPU response requested: $(read_dvfs)";echo "Userspace: $(getprop vendor.rkmaliglobal.ready)";echo "Kernel health: $(getprop vendor.rkmalikernel.ready)";;
 on|off) [ "$#" = 1 ] || exit 2
  if [ "$1" = off ];then cold_restore || exit 5;fi
  if [ "$1" = on ] && [ "$(read_kernel)" = next ] && [ "$(read_cold)" = on ];then
   cold_prepared && "$MODDIR/bin/cold_boot64" apply || exit 5
  fi
  atomic_state mode "$1" || exit 1;[ "$1" != on ] || rm -f "$STATE/boot-pending";echo 'Saved; reboot to apply. 已保存，重启后生效。';;
 kernel) [ "$#" = 2 ] || exit 2;case "$2" in stock|next) ;;*) exit 2;;esac
  if [ "$2" = next ];then kernel_supported || exit 4;fi
  if [ "$2" = stock ];then cold_restore || exit 5;fi
  if [ "$2" = next ] && [ "$(read_cold)" = on ];then cold_prepared && "$MODDIR/bin/cold_boot64" apply || exit 5;fi
  atomic_state kernel-mode "$2" || exit 1
  if [ "$2" = next ];then
   atomic_state mode on || exit 1;rm -f "$STATE/boot-pending"
   # An explicit retry may clear an old failed attempt, never an active one.
   [ "$(getprop vendor.rkmalikernel.ready)" = checking ] || rm -f "$STATE/kernel-pending"
  fi
  rm -f "$STATE/kernel-recovery";echo 'Saved pair; reboot to apply. 组合已保存，重启后生效。';;
 dvfs) [ "$#" = 2 ] || exit 2;case "$2" in on|off) ;;*) exit 2;;esac
  if [ "$2" = on ];then
   dvfs_supported || exit 4
   # Enable ancestors first; interrupted saves never enable an unsupported leaf.
   atomic_state mode on || exit 1
   atomic_state kernel-mode next || exit 1
   rm -f "$STATE/boot-pending" "$STATE/kernel-recovery"
   [ "$(getprop vendor.rkmalikernel.ready)" = checking ] || rm -f "$STATE/kernel-pending"
   if [ "$(read_cold)" = on ];then cold_prepared && "$MODDIR/bin/cold_boot64" apply || exit 5;fi
  fi
  atomic_state dvfs-mode "$2" || exit 1
  echo 'Saved GPU response selection; reboot to apply.';;
 cold) [ "$#" = 2 ] || exit 2;case "$2" in
  on)
   kernel_supported && cold_prepared || exit 4
   atomic_state mode on && atomic_state kernel-mode next && atomic_state cold-mode on || exit 1
   "$MODDIR/bin/cold_boot64" apply || { atomic_state cold-mode off;atomic_state kernel-mode stock;exit 5; }
   rm -f "$STATE/boot-pending" "$STATE/kernel-pending"
   # An explicit retry can clear a previous failed boot, never this boot's trial.
   [ "$(cat "$STATE/cold-pending" 2>/dev/null)" = "$(cat /proc/sys/kernel/random/boot_id)" ] || rm -f "$STATE/cold-pending"
   ;;
  off)
   cold_restore || exit 5
   # Off restores the original KMD on reboot, never schedules late handover.
   atomic_state cold-mode off && atomic_state kernel-mode stock || exit 1;;
  *) exit 2;;esac
  echo 'Boot selection saved and verified. Reboot to apply; no live swap.';;
 *) echo 'Usage: mode-control.sh on|off|status|kernel stock|next|dvfs on|off|cold on|off|fence on|off';exit 2;;
esac
