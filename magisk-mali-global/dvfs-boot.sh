#!/system/bin/sh
# A one-shot boot policy; no daemon, clock limits, voltage or thermal changes.
MODDIR=${0%/*}
. "$MODDIR/kernel-state.sh"
. "$MODDIR/dvfs-state.sh"
mode=off
resetprop -n vendor.rkmalidvfs.ready off
if [ "$(cat "$MODDIR/active-dvfs-plan" 2>/dev/null)" = on ] &&
   [ "$(cat "$MODDIR/active-mode" 2>/dev/null)" = on ];then
 if [ "$(getprop vendor.rkmaliglobal.ready)" = active ] &&
    [ "$(getprop vendor.rkmalikernel.ready)" = active ] &&
    [ "$(kernel_active)" = next ] && dvfs_supported &&
    [ "$(cat "$DVFS/polling_interval")" = 100 ];then
  # Keep the driver's own governor, OPP table, caps and thermal protection.
  if printf '20\n' > "$DVFS/polling_interval" &&
     [ "$(cat "$DVFS/polling_interval")" = 20 ];then
   mode=on
   resetprop -n vendor.rkmalidvfs.ready active
   echo 'PASS GPU response: simple_ondemand polling 100 -> 20 ms; clock limits retained.' >> "$MODDIR/status.txt"
  else
   printf '100\n' > "$DVFS/polling_interval"
   resetprop -n vendor.rkmalidvfs.ready failed
   echo 'WARN GPU response write failed; original polling restored.' >> "$MODDIR/status.txt"
  fi
 else
  resetprop -n vendor.rkmalidvfs.ready skipped
  echo 'WARN GPU response skipped: paired kernel or original polling contract unavailable.' >> "$MODDIR/status.txt"
 fi
fi
printf '%s\n' "$mode" > "$MODDIR/active-dvfs-mode"
cat /proc/sys/kernel/random/boot_id > "$MODDIR/active-dvfs-boot-id"
