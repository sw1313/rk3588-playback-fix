#!/system/bin/sh
MODDIR=${0%/*}
until [ "$(getprop sys.boot_completed)" = 1 ]; do sleep 2; done
status="$MODDIR/status.txt"
{
    echo "RK3588 RGA 1.10.6 boot check"
    date -u
    echo "Kernel: $(cat /sys/kernel/debug/rkrga/driver_version 2>/dev/null)"
    echo "ARM64:"
    timeout 15 /system/bin/env LD_LIBRARY_PATH=/vendor/lib64:/system/lib64 "$MODDIR/bin/probe64" /vendor/lib64/librga.so "$MODDIR/contracts/arm64-symbols.txt" load '1.10.6_[3]' component
    result64=$?
    echo "ARM32:"
    timeout 15 /system/bin/env LD_LIBRARY_PATH=/vendor/lib:/system/lib "$MODDIR/bin/probe32" /vendor/lib/librga.so "$MODDIR/contracts/arm-symbols.txt" load '1.10.6_[3]' component
    result32=$?
    if [ "$result64" = 0 ] && [ "$result32" = 0 ]; then
        echo "PASS: both active libraries and Codec2 dependencies load."
    else
        touch "$MODDIR/disable"
        echo "FAIL: module disabled for rollback on the next reboot."
        echo "Current boot mounts remain active until reboot."
    fi
} > "$status" 2>&1
