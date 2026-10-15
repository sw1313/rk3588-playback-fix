#!/system/bin/sh
[ "$BOOTMODE" = true ] && [ "$API" = 32 ] && [ "$ARCH" = arm64 ] || abort "Requires verified running API32 ARM64 firmware"
[ "$(getprop ro.board.platform)" = rk3588 ] && [ "$(uname -r)" = 5.10.66 ] || abort "Unsupported platform/kernel"
cd "$MODPATH" || abort "Module directory unavailable"
sha256sum -c checksums.sha256 >/dev/null 2>&1 || abort "Payload hash differs"
sha256sum -c contracts/platform.sha256 >/dev/null 2>&1 || abort "Original graphics profile differs"
STATE=/data/adb/rk3588-mali-native
if [ ! -f "$STATE/original-driver" ]; then
 [ -z "$(getprop ro.gfx.driver.0)" ] || abort "An existing graphics driver package is configured"
 case "$(settings get global updatable_driver_all_apps)" in null|0|'') ;; *) abort "Global graphics override is already set";; esac
 case "$(settings get global updatable_driver_production_allowlist)" in null|'') ;; *) abort "Existing graphics allowlist must be retained";; esac
 mkdir -p "$STATE" || abort "Cannot create state"
 getprop ro.gfx.driver.0 > "$STATE/original-driver"
 settings get global updatable_driver_production_opt_in_apps > "$STATE/original-opt-in"
 echo g18 > "$STATE/selected"
 echo 1 > "$STATE/enabled"
 echo com.sw1313.malitest > "$STATE/apps"
 : > "$STATE/owned-apps"
fi
set_perm_recursive "$MODPATH" 0 0 0755 0644
for f in mode-control.sh post-fs-data.sh service.sh action.sh uninstall.sh bin/load_probe64; do set_perm "$MODPATH/$f" 0 0 0755; done
for candidate in g15 g18; do
 apk="$MODPATH/system/app/RKMali${candidate}/RkMali-$candidate.apk"
 prefix="$apk!/lib/arm64-v8a"
 timeout 10 /system/bin/env LD_LIBRARY_PATH="$prefix:/vendor/lib64:/system/lib64:/apex/com.android.vndk.v32/lib64" "$MODPATH/bin/load_probe64" "$prefix/libGLES_mali.so" || abort "Native driver loading preflight failed"
done
ui_print "Native driver packages staged; only the independent test app is opted in initially. Reboot to register system driver APKs."
