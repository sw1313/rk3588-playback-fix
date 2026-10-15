#!/system/bin/sh
MODROOT=${0%/*}
[ ! -e "$MODROOT/disable" ] || exit 0
. "$MODROOT/common.sh"
# Validate the selections actually assembled for this boot, not a user edit
# that may already be queued for the next boot.
value() { sed -n "s/^$1=//p" "$MODROOT/active-config"; }
MPP=$(value mpp); RGA=$(value rga); C2=$(value codec2); YUV=$(value libyuv); SOFT=$(value software)
HDR_STATIC=$(value hdr_static); HDR_PLUS=$(value hdr10plus)
recover() {
 touch "$MODROOT/disable"
 resetprop debug.rkcodec2.surface_upgrade 0
 resetprop -n debug.rkcodec2.hdr_static_buffer 0
 resetprop -n debug.rkcodec2.hdr10plus 0
 resetprop -n debug.rkhdr.static 0
 echo "FAIL: $1; full stack disabled, rebooting once to restore originals." >> "$MODROOT/status.txt"
 sync
 reboot
 exit 1
}
: > "$MODROOT/status.txt"
if [ -s "$MODROOT/active.sha256" ]; then
 sha256sum -c "$MODROOT/active.sha256" >> "$MODROOT/status.txt" 2>&1 || recover "active file hashes differ"
fi
if [ "$HDR_STATIC" = 1 ] && [ "$(getprop vendor.rkhdr.adapter)" != active ]; then
 # Some vendor HALs start before Magic Mount. Capture the actual loaded image,
 # then reload the compositor once after overlays exist. Restart framework
 # consumers as well; they must not retain a dead SurfaceFlinger connection.
 {
  echo "HDR initial adapter=$(getprop vendor.rkhdr.adapter) boot=$(getprop sys.boot_completed)"
  pid=$(getprop init.svc_debug_pid.vendor.hwcomposer-2-1)
  [ -z "$pid" ] || grep 'hwcomposer.rk30board' "/proc/$pid/maps"
  logcat -b all -d -t 100 -s RKHdrHwc
 } > "$MODROOT/hdr-startup.txt" 2>&1
 stop surfaceflinger
 stop vendor.hwcomposer-2-1
 n=0
 while [ "$(getprop init.svc.vendor.hwcomposer-2-1)" != stopped ] && [ "$n" -lt 50 ]; do sleep 0.1; n=$((n+1)); done
 start vendor.hwcomposer-2-1
 start surfaceflinger
 setprop ctl.restart zygote
 setprop ctl.restart zygote_secondary
 n=0
 while [ "$(getprop vendor.rkhdr.adapter)" != active ] && [ "$n" -lt 50 ]; do sleep 0.2; n=$((n+1)); done
 echo "HDR reloaded adapter=$(getprop vendor.rkhdr.adapter)" >> "$MODROOT/hdr-startup.txt"
fi
for component in mpp rga codec2 libyuv software hdr; do
 selected "$component" || continue
 check=vendor.sha256
 case "$component" in codec2|libyuv|software|hdr) check=platform.sha256 ;; esac
 sha256sum -c "$MODROOT/components/$component/contracts/$check" >> "$MODROOT/status.txt" 2>&1 || recover "$component dependency ABI differs"
done
if [ "$MPP" = 1 ]; then
 c="$MODROOT/components/mpp"
 for abi in 32 64; do
  if [ "$abi" = 64 ]; then dir=lib64; label=arm64; else dir=lib; label=arm; fi
  timeout 8 /system/bin/env LD_LIBRARY_PATH=/vendor/$dir "$c/bin/probe$abi" /vendor/$dir/libmpp.so "$c/contracts/$label-symbols.txt" c08762e >> "$MODROOT/status.txt" 2>&1 || recover "MPP load failed"
  timeout 8 /system/bin/env LD_LIBRARY_PATH=/vendor/$dir "$c/bin/probe$abi" /vendor/$dir/libvpu.so "$c/contracts/vpu-$label-symbols.txt" - >> "$MODROOT/status.txt" 2>&1 || recover "VPU load failed"
  timeout 8 /system/bin/env LD_LIBRARY_PATH=/vendor/$dir "$c/bin/hdr_lifetime$abi" /vendor/$dir/libmpp.so >> "$MODROOT/status.txt" 2>&1 || recover "MPP HDR lifetime checks failed"
 done
fi
if [ "$RGA" = 1 ]; then
 c="$MODROOT/components/rga"
 for abi in 32 64; do
  if [ "$abi" = 64 ]; then dir=lib64; label=arm64; else dir=lib; label=arm; fi
  timeout 15 /system/bin/env LD_LIBRARY_PATH=/vendor/$dir:/system/$dir "$c/bin/probe$abi" /vendor/$dir/librga.so "$c/contracts/$label-symbols.txt" load '1.10.6_[3]' component >> "$MODROOT/status.txt" 2>&1 || recover "RGA load failed"
 done
fi
if [ "$YUV" = 1 ]; then
 timeout 15 "$MODROOT/components/libyuv/bin/quality_probe64" /system/lib64/libyuv.so >> "$MODROOT/status.txt" 2>&1 || recover "libyuv precision test failed"
fi
if [ "$C2" = 1 ]; then
 c="$MODROOT/components/codec2"
 sh "$c/check-runtime.sh" >> "$MODROOT/status.txt" 2>&1 || recover "Codec2 runtime profile differs"
fi
if [ "$SOFT" = 1 ]; then
 sh "$MODROOT/components/software/mount-apex.sh" >> "$MODROOT/status.txt" 2>&1 || recover "software APEX mount or factory check failed"
fi
if [ "$C2" = 1 ] || [ "$SOFT" = 1 ]; then
 stop media
 stop android-hardware-media-c2-hal-1-1
 start android-hardware-media-c2-hal-1-1
 start media
fi
attempt=0
while [ "$(getprop sys.boot_completed)" != 1 ] && [ "$attempt" -lt 45 ]; do sleep 2; attempt=$((attempt+1)); done
[ "$(getprop sys.boot_completed)" = 1 ] || recover "boot did not complete"
if [ "$C2" = 1 ]; then
 c="$MODROOT/components/codec2"
 timeout 25 /system/bin/env LD_LIBRARY_PATH=/vendor/lib:/system/lib:/apex/com.android.vndk.v32/lib /vendor/bin/hw/android.hardware.media.c2@1.1-service --check-only > "$MODROOT/codec2-status.txt" 2>&1 || recover "Codec2 HIDL preflight failed"
 while read -r name; do
  grep -F "COMPONENT $name rank=" "$MODROOT/codec2-status.txt" >/dev/null || recover "missing original Codec2 entry: $name"
 done < "$c/contracts/stock-components.txt"
fi
if [ "$HDR_STATIC" = 1 ]; then
 c="$MODROOT/components/hdr"
 [ "$(getprop vendor.rkhdr.adapter)" = active ] || recover "HDR adapter not active in original composer"
 # Active vendor bytes were already verified above. Load their identical module
 # sources in this separate test process, whose default linker namespace does
 # not grant direct dlopen access to vendor hardware-plugin paths.
 timeout 10 /system/bin/env LD_LIBRARY_PATH=/vendor/lib64/hw:/vendor/lib64:/system/lib64:/apex/com.android.vndk.v32/lib64 "$c/bin/check64" "$c/system/vendor/lib64/hw/hwcomposer.rk30board.so" "$c/system/vendor/lib64/hw/hwcomposer.rk30board.rkhdr.so" >> "$MODROOT/status.txt" 2>&1 || recover "HDR original-HWC interface check failed"
fi
echo "PASS unified decode stack, selected components and original interfaces. Playback tests are separate." >> "$MODROOT/status.txt"
