#!/system/bin/sh
# kernel-state.sh must be sourced first. This backend never performs a live swap.
read_cold(){ v=$(cat "$STATE/cold-mode" 2>/dev/null);case "$v" in on) echo on;;*) echo off;;esac; }
cold_booting(){ grep -qw 'initcall_blacklist=kbase_platform_driver_init' /proc/cmdline; }
cold_prepared(){ [ "$(magisk -V)" = 30700 ] && [ -x "$MODDIR/bin/cold_boot64" ] && "$MODDIR/bin/cold_boot64" verify >/dev/null 2>&1; }
cold_restore(){
 # Backup lives outside the module, and must survive removal/manager disable.
 [ ! -f "$STATE/boot/profile" ] || "$MODDIR/bin/cold_boot64" restore
}
cold_receipt(){
 boot=$(cat /proc/sys/kernel/random/boot_id) || return 1
 profile=$(sed -n 's/^profile_id //p' "$STATE/boot/profile")
 [ "$(cat "$STATE/cold-receipt" 2>/dev/null)" = "$boot $profile released" ]
}
cold_plan(){
 atomic_state kernel-boot-plan stock || return 1
 [ "$(read_cold)" = on ] && [ "$(read_mode)" = on ] && [ "$(read_kernel)" = next ] || return 1
 kernel_supported && cold_prepared || return 1
 atomic_state kernel-boot-plan cold
}
