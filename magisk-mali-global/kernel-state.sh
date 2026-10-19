#!/system/bin/sh
STATE=/data/adb/rk3588-mali-global
RUN=$STATE/kernel-run
read_mode(){ v=$(cat "$STATE/mode" 2>/dev/null);case "$v" in on|off) echo "$v";;*) echo on;;esac; }
read_kernel(){ v=$(cat "$STATE/kernel-mode" 2>/dev/null);case "$v" in stock|next) echo "$v";;*) echo stock;;esac; }
atomic_state(){
 case "$1" in mode|kernel-mode|kernel-pending|kernel-boot-plan|kernel-recovery|kernel-active-boot|dvfs-mode|cold-mode|cold-plan|fence-mode) ;;*) return 2;;esac
 case "$2" in ''|*[!a-zA-Z0-9_-]*) return 2;;esac
 [ ! -L "$STATE" ] || return 2
 mkdir -p "$STATE";chmod 0700 "$STATE"
 tmp="$STATE/$1.tmp.$$"
 (umask 077;set -C;printf '%s\n' "$2" > "$tmp") || return 1
 mv -f "$tmp" "$STATE/$1" || return 1
 sync
}
kernel_supported(){
 [ "$(getprop ro.board.platform)" = rk3588 ] && [ "$(uname -r)" = 5.10.66 ] || return 1
 [ "$(cat /sys/module/bifrost_kbase/version 2>/dev/null)" = 'g12p0-01eac0 (UK version 1.11)' ] || return 1
 [ "$(sha256sum /vendor/etc/firmware/mali_csffw.bin | cut -d ' ' -f 1)" = 267f958f7e63065b8cdb6c2119526f92f0860b2376ad4b00bcc121727d6c4ed5 ]
}
kernel_active(){
 [ "$(readlink /sys/bus/platform/devices/fb000000.gpu/driver 2>/dev/null)" = ../../../bus/platform/drivers/rk_next_mali ] || { echo stock;return; }
 [ "$(cat /sys/module/rk_mali_next/parameters/trial_fw_size 2>/dev/null)" = 266240 ] && [ "$(cat /sys/module/rk_mali_next/parameters/trial_fw_hi 2>/dev/null)" = 2619577260 ] && [ "$(cat /sys/module/rk_mali_next/parameters/trial_fw_lo 2>/dev/null)" = 3266300732 ] && { echo next;return; }
 echo checking
}
kernel_boot_plan(){
 boot=$(cat /proc/sys/kernel/random/boot_id) || return 1
 previous=$(cat "$STATE/kernel-pending" 2>/dev/null)
 if [ -n "$previous" ] && [ "$previous" != "$boot" ];then
  atomic_state kernel-mode stock || return 1
  atomic_state kernel-recovery interrupted_previous_boot || return 1
  rm -f "$STATE/kernel-pending"
 fi
 if [ "$(read_mode)" = on ] && [ "$(read_kernel)" = next ] && kernel_supported;then
  atomic_state kernel-pending "$boot" || return 1
  atomic_state kernel-boot-plan next
 else atomic_state kernel-boot-plan stock;fi
}
