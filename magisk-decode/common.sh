#!/system/bin/sh
# Caller sets MODROOT. Configuration is data, never shell code.
flag() {
 value=$(sed -n "s/^$1=//p" "$MODROOT/config" 2>/dev/null)
 case "$value" in 0|1) echo "$value" ;; *) return 1 ;; esac
}
resolve() {
 MPP=$(flag mpp) && RGA=$(flag rga) && C2=$(flag codec2) && YUV=$(flag libyuv) && SOFT=$(flag software) || return 1
 HDR_STATIC=$(flag hdr_static) && HDR_PLUS=$(flag hdr10plus) || return 1
 SURFACE=$(flag codec2_surface) || return 1
 NATIVE_BP=$(flag codec2_native_backpressure) || return 1
 SDK=$(flag codec2_sdk) || return 1
 SDK_NEXT=$(flag codec2_sdk_next) || return 1
 AV1_NATIVE=$(flag av1_native) || return 1
 AV1_GRAIN=$(flag av1_grain) || return 1
 AV1_AFBC=$(flag av1_afbc) || return 1
 AV1_TEXTURE_AFBC=$(flag av1_texture_afbc) || return 1
 LIFECYCLE=$(flag codec2_lifecycle) || return 1
 HWC_REUSE=$(flag hwc_metadata_reuse) || return 1
 HWC_SCALE=$(flag hwc_scaled_overlay) || return 1
 HWC_MODE_CACHE=$(flag hwc_mode_cache) || return 1
 HWC_CLIENT_HDR=$(flag hwc_client_hdr10) || return 1
 HWUI_RGB10=$(flag hwui_rgb10) || return 1
 GRALLOC_HDR_LEASE=$(flag gralloc_hdr_lease) || return 1
 DELAY_NOTIFY=$(flag codec2_delay_notify) || return 1
 HEVC_NATIVE=$(flag hevc_native_surface) || return 1
 HEVC_EOS=$(flag hevc_eos_pending) || return 1
 P010_PAIRS=$(flag p010_pairs) || return 1
 P010_POOL=$(flag p010_copy_pool) || return 1
 HDMI_GUARD=$(flag hdmi_audio_guard) || return 1
 DISPLAY_QMS=$(flag display_qms 2>/dev/null) || DISPLAY_QMS=0
 DISPLAY_POLICY=$(flag display_policy 2>/dev/null) || DISPLAY_POLICY=0
 DISPLAY_SESSION=$(flag display_session 2>/dev/null) || DISPLAY_SESSION=0
 DISPLAY_DDC=$(flag display_ddc 2>/dev/null) || DISPLAY_DDC=0
 [ -s "$MODROOT/components/ddc/rk_decode_ddc.ko" ] || DISPLAY_DDC=0
 [ -s "$MODROOT/components/display_policy/preflight" ] &&
 [ -s "$MODROOT/components/display_policy/variants/services.jar" ] &&
 [ -s "$MODROOT/components/display_policy/oat.sha256" ] &&
 [ -s "$MODROOT/components/display_policy/oat-inputs.sha256" ] || DISPLAY_POLICY=0
 [ "$HDR_STATIC" = 1 ] && [ -s "$MODROOT/components/qms/profile" ] &&
 [ -s "$MODROOT/components/qms/rk_decode_qms.ko" ] && [ -x "$MODROOT/components/qms/boot-stage64" ] || DISPLAY_QMS=0
 grep -qw 'initcall_blacklist=kbase_platform_driver_init' /proc/cmdline || DISPLAY_QMS=0
 [ -f /data/adb/modules/rk3588_mali_global/disable ] && DISPLAY_QMS=0
 [ -s "$MODROOT/components/hdmi_event/preflight" ] && [ -s "$MODROOT/components/hdmi_event/target/libhevt.so" ] || HDMI_GUARD=0
 [ "$MPP" = 1 ] || AV1_GRAIN=0
 BUFFER=$(flag codec2_buffer) || return 1
 P010_FAST=$(flag p010_fast) || return 1
 [ "$MPP" = 1 ] && [ "$RGA" = 1 ] || C2=0
 [ "$C2" = 1 ] || SURFACE=0
 if [ "$C2" != 1 ]; then SDK=0; SOFT=0; HDR_STATIC=0; HDR_PLUS=0; fi
 [ "$HDR_STATIC" = 1 ] || HWC_REUSE=0
 [ "$HDR_STATIC" = 1 ] || HWC_SCALE=0
 [ "$HDR_STATIC" = 1 ] || HWC_MODE_CACHE=0
 [ "$HDR_STATIC" = 1 ] || HWUI_RGB10=0
 [ "$HDR_STATIC" = 1 ] && [ "$HWUI_RGB10" = 1 ] || HWC_CLIENT_HDR=0
 # Composer 2.1 does not carry the later client-target format request. Do not
 # enable HDR before a complete physical HDR target path has passed validation.
 [ -s "$MODROOT/components/hdr/sf-target10-preflight" ] && [ -s "$MODROOT/components/hdr/target/libsfhdr.so" ] || HWC_CLIENT_HDR=0
 [ -s "$MODROOT/components/hdr/alloc-target10-preflight" ] && [ -s "$MODROOT/components/hdr/target/libga10.so" ] || HWC_CLIENT_HDR=0
 # Uses the already paired SF loader; no second SF/boot replacement.
 [ "$HWC_CLIENT_HDR" = 1 ] &&
 [ -s "$MODROOT/components/refresh_session/system/lib64/librkses.so" ] &&
 [ -s "$MODROOT/components/refresh_session/system/lib64/libsfhdr.so" ] || DISPLAY_SESSION=0
 # RGB10 scanout on this firmware requires the original AFBC FB policy.
 case "$(getprop vendor.gralloc.no_afbc_for_fb_target_layer)" in ''|0) ;; *) HWC_CLIENT_HDR=0 ;; esac
 # This private candidate requires the existing ramdisk graphics barrier for
 # allocator/SF first execution. Never silently perform a late allocator swap.
 grep -qw 'initcall_blacklist=kbase_platform_driver_init' /proc/cmdline || HWC_CLIENT_HDR=0
 [ "$SDK" = 1 ] || BUFFER=0
 [ "$SDK" = 1 ] || SDK_NEXT=0
 [ "$SDK_NEXT" = 1 ] && [ "$SURFACE" = 1 ] || AV1_NATIVE=0
 [ "$SDK_NEXT" = 1 ] || LIFECYCLE=0
 [ "$SDK_NEXT" = 1 ] && [ "$SURFACE" = 1 ] && [ "$LIFECYCLE" = 1 ] || NATIVE_BP=0
 [ "$SDK_NEXT" = 1 ] || DELAY_NOTIFY=0
 [ "$SDK_NEXT" = 1 ] || HEVC_EOS=0
 [ "$SDK_NEXT" = 1 ] && [ "$BUFFER" = 1 ] || P010_POOL=0
 [ -s "$MODROOT/components/codec2_pool/system/lib64/libcp10.so" ] || P010_POOL=0
 [ "$SDK_NEXT" = 1 ] && [ "$P010_FAST" = 1 ] || P010_PAIRS=0
 [ "$SDK_NEXT" = 1 ] && [ "$HDR_STATIC" = 1 ] || GRALLOC_HDR_LEASE=0
 [ "$SDK_NEXT" = 1 ] && [ "$HDR_STATIC" = 1 ] && [ "$SURFACE" = 1 ] || HEVC_NATIVE=0
 [ "$AV1_NATIVE" = 1 ] || AV1_AFBC=0
 [ "$AV1_AFBC" = 1 ] || AV1_TEXTURE_AFBC=0
 [ "$C2" = 1 ] || P010_FAST=0
 [ "$HDR_STATIC" = 1 ] || DISPLAY_QMS=0
 [ "$HWC_CLIENT_HDR" = 1 ] || DISPLAY_SESSION=0
 [ "$SDK" != 1 ] || [ -s "$MODROOT/components/codec2_sdk/system/vendor/lib/hw/libcodec2_rx_component.so" ] || return 1
 [ "$SDK_NEXT" != 1 ] || [ -s "$MODROOT/components/codec2_sdk_next/system/vendor/lib/hw/libcodec2_rx_component.so" ] || return 1
}
selected() {
 case "$1" in mpp) [ "$MPP" = 1 ] ;; rga) [ "$RGA" = 1 ] ;; codec2) [ "$C2" = 1 ] ;; libyuv) [ "$YUV" = 1 ] ;; software) [ "$SOFT" = 1 ] ;; hdr) [ "$HDR_STATIC" = 1 ] ;; *) return 1 ;; esac
}
