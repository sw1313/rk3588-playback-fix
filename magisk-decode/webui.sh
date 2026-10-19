#!/system/bin/sh
# Fixed-command bridge: no arbitrary command, path or configuration evaluation.
CURRENTROOT=${0%/*}
MODROOT=$CURRENTROOT
NEXTROOT=/data/adb/modules_update/${CURRENTROOT##*/}
if [ "$CURRENTROOT" = "/data/adb/modules/${CURRENTROOT##*/}" ] && [ -f "$NEXTROOT/config" ]; then
 MODROOT=$NEXTROOT
fi
. "$MODROOT/common.sh"
keys='mpp rga codec2 codec2_sdk codec2_sdk_next codec2_buffer libyuv software hdr_static hdr10plus p010_fast av1_native av1_grain av1_afbc av1_texture_afbc codec2_lifecycle hwc_metadata_reuse hwc_scaled_overlay hwc_mode_cache hwc_client_hdr10 hwui_rgb10 gralloc_hdr_lease codec2_delay_notify hevc_native_surface hevc_eos_pending p010_pairs p010_copy_pool codec2_surface codec2_native_backpressure hdmi_audio_guard display_qms display_policy display_session'
version() {
 v=$(sed -n 's/^version=//p' "$1/module.prop" 2>/dev/null)
 case "$v" in ''|*[!0-9A-Za-z._+-]*) printf 'unknown' ;; *) printf '%s' "$v" ;; esac
}
effective() {
 case "$1" in
 mpp) echo "$MPP" ;; rga) echo "$RGA" ;; codec2) echo "$C2" ;;
 codec2_sdk) echo "$SDK" ;; codec2_sdk_next) echo "$SDK_NEXT" ;;
 codec2_surface) echo "$SURFACE" ;;
 codec2_native_backpressure) echo "$NATIVE_BP" ;;
 codec2_buffer) echo "$BUFFER" ;; libyuv) echo "$YUV" ;; software) echo "$SOFT" ;;
 hdr_static) echo "$HDR_STATIC" ;; hdr10plus) echo "$HDR_PLUS" ;; p010_fast) echo "$P010_FAST" ;;
 av1_native) echo "$AV1_NATIVE" ;; av1_grain) echo "$AV1_GRAIN" ;; av1_afbc) echo "$AV1_AFBC" ;;
 av1_texture_afbc) echo "$AV1_TEXTURE_AFBC" ;;
 codec2_lifecycle) echo "$LIFECYCLE" ;;
 hwc_metadata_reuse) echo "$HWC_REUSE" ;;
 hwc_scaled_overlay) echo "$HWC_SCALE" ;;
 hwc_mode_cache) echo "$HWC_MODE_CACHE" ;;
 hwc_client_hdr10) echo "$HWC_CLIENT_HDR" ;;
 hwui_rgb10) echo "$HWUI_RGB10" ;;
 gralloc_hdr_lease) echo "$GRALLOC_HDR_LEASE" ;;
 codec2_delay_notify) echo "$DELAY_NOTIFY" ;;
 hevc_native_surface) echo "$HEVC_NATIVE" ;;
 hevc_eos_pending) echo "$HEVC_EOS" ;;
 p010_pairs) echo "$P010_PAIRS" ;;
 p010_copy_pool) echo "$P010_POOL" ;;
 hdmi_audio_guard) echo "$HDMI_GUARD" ;;
 display_qms) echo "$DISPLAY_QMS" ;;
 display_policy) echo "$DISPLAY_POLICY" ;;
 display_session) echo "$DISPLAY_SESSION" ;;
 esac
}
bit() {
 b=$(sed -n "s/^$2=//p" "$1" 2>/dev/null)
 case "$b" in 0|1) printf '%s' "$b" ;; *) printf 'null' ;; esac
}
status() {
 enabled=true; [ ! -e "$MODROOT/disable" ] || enabled=false
 removing=false; [ ! -e "$CURRENTROOT/remove" ] && [ ! -e "$MODROOT/remove" ] || removing=true
 staged=false; [ "$MODROOT" = "$CURRENTROOT" ] || staged=true
 valid=true; resolve || valid=false
 active_known=false; active_enabled=false
 boot=$(cat /proc/sys/kernel/random/boot_id 2>/dev/null)
 receipt=$(cat "$CURRENTROOT/active-boot-id" 2>/dev/null)
 if [ -n "$boot" ] && [ -n "$receipt" ]; then
  active_known=true
  if [ "$boot" = "$receipt" ]; then
   active_enabled=true
   for key in $keys; do
    [ "$(bit "$CURRENTROOT/active-config" "$key")" != null ] || active_known=false
   done
  fi
 fi
 active_version=unknown
 if [ "$active_enabled" = true ]; then
  v=$(cat "$CURRENTROOT/active-version" 2>/dev/null)
  case "$v" in ''|*[!0-9A-Za-z._+-]*) ;; *) active_version=$v ;; esac
 fi
 pending=false
 [ "$staged" = false ] && [ "$removing" = false ] || pending=true
 [ "$active_known" = true ] || pending=true
 [ "$enabled" = "$active_enabled" ] || pending=true
 [ "$active_enabled" = false ] || [ "$(version "$MODROOT")" = "$active_version" ] || pending=true
 health=unknown
 if [ "$active_enabled" = true ]; then
  health=checking
  if grep -q '^FAIL' "$CURRENTROOT/status.txt" 2>/dev/null; then health=failed
  elif grep -q '^PASS unified decode stack' "$CURRENTROOT/status.txt" 2>/dev/null; then health=passed; fi
 fi
 printf '{"schema":1,"version":"%s","activeVersion":"%s","enabled":%s,"activeKnown":%s,"activeEnabled":%s,"valid":%s,"staged":%s,"removing":%s,"health":"%s","components":{' "$(version "$MODROOT")" "$active_version" "$enabled" "$active_known" "$active_enabled" "$valid" "$staged" "$removing" "$health"
 sep=''
 for key in $keys; do
  requested=$(bit "$MODROOT/config" "$key")
  next=null
  if [ "$valid" = true ]; then
   next=$(effective "$key")
   [ "$enabled" = true ] && [ "$removing" = false ] || next=0
  fi
  active=null
  if [ "$active_known" = true ]; then
   active=0
   [ "$active_enabled" = false ] || active=$(bit "$CURRENTROOT/active-config" "$key")
   if [ "$key" = display_qms ] && [ "$active" = 1 ]; then
    [ "$(cat /sys/module/rk_decode_qms/parameters/owner_online 2>/dev/null)" = 1 ] &&
     [ "$(getprop vendor.rkhdr.qms_boot)" = "$boot" ] &&
     [ "$(getprop vendor.rkhdr.qms)" = active ] || active=0
   fi
   if [ "$key" = display_session ] && [ "$active" = 1 ]; then
    [ "$(getprop vendor.rkdecode.refresh_session)" = active ] || active=0
   fi
  fi
  [ "$next" = "$active" ] || pending=true
  printf '%s"%s":{"requested":%s,"effective":%s,"active":%s}' "$sep" "$key" "$requested" "$next" "$active"
  sep=,
 done
 printf '},"pendingReboot":%s}\n' "$pending"
}
case "$1" in
 bundle)
  [ "$#" = 3 ] || exit 2
  . "$MODROOT/bundles.sh"
  bundle_members "$2" >/dev/null || exit 2
  case "$3" in on|off) ;; *) exit 2 ;; esac
  [ ! -e "$CURRENTROOT/remove" ] && [ ! -e "$MODROOT/remove" ] || exit 3
  sh "$CURRENTROOT/mode-control.sh" bundle "$2" "$3" >&2 || exit $?
  status
  ;;
 status) [ "$#" = 1 ] || exit 2; status ;;
 backend)
  [ "$#" = 2 ] || exit 2
  case "$2" in legacy|simd|latest) ;; *) exit 2 ;; esac
  [ ! -e "$CURRENTROOT/remove" ] && [ ! -e "$MODROOT/remove" ] || exit 3
  sh "$CURRENTROOT/mode-control.sh" codec2_backend "$2" >&2 || exit $?
  status
  ;;
 set)
  [ "$#" = 3 ] || exit 2
  case " $keys all " in *" $2 "*) ;; *) exit 2 ;; esac
  case "$3" in on|off) ;; *) exit 2 ;; esac
  [ ! -e "$CURRENTROOT/remove" ] && [ ! -e "$MODROOT/remove" ] || exit 3
  if [ "$2" = all ]; then
   sh "$CURRENTROOT/mode-control.sh" all "$3" >&2 || exit $?
  else
   sh "$CURRENTROOT/mode-control.sh" linked "$2" "$3" >&2 || exit $?
  fi
  status
  ;;
 *) exit 2 ;;
esac
