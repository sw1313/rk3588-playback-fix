#!/system/bin/sh
# Fixed UI bundles. No presets are applied when opening/updating the UI.
bundle_members() {
 case "$1" in
 core) echo 'mpp rga codec2 libyuv software' ;;
 native) echo 'codec2_surface codec2_native_backpressure codec2_lifecycle codec2_delay_notify hevc_eos_pending codec2_buffer' ;;
 av1) echo 'av1_native av1_grain av1_afbc av1_texture_afbc' ;;
 hdr) echo 'hdr_static hdr10plus hwui_rgb10 hwc_client_hdr10 gralloc_hdr_lease hevc_native_surface p010_fast p010_pairs p010_copy_pool' ;;
 display) echo 'hwc_mode_cache hwc_metadata_reuse hwc_scaled_overlay hdmi_audio_guard display_policy display_session' ;;
 *) return 2 ;;
 esac
}
