#!/system/bin/sh
resetprop --delete debug.rkdecode.refresh_session
resetprop --delete debug.rkdecode.session_trace
resetprop --delete vendor.rkdecode.refresh_session
resetprop -p --delete persist.vendor.rkdisplay.hdr_force_disable
resetprop --delete debug.rkcodec2.av1_afbc
resetprop --delete debug.rkcodec2.av1_texture_afbc
resetprop --delete debug.rkcodec2.lifecycle_fix
resetprop --delete debug.rkcodec2.av1_grain
resetprop --delete debug.rkcodec2.av1_direct
resetprop --delete debug.rkcodec2.surface_upgrade
resetprop --delete debug.rkcodec2.native_surface_upgrade
resetprop --delete debug.rkcodec2.native_backpressure
resetprop --delete debug.rkcodec2.buffer_upgrade
resetprop --delete debug.rkcodec2.p010_fast
resetprop --delete debug.rkcodec2.hdr_static_buffer
resetprop --delete debug.rkcodec2.hdr10plus
resetprop --delete debug.rkcodec2.delay_notify
resetprop --delete debug.rkcodec2.hevc_native_surface
resetprop --delete debug.rkcodec2.hevc_eos_pending
resetprop --delete debug.rkcodec2.p010_pairs
resetprop --delete debug.rkcodec2.p010_pool
resetprop --delete debug.rkhdr.client_hdr10
resetprop --delete debug.rkhdr.hdmi_audio_guard
resetprop --delete vendor.rkhdr.hdmi_event
resetprop --delete vendor.rkhdr.sf_target10
resetprop --delete vendor.rkhdr.sf_target10_error
resetprop --delete vendor.rkhdr.alloc10
resetprop --delete debug.rkcodec2.gralloc_hdr_lease
resetprop --delete debug.rkhdr.metadata_reuse
resetprop --delete debug.rkhdr.static
echo "Unified decode stack removed. Reboot restores firmware libraries; previous standalone modules remain disabled."
