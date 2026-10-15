#!/system/bin/sh
MODDIR=${0%/*}
STATE=/data/adb/rk3588-mali-global
mkdir -p "$STATE"
case "$1" in
 status) echo "Scope: system-wide ARM32 + ARM64";echo "Requested: $(cat "$STATE/mode" 2>/dev/null)";echo "Applied this boot: $(cat "$MODDIR/active-mode" 2>/dev/null)";echo "Runtime: $(getprop vendor.rkmaliglobal.ready)";;
 on) echo on > "$STATE/mode";rm -f "$STATE/boot-pending";echo "Global g18p0 selected; reboot to apply.";;
 off) echo off > "$STATE/mode";echo "Original system driver selected; reboot to apply.";;
 *) echo "Usage: mode-control.sh on|off|status";exit 1;;
esac
