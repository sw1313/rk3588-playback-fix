#!/system/bin/sh
MODDIR=${0%/*}
# A bounded late-start check; it does not restart Android codec services.
attempt=0
while [ "$(getprop sys.boot_completed)" != 1 ] && [ "$attempt" -lt 60 ]; do
    sleep 2
    attempt=$((attempt + 1))
done
[ "$(getprop sys.boot_completed)" = 1 ] || exit 0
[ ! -e "$MODDIR/disable" ] || exit 0
status="$MODDIR/status.txt"
# Boot-launched /data executables do not automatically have /vendor in their
# linker search paths. Limit the override to each helper process and its ABI.
if LD_LIBRARY_PATH=/vendor/lib64 timeout 8 "$MODDIR/bin/probe64" /vendor/lib64/libmpp.so "$MODDIR/contracts/arm64-symbols.txt" c08762e >"$status" 2>&1 &&
   LD_LIBRARY_PATH=/vendor/lib timeout 8 "$MODDIR/bin/probe32" /vendor/lib/libmpp.so "$MODDIR/contracts/arm-symbols.txt" c08762e >>"$status" 2>&1; then
    echo "Library load checks passed. This is not a full Codec2 playback test." >>"$status"
else
    touch "$MODDIR/disable"
    echo "Library check failed. Module disabled for the next reboot; no automatic reboot performed." >>"$status"
fi
