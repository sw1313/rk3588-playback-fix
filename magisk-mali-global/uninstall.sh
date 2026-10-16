#!/system/bin/sh
echo off > /data/adb/rk3588-mali-global/mode
# Magic Mount removes GPU overlays on reboot. Original partitions were not written.

echo stock > /data/adb/rk3588-mali-global/kernel-mode
