#!/system/bin/sh
MODDIR=${0%/*}
. "$MODDIR/kernel-state.sh"
case "$1" in
 status) echo "Userspace requested: $(read_mode)";echo "Kernel requested: $(read_kernel)";echo "Kernel active: $(kernel_active)";echo "Userspace: $(getprop vendor.rkmaliglobal.ready)";echo "Kernel health: $(getprop vendor.rkmalikernel.ready)";;
 on|off) [ "$#" = 1 ] || exit 2;atomic_state mode "$1" || exit 1;[ "$1" != on ] || rm -f "$STATE/boot-pending";echo 'Saved; reboot to apply. 已保存，重启后生效。';;
 kernel) [ "$#" = 2 ] || exit 2;case "$2" in stock|next) ;;*) exit 2;;esac
  if [ "$2" = next ];then kernel_supported || exit 4;fi
  atomic_state kernel-mode "$2" || exit 1
  if [ "$2" = next ];then
   atomic_state mode on || exit 1;rm -f "$STATE/boot-pending"
   # An explicit retry may clear an old failed attempt, never an active one.
   [ "$(getprop vendor.rkmalikernel.ready)" = checking ] || rm -f "$STATE/kernel-pending"
  fi
  rm -f "$STATE/kernel-recovery";echo 'Saved pair; reboot to apply. 组合已保存，重启后生效。';;
 *) echo 'Usage: mode-control.sh on|off|status|kernel stock|next';exit 2;;
esac
