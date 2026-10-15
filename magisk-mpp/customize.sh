#!/system/bin/sh
# Magisk sources this script after extracting the module.
[ "$BOOTMODE" = true ] || abort "Install from the Magisk app on a running Android system."
[ "$API" = 32 ] || abort "This build is restricted to Android 12 / API 32."
[ "$ARCH" = arm64 ] || abort "An ARM64 system with ARM32 vendor support is required."
[ "$(getprop ro.board.platform)" = rk3588 ] || abort "This module is restricted to RK3588."

ui_print "Checking the tested vendor stack and packaged payload..."
cd "$MODPATH" || abort "Module directory unavailable."
sha256sum -c checksums.sha256 >/dev/null 2>&1 || abort "Module payload checksum mismatch."
sha256sum -c contracts/vendor.sha256 >/dev/null 2>&1 || abort "Vendor components differ from the tested firmware. No files were replaced."

check_current_mpp() {
    current=$(sha256sum "$1" 2>/dev/null) || return 1
    current=${current%% *}
    candidate=$(sha256sum "$2" 2>/dev/null) || return 1
    candidate=${candidate%% *}
    [ "$current" = "$3" ] || [ "$current" = "$4" ] || [ "$current" = "$candidate" ] || grep -Fx "$current  $1" "$MODPATH/contracts/prior-active.sha256" >/dev/null
}
check_current_mpp /vendor/lib64/libmpp.so "$MODPATH/system/vendor/lib64/libmpp.so" 00174d33a20fa8fede30f5887ad0eb3086accf2078230c2d1b7f325b591a2b1f 4747b4cd1a9d25c5060f0eae080489b0e96a2d40fb37109fad7941f389890a25 || abort "Unknown ARM64 MPP baseline."
check_current_mpp /vendor/lib/libmpp.so "$MODPATH/system/vendor/lib/libmpp.so" 6d0c3e2669741cd72b487b059dc9c641f80d97a8e5b304b9059371f6f1e0ce09 bb04d28ce91a1c3b264a02a982c287c1206967dfb6c2e70a1c842b01243affc5 || abort "Unknown ARM32 MPP baseline."
check_current_mpp /vendor/lib64/libvpu.so "$MODPATH/system/vendor/lib64/libvpu.so" 149bfb08da00af59e8a74fd775562d640cd6a17d84c9ba1a14d1cba089e70d66 - || abort "Unknown ARM64 VPU baseline."
check_current_mpp /vendor/lib/libvpu.so "$MODPATH/system/vendor/lib/libvpu.so" baa4c8469b3a25584ef6c39f894a23be6b1353bfaf1ebbc047c6fae45322cefb - || abort "Unknown ARM32 VPU baseline."

set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/bin/probe64" 0 0 0755
set_perm "$MODPATH/bin/probe32" 0 0 0755
set_perm "$MODPATH/bin/hdr_lifetime64" 0 0 0755
set_perm "$MODPATH/bin/hdr_lifetime32" 0 0 0755
ui_print "Checking ARM64 library loading and vendor symbols..."
timeout 8 "$MODPATH/bin/probe64" "$MODPATH/system/vendor/lib64/libmpp.so" "$MODPATH/contracts/arm64-symbols.txt" c08762e || abort "ARM64 MPP preflight failed."
ui_print "Checking ARM32 library loading and vendor symbols..."
timeout 8 "$MODPATH/bin/probe32" "$MODPATH/system/vendor/lib/libmpp.so" "$MODPATH/contracts/arm-symbols.txt" c08762e || abort "ARM32 MPP preflight failed."
timeout 8 /system/bin/env LD_LIBRARY_PATH="$MODPATH/system/vendor/lib64:/vendor/lib64" "$MODPATH/bin/probe64" "$MODPATH/system/vendor/lib64/libvpu.so" "$MODPATH/contracts/vpu-arm64-symbols.txt" - || abort "ARM64 VPU preflight failed."
timeout 8 /system/bin/env LD_LIBRARY_PATH="$MODPATH/system/vendor/lib:/vendor/lib" "$MODPATH/bin/probe32" "$MODPATH/system/vendor/lib/libvpu.so" "$MODPATH/contracts/vpu-arm-symbols.txt" - || abort "ARM32 VPU preflight failed."
timeout 8 /system/bin/env LD_LIBRARY_PATH="$MODPATH/system/vendor/lib64:/vendor/lib64" "$MODPATH/bin/hdr_lifetime64" "$MODPATH/system/vendor/lib64/libmpp.so" || abort "ARM64 HDR lifetime check failed."
timeout 8 /system/bin/env LD_LIBRARY_PATH="$MODPATH/system/vendor/lib:/vendor/lib" "$MODPATH/bin/hdr_lifetime32" "$MODPATH/system/vendor/lib/libmpp.so" || abort "ARM32 HDR lifetime check failed."
ui_print "Paired MPP/VPU compatibility libraries ready. Reboot to activate."
ui_print "Action disables this module; reboot again to restore stock libraries."
ui_print "Kernel, Codec2, clocks and LSPosed settings are retained."
