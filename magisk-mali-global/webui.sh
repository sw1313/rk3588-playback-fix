#!/system/bin/sh
CURRENT=${0%/*}
MODDIR=$CURRENT
case "$CURRENT" in /data/adb/modules/rk3588_mali_global)
 [ ! -f /data/adb/modules_update/rk3588_mali_global/module.prop ] || MODDIR=/data/adb/modules_update/rk3588_mali_global;;esac
. "$MODDIR/kernel-state.sh"
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
 health=$(getprop vendor.rkmalikernel.ready);case "$health" in active|stock|checking|failed) ;;*) health=unknown;;esac
 pending=false
 [ "$activeKnown" = true ] && [ "$next" = "$active" ] && [ "$nextKernel" = "$activeKernel" ] && [ "$staged" = false ] && [ "$removing" = false ] || pending=true
 [ "$activeVersion" = "$(version "$MODDIR")" ] || pending=true
 recovery=$(cat "$STATE/kernel-recovery" 2>/dev/null);case "$recovery" in '' ) recovery=none;; interrupted_previous_boot|boot_plan_failed|kernel_trial_failed|unsupported_kernel|missing_boot_latch|userspace_unverified|kernel_payload_mismatch|csf_cache_mismatch|invalid_runtime_directory|invalid_firmware_directory) ;;*) recovery=failed;;esac
 printf '{"schema":1,"version":"%s","activeVersion":"%s","requested":"%s","effective":"%s","active":"%s","activeKnown":%s,"moduleEnabled":%s,"staged":%s,"removing":%s,"supported":%s,"kernelRequested":"%s","kernelEffective":"%s","kernelActive":"%s","kernelHealth":"%s","recovery":"%s","pendingReboot":%s}\n' "$(version "$MODDIR")" "$activeVersion" "$mode" "$next" "$active" "$activeKnown" "$moduleEnabled" "$staged" "$removing" "$supported" "$kernel" "$nextKernel" "$activeKernel" "$health" "$recovery" "$pending"
}
case "$1" in
 status) [ "$#" = 1 ] || exit 2;status;;
 set) [ "$#" = 2 ] || exit 2;case "$2" in on|off) ;;*) exit 2;;esac
  [ ! -e "$CURRENT/remove" ] && [ ! -e "$MODDIR/remove" ] || exit 3
  sh "$MODDIR/mode-control.sh" "$2" >&2 || exit $?;status;;
 kernel) [ "$#" = 2 ] || exit 2;case "$2" in stock|next) ;;*) exit 2;;esac
  [ ! -e "$CURRENT/remove" ] && [ ! -e "$MODDIR/remove" ] || exit 3
  sh "$MODDIR/mode-control.sh" kernel "$2" >&2 || exit $?;status;;
 *) exit 2;;
esac
