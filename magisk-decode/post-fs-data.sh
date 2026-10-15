#!/system/bin/sh
MODROOT=${0%/*}
if sh "$MODROOT/assemble.sh"; then
 value=$(sed -n 's/^codec2=//p' "$MODROOT/active-config")
 resetprop -n debug.rkcodec2.surface_upgrade "$value"
 resetprop -n debug.rkcodec2.hdr_static_buffer "$(sed -n 's/^hdr_static=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkhdr.static "$(sed -n 's/^hdr_static=//p' "$MODROOT/active-config")"
 resetprop -n debug.rkcodec2.hdr10plus "$(sed -n 's/^hdr10plus=//p' "$MODROOT/active-config")"
 resetprop -n vendor.rkhdr.adapter pending
else
 touch "$MODROOT/disable" "$MODROOT/skip_mount"
 resetprop -n debug.rkcodec2.surface_upgrade 0
 resetprop -n debug.rkcodec2.hdr_static_buffer 0
 resetprop -n debug.rkhdr.static 0
 resetprop -n debug.rkcodec2.hdr10plus 0
 echo "FAIL before mounting; no decode overlays enabled." > "$MODROOT/status.txt"
fi
