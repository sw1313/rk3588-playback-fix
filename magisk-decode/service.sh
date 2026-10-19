#!/system/bin/sh
MODROOT=${0%/*}
[ ! -e "$MODROOT/disable" ] || exit 0
. "$MODROOT/common.sh"
# Validate the selections actually assembled for this boot, not a user edit
# that may already be queued for the next boot.
value() { sed -n "s/^$1=//p" "$MODROOT/active-config"; }
MPP=$(value mpp); RGA=$(value rga); C2=$(value codec2); YUV=$(value libyuv); SOFT=$(value software)
HDR_STATIC=$(value hdr_static); HDR_PLUS=$(value hdr10plus)
SDK=$(value codec2_sdk)
SDK_NEXT=$(value codec2_sdk_next)
recover() {
 touch "$MODROOT/disable"
 resetprop debug.rkcodec2.surface_upgrade 0
 resetprop -n debug.rkcodec2.native_surface_upgrade 0
 resetprop -n debug.rkcodec2.native_backpressure 0
 resetprop -n debug.rkcodec2.av1_afbc 0
 resetprop -n debug.rkcodec2.av1_texture_afbc 0
 resetprop -n debug.rkcodec2.lifecycle_fix 0
 resetprop -n debug.rkcodec2.av1_grain 0
 resetprop -n debug.rkcodec2.av1_direct 0
 resetprop -n debug.rkcodec2.buffer_upgrade 0
 resetprop -n debug.rkcodec2.p010_fast 0
 resetprop -n debug.rkcodec2.hdr_static_buffer 0
 resetprop -n debug.rkcodec2.hdr10plus 0
 resetprop -n debug.rkcodec2.delay_notify 0
 resetprop -n debug.rkcodec2.hevc_native_surface 0
 resetprop -n debug.rkcodec2.hevc_eos_pending 0
 resetprop -n debug.rkcodec2.p010_pairs 0
 resetprop -n debug.rkcodec2.p010_pool 0
 resetprop -n debug.rkcodec2.gralloc_hdr_lease 0
 resetprop -n debug.rkhdr.client_hdr10 0
 resetprop -n debug.rkhdr.mode_cache 0
 resetprop -n debug.rkhdr.hdmi_audio_guard 0
 resetprop -n debug.rkhdr.metadata_reuse 0
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
cold_start=false
if grep -qw 'initcall_blacklist=kbase_platform_driver_init' /proc/cmdline;then
 cold_start=true
 # Mali's ramdisk gate releases the first HWC only after all Magic Mount
 # overlays exist. Do not mistake that pending first execution for a stale HAL.
 boot=$(cat /proc/sys/kernel/random/boot_id)
 mali=/data/adb/rk3588-mali-global
 n=0
 while [ "$n" -lt 75 ];do
  read -r receipt_boot receipt_profile receipt_phase < "$mali/cold-receipt"
  [ "$receipt_boot" != "$boot" ] || [ "$receipt_phase" != released ] || break
  sleep 1;n=$((n+1))
 done
 [ "$receipt_boot" = "$boot" ] && [ "$receipt_phase" = released ] || recover "cold GPU first binding not released"
 if [ "$HDR_STATIC" = 1 ];then
  n=0
  while [ "$(getprop vendor.rkhdr.adapter)" != active ] && [ "$n" -lt 50 ];do sleep 0.2;n=$((n+1));done
  [ "$(getprop vendor.rkhdr.adapter)" = active ] || recover "cold first HWC did not load HDR adapter"
 fi
fi
if [ "$cold_start" = false ] && [ "$HDR_STATIC" = 1 ] && [ "$(getprop vendor.rkhdr.adapter)" != active ]; then
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
if [ "$(value hdmi_audio_guard)" = 1 ]; then
 # libandroid_runtime can precede Magic Mount. Reload framework consumers once
 # after boot overlays exist; never touch GPU/KMD binding or Zygisk selection.
 pid=$(pidof system_server)
 if [ -z "$pid" ] || ! grep -q '/system/lib64/libhevt.so' "/proc/$pid/maps"; then
  setprop ctl.restart zygote
  setprop ctl.restart zygote_secondary
 fi
 n=0
 while [ "$n" -lt 80 ];do
  pid=$(pidof system_server)
  if [ -n "$pid" ] && grep -q '/system/lib64/libhevt.so' "/proc/$pid/maps" && [ "$(getprop vendor.rkhdr.hdmi_event)" = active ];then break;fi
  sleep 0.5;n=$((n+1))
 done
 [ "$n" -lt 80 ] || recover "HDMI event framework adapter not active in system_server"
 echo "PASS HDMI transient event adapter; physical disconnects retained, application/mode tests separate." >> "$MODROOT/status.txt"
fi
if [ "$(value hwc_client_hdr10)" = 1 ]; then
 [ "$(getprop vendor.rkhdr.alloc10)" = active ] || recover "physical RGB10 allocator adapter not first-loaded"
 [ "$(getprop vendor.rkhdr.target_plan)" = active ] || recover "physical RGB10 HWC plane plan adapter not loaded"
 allocatorpid=$(getprop init.svc_debug_pid.vendor.gralloc-4-0)
 [ -n "$allocatorpid" ] && grep -q '/vendor/lib64/libga10.so' "/proc/$allocatorpid/maps" || recover "allocator target adapter receipt differs from actual process"
 # SF may have executed the original binary before Magic Mount, even when
 # the first vendor HWC correctly waits behind the cold GPU gate.
 if [ "$(getprop vendor.rkhdr.sf_target10)" != active ]; then
  stop surfaceflinger
  start surfaceflinger
  setprop ctl.restart zygote
  setprop ctl.restart zygote_secondary
 fi
 n=0
 while [ "$(getprop vendor.rkhdr.sf_target10)" != active ] && [ "$n" -lt 50 ]; do sleep 0.2;n=$((n+1));done
 [ "$(getprop vendor.rkhdr.sf_target10)" = active ] || recover "SF physical HDR target adapter not active"
 sfpid=$(pidof surfaceflinger)
 [ -n "$sfpid" ] && grep -q '/system/lib64/libsfhdr.so' "/proc/$sfpid/maps" || recover "SF target adapter receipt differs from actual process"
fi
if [ "$(value display_session)" = 1 ]; then
 if ! sh "$MODROOT/refresh-session-state.sh" verify; then
  sh "$MODROOT/refresh-session-state.sh" failed
  echo 'FAIL refresh-session startup; only display_session withdrawn, rebooting.' >> "$MODROOT/status.txt"
  sync; reboot; exit 1
 fi
 echo 'PASS system refresh-session attached; physical pause/seek/exit acceptance pending.' >> "$MODROOT/status.txt"
fi
for component in mpp rga codec2 libyuv software hdr; do
 selected "$component" || continue
 check=vendor.sha256
 case "$component" in codec2|libyuv|software|hdr) check=platform.sha256 ;; esac
 sha256sum -c "$MODROOT/components/$component/contracts/$check" >> "$MODROOT/status.txt" 2>&1 || recover "$component dependency ABI differs"
done
if [ "$SDK" = 1 ]; then
 sdk_component=codec2_sdk
 [ "$SDK_NEXT" != 1 ] || sdk_component=codec2_sdk_next
 sha256sum -c "$MODROOT/components/$sdk_component/contracts/platform.sha256" >> "$MODROOT/status.txt" 2>&1 || recover "SDK Codec2 additional API32 dependencies differ"
fi
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
if ! sh "$MODROOT/ddc-state.sh" recover; then
 echo "FAIL native HDMI reinitialization after EDID repair; no automatic repeat/reboot." >> "$MODROOT/status.txt"
fi
if ! sh "$MODROOT/ddc-state.sh" qualified; then
 echo "FAIL DDC startup qualification; physical HDMI acceptance pending." >> "$MODROOT/status.txt"
fi
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
if [ "$(value hwc_scaled_overlay)" = 1 ];then
 c="$MODROOT/components/hdr"
 timeout 10 /system/bin/env LD_LIBRARY_PATH=/vendor/lib64:/system/lib64:/apex/com.android.vndk.v32/lib64 "$c/bin/plane_probe64" "$c/system/vendor/lib64/hw/hwcomposer.rk30board.rkhdr.so" "$c/variants/hwcomposer.rk30board.scaled.so" >> "$MODROOT/status.txt" 2>&1 || recover "scaled overlay policy preflight failed"
fi

if [ "$(value display_qms)" = 1 ]; then
 # Startup qualification only: this marker permits the next boot trial. It
 # never asserts physical no-black, fractional timing or HDR acceptance.
 qstate=/data/adb/rk3588-decode-qms
 boot=$(cat /proc/sys/kernel/random/boot_id)
 qfail() {
  sed 's/^display_qms=.*/display_qms=0/' "$MODROOT/config" > "$MODROOT/config.qms-failed" &&
   mv "$MODROOT/config.qms-failed" "$MODROOT/config" || touch "$MODROOT/disable"
  echo "FAIL QMS startup: $1; display candidate disabled for reboot." >> "$MODROOT/status.txt"
  sync;reboot;exit 1
 }
 [ "$(cat /sys/module/rk_decode_qms/parameters/owner_online 2>/dev/null)" = 1 ] || qfail "cold owner absent"
 [ "$(getprop vendor.rkhdr.qms_boot)" = "$boot" ] && [ "$(getprop vendor.rkhdr.qms)" = active ] || qfail "HWC binding absent"
 timeout 4 "$MODROOT/components/qms/boot-stage64" status > "$qstate/runtime-status" || qfail "endpoint unavailable"
 grep -q '"error":0' "$qstate/runtime-status" || qfail "endpoint fault"
 read -r receipt_boot receipt_profile receipt_phase < "$qstate/receipt"
 [ "$receipt_boot" = "$boot" ] && [ "$receipt_phase" = prepared ] &&
 [ "$(sed -n 's/^profile_id //p' "$MODROOT/components/qms/profile")" = "$receipt_profile" ] || qfail "receipt mismatch"
 (umask 077;printf '%s\n' "$boot" > "$qstate/qualified.next") &&
  mv "$qstate/qualified.next" "$qstate/qualified" || qfail "qualification marker failed"
 echo "PASS QMS cold owner, pre-graphics baseline VTEM enable/restore and HWC endpoint; physical HDMI acceptance separate." >> "$MODROOT/status.txt"
