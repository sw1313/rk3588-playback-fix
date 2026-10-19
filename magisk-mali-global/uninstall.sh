#!/system/bin/sh
MODDIR=${0%/*}
. "$MODDIR/kernel-state.sh"
. "$MODDIR/cold-state.sh"
cold_restore || exit 1
echo off > "$STATE/cold-mode"
echo off > /data/adb/rk3588-mali-global/mode
# Retain private boot backup: the independent ramdisk keeper also handles disable.

echo stock > /data/adb/rk3588-mali-global/kernel-mode
echo off > /data/adb/rk3588-mali-global/dvfs-mode
