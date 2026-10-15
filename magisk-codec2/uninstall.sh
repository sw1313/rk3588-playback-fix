#!/system/bin/sh
# Systemless overlays are removed by Magisk after reboot.
resetprop --delete debug.rkcodec2.surface_upgrade