fi

if [ "$(value display_policy)" = 1 ]; then
 n=0
 while [ "$(getprop sys.boot_completed)" != 1 ] && [ "$n" -lt 120 ]; do sleep 1;n=$((n+1));done
 if ! sh "$MODROOT/display-policy-state.sh" qualified; then
  sh "$MODROOT/display-policy-state.sh" failed
  echo "FAIL display policy startup; only display_policy disabled for reboot." >> "$MODROOT/status.txt"
  sync;reboot;exit 1
 fi
 echo "PASS display policy startup and mounted DEX; menu/fixed-mode acceptance separate." >> "$MODROOT/status.txt"
fi
echo "PASS unified decode stack, selected components and original interfaces. Playback tests are separate." >> "$MODROOT/status.txt"

if [ "$(value hwc_mode_cache)" = 1 ]; then
 [ "$(getprop vendor.rkhdr.mode_cache)" = active ] || recover "HWC mode cache adapter not active"
fi
if [ "$(value hwc_client_hdr10)" = 1 ]; then
 # A rejected physical target must not continue emitting HDR labels. The
 # adapter latches failure; reboot rollback removes all paired HDR overlays.
 while [ ! -e "$MODROOT/disable" ]; do
  [ "$(getprop vendor.rkhdr.sf_target10_error)" != 1 ] || recover "physical HDR target format failed"
  [ "$(getprop vendor.rkhdr.target_plan_error)" != 1 ] || recover "physical HDR target plane validation failed"
  sleep 2
 done
fi
