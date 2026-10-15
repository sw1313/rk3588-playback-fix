#!/system/bin/sh
resetprop --delete debug.rkcodec2.surface_upgrade
resetprop --delete debug.rkcodec2.buffer_upgrade
resetprop --delete debug.rkcodec2.hdr_static_buffer
resetprop --delete debug.rkcodec2.hdr10plus
resetprop --delete debug.rkhdr.static
echo "Unified decode stack removed. Reboot restores firmware libraries; previous standalone modules remain disabled."
