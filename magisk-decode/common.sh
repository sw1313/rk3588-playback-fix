#!/system/bin/sh
# Caller sets MODROOT. Configuration is data, never shell code.
flag() {
 value=$(sed -n "s/^$1=//p" "$MODROOT/config" 2>/dev/null)
 case "$value" in 0|1) echo "$value" ;; *) return 1 ;; esac
}
resolve() {
 MPP=$(flag mpp) && RGA=$(flag rga) && C2=$(flag codec2) && YUV=$(flag libyuv) && SOFT=$(flag software) || return 1
 HDR_STATIC=$(flag hdr_static) && HDR_PLUS=$(flag hdr10plus) || return 1
 SDK=$(flag codec2_sdk) || return 1
 BUFFER=$(flag codec2_buffer) || return 1
 [ "$MPP" = 1 ] && [ "$RGA" = 1 ] || C2=0
 if [ "$C2" != 1 ]; then SDK=0; SOFT=0; HDR_STATIC=0; HDR_PLUS=0; fi
 [ "$SDK" = 1 ] || BUFFER=0
 [ "$SDK" != 1 ] || [ -s "$MODROOT/components/codec2_sdk/system/vendor/lib/hw/libcodec2_rx_component.so" ] || return 1
}
selected() {
 case "$1" in mpp) [ "$MPP" = 1 ] ;; rga) [ "$RGA" = 1 ] ;; codec2) [ "$C2" = 1 ] ;; libyuv) [ "$YUV" = 1 ] ;; software) [ "$SOFT" = 1 ] ;; hdr) [ "$HDR_STATIC" = 1 ] ;; *) return 1 ;; esac
}
