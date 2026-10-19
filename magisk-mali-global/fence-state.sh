#!/system/bin/sh
# kernel-state.sh and cold-state.sh must be sourced first.
read_fence(){ v=$(cat "$STATE/fence-mode" 2>/dev/null);case "$v" in on) echo on;;*) echo off;;esac; }
fence_supported(){ kernel_supported && cold_prepared && grep -q 'csf_fence_safe' "$MODDIR/kernel/rk_mali_next.ko"; }
fence_active(){
 [ "$(kernel_active)" = next ] || { echo off;return; }
 case "$(cat /sys/module/rk_mali_next/parameters/csf_fence_safe 2>/dev/null)" in
 Y) echo on;;N) echo off;;*) echo unknown;;esac
}
