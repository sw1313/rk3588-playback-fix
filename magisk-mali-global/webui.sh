#!/system/bin/sh
CURRENT=${0%/*}
MODDIR=$CURRENT
case "$CURRENT" in /data/adb/modules/rk3588_mali_global)
 [ ! -f /data/adb/modules_update/rk3588_mali_global/module.prop ] || MODDIR=/data/adb/modules_update/rk3588_mali_global;;esac
. "$MODDIR/kernel-state.sh"
. "$MODDIR/dvfs-state.sh"
. "$MODDIR/cold-state.sh"
. "$MODDIR/fence-state.sh"
version(){ v=$(sed -n 's/^version=//p' "$1/module.prop" 2>/dev/null);case "$v" in ''|*[!0-9A-Za-z._+-]*) echo unknown;;*) echo "$v";;esac; }
status(){
 mode=$(read_mode);kernel=$(read_kernel);supported=false;kernel_supported && supported=true
 moduleEnabled=true;[ ! -e "$CURRENT/disable" ] && [ ! -e "$MODDIR/disable" ] || moduleEnabled=false
 removing=false;[ ! -e "$CURRENT/remove" ] && [ ! -e "$MODDIR/remove" ] || removing=true
 staged=false;[ "$MODDIR" = "$CURRENT" ] || staged=true
 activeKnown=false;active=unknown;activeVersion=unknown
 boot=$(cat /proc/sys/kernel/random/boot_id)
 if [ "$(cat "$CURRENT/active-boot-id" 2>/dev/null)" = "$boot" ];then
  active=$(cat "$CURRENT/active-mode" 2>/dev/null);case "$active" in on|off) activeKnown=true;;*) active=unknown;;esac
  activeVersion=$(cat "$CURRENT/active-version" 2>/dev/null);case "$activeVersion" in ''|*[!0-9A-Za-z._+-]*) activeVersion=unknown;;esac
 fi
 next=$mode;[ "$moduleEnabled" = true ] && [ "$removing" = false ] || next=off
 nextKernel=stock
 [ "$next" != on ] || [ "$kernel" != next ] || [ "$supported" != true ] || nextKernel=next
 activeKernel=$(kernel_active)
 dvfs=$(read_dvfs);dvfsSupported=false;dvfs_supported && dvfsSupported=true
 nextDvfs=off
 [ "$next" != on ] || [ "$nextKernel" != next ] || [ "$dvfs" != on ] || [ "$dvfsSupported" != true ] || nextDvfs=on
 activeDvfs=$(dvfs_active)
 dvfsHealth=$(getprop vendor.rkmalidvfs.ready);case "$dvfsHealth" in active|off|pending|failed|skipped) ;;*) dvfsHealth=unknown;;esac
 interval=$(cat "$DVFS/polling_interval" 2>/dev/null);case "$interval" in ''|*[!0-9]*) interval=null;;esac
 health=$(getprop vendor.rkmalikernel.ready);case "$health" in active|stock|checking|failed) ;;*) health=unknown;;esac
 pending=false
 [ "$activeKnown" = true ] && [ "$next" = "$active" ] && [ "$nextKernel" = "$activeKernel" ] && [ "$staged" = false ] && [ "$removing" = false ] || pending=true
 [ "$activeVersion" = "$(version "$MODDIR")" ] || pending=true
 [ "$activeDvfs" = "$nextDvfs" ] || pending=true
 cold=$(read_cold);coldPrepared=false;cold_prepared && coldPrepared=true
 coldActive=off;cold_booting && coldActive=on
 coldEffective=off
 [ "$next" != on ] || [ "$kernel" != next ] || [ "$cold" != on ] || [ "$coldPrepared" != true ] || coldEffective=on
 [ "$coldActive" = "$coldEffective" ] || pending=true
 fence=$(read_fence);fenceSupported=false;fence_supported && fenceSupported=true
 fenceActive=$(fence_active);fenceEffective=off
 [ "$coldEffective" != on ] || [ "$fence" != on ] || [ "$fenceSupported" != true ] || fenceEffective=on
 [ "$fenceActive" = "$fenceEffective" ] || pending=true
 printf '{"fenceRequested":"%s","fenceSupported":%s,"fenceActive":"%s","fenceEffective":"%s",' "$fence" "$fenceSupported" "$fenceActive" "$fenceEffective"
 printf '"coldRequested":"%s","coldPrepared":%s,"coldActive":"%s","coldEffective":"%s",' "$cold" "$coldPrepared" "$coldActive" "$coldEffective"
 printf '"rootStartup":"%s",' "$(root_startup_reason)"
 recovery=$(cat "$STATE/kernel-recovery" 2>/dev/null);case "$recovery" in '' ) recovery=none;; interrupted_previous_boot|boot_plan_failed|kernel_trial_failed|unsupported_kernel|missing_boot_latch|userspace_unverified|kernel_payload_mismatch|csf_cache_mismatch|invalid_runtime_directory|invalid_firmware_directory) ;;*) recovery=failed;;esac
 printf '"schema":1,"version":"%s","activeVersion":"%s","requested":"%s","effective":"%s","active":"%s","activeKnown":%s,"moduleEnabled":%s,"staged":%s,"removing":%s,"supported":%s,"kernelRequested":"%s","kernelEffective":"%s","kernelActive":"%s","kernelHealth":"%s","recovery":"%s","dvfsRequested":"%s","dvfsEffective":"%s","dvfsActive":"%s","dvfsSupported":%s,"dvfsHealth":"%s","pollingIntervalMs":%s,"pendingReboot":%s}\n' "$(version "$MODDIR")" "$activeVersion" "$mode" "$next" "$active" "$activeKnown" "$moduleEnabled" "$staged" "$removing" "$supported" "$kernel" "$nextKernel" "$activeKernel" "$health" "$recovery" "$dvfs" "$nextDvfs" "$activeDvfs" "$dvfsSupported" "$dvfsHealth" "$interval" "$pending"
}
case "$1" in
 fence) [ "$#" = 2 ] || exit 2;case "$2" in on|off) ;;*) exit 2;;esac
  [ ! -e "$CURRENT/remove" ] && [ ! -e "$MODDIR/remove" ] || exit 3
  sh "$MODDIR/mode-control.sh" fence "$2" >&2 || exit $?;status;;
 status) [ "$#" = 1 ] || exit 2;status;;
 set) [ "$#" = 2 ] || exit 2;case "$2" in on|off) ;;*) exit 2;;esac
  [ ! -e "$CURRENT/remove" ] && [ ! -e "$MODDIR/remove" ] || exit 3
  sh "$MODDIR/mode-control.sh" "$2" >&2 || exit $?;status;;
 kernel) [ "$#" = 2 ] || exit 2;case "$2" in stock|next) ;;*) exit 2;;esac
  [ ! -e "$CURRENT/remove" ] && [ ! -e "$MODDIR/remove" ] || exit 3
  sh "$MODDIR/mode-control.sh" kernel "$2" >&2 || exit $?;status;;
 dvfs) [ "$#" = 2 ] || exit 2;case "$2" in on|off) ;;*) exit 2;;esac
  [ ! -e "$CURRENT/remove" ] && [ ! -e "$MODDIR/remove" ] || exit 3
  sh "$MODDIR/mode-control.sh" dvfs "$2" >&2 || exit $?;status;;
 cold) [ "$#" = 2 ] || exit 2;case "$2" in on|off) ;;*) exit 2;;esac
  [ ! -e "$CURRENT/remove" ] && [ ! -e "$MODDIR/remove" ] || exit 3
  sh "$MODDIR/mode-control.sh" cold "$2" >&2 || exit $?;status;;
 *) exit 2;;
esac
