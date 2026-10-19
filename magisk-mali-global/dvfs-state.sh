#!/system/bin/sh
# Caller sources kernel-state.sh first. Only this verified devfreq node is used.
DVFS=/sys/class/devfreq/fb000000.gpu
read_dvfs(){ v=$(cat "$STATE/dvfs-mode" 2>/dev/null);case "$v" in on|off) echo "$v";;*) echo off;;esac; }
dvfs_supported(){
 kernel_supported || return 1
 [ "$(cat "$DVFS/governor" 2>/dev/null)" = simple_ondemand ] || return 1
 case "$(cat "$DVFS/polling_interval" 2>/dev/null)" in 20|100) ;;*) return 1;;esac
 [ -w "$DVFS/polling_interval" ]
}
dvfs_active(){
 [ "$(cat "$CURRENT/active-dvfs-boot-id" 2>/dev/null)" = "$boot" ] || { echo unknown;return; }
 v=$(cat "$CURRENT/active-dvfs-mode" 2>/dev/null)
 case "$v" in
 on) [ "$(kernel_active)" = next ] && [ "$(cat "$DVFS/governor" 2>/dev/null)" = simple_ondemand ] && [ "$(cat "$DVFS/polling_interval" 2>/dev/null)" = 20 ] && { echo on;return; };echo unknown;;
 off) echo off;;
 *) echo unknown;;
 esac
}
