SKIPUNZIP=0
[ "$BOOTMODE" = true ] || abort "Install from Magisk app, not recovery."
[ "$(getprop ro.board.platform)" = rk3588 ] || abort "RK3588 required."
[ "$API" = 32 ] || abort "This Frame Guard is verified for Android 12 only."
chmod 755 "$MODPATH"/*.sh
sh "$MODPATH/control.sh" status
ui_print "Original settings are saved on first apply; Action toggles restore/apply."
ui_print "Disabling the module alone does not undo persisted settings. Use Action or uninstall."
