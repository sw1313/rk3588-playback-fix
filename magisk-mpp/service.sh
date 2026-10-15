#!/system/bin/sh
MODDIR=${0%/*}
[ ! -e "$MODDIR/disable" ] || exit 0
cd "$MODDIR" || exit 1
recover() {
 sh "$MODDIR/mode-control.sh" off >> "$MODDIR/status.txt" 2>&1
 echo "FAIL: $1; rebooting once to restore original libraries." >> "$MODDIR/status.txt"
 sync
 reboot
 exit 1
}
: > "$MODDIR/status.txt"
sha256sum -c contracts/active.sha256 >> "$MODDIR/status.txt" 2>&1 || recover "active payload mismatch"
sha256sum -c contracts/vendor.sha256 >> "$MODDIR/status.txt" 2>&1 || recover "vendor consumer ABI changed"
for abi in 32 64; do
 if [ "$abi" = 64 ]; then dir=lib64; label=arm64; else dir=lib; label=arm; fi
 LD_LIBRARY_PATH=/vendor/$dir timeout 8 "$MODDIR/bin/probe$abi" /vendor/$dir/libmpp.so "$MODDIR/contracts/$label-symbols.txt" c08762e >> "$MODDIR/status.txt" 2>&1 || recover "MPP loading failed"
 LD_LIBRARY_PATH=/vendor/$dir timeout 8 "$MODDIR/bin/probe$abi" /vendor/$dir/libvpu.so "$MODDIR/contracts/vpu-$label-symbols.txt" - >> "$MODDIR/status.txt" 2>&1 || recover "VPU loading failed"
done
echo "PASS paired MPP/VPU load, symbols and pinned consumer ABI. Playback validation is separate." >> "$MODDIR/status.txt"
