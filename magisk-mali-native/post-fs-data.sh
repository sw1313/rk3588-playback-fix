#!/system/bin/sh
# Register the selected package before framework graphics services start.
# App opt-in settings remain explicit; never enable the global all-app mode.
STATE=/data/adb/rk3588-mali-native
if [ "$(cat "$STATE/enabled" 2>/dev/null)" = 1 ]; then
 selected=$(cat "$STATE/selected")
 case "$selected" in g15|g18) resetprop ro.gfx.driver.0 "com.sw1313.malidriver.$selected";; esac
fi
exit 0
