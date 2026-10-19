#!/system/bin/sh
# Private one-shot observation before graphics; separate read-only module only.
# Never selects QMS, registers an endpoint, writes boot, disables another module,
# restarts a service or requests reboot. Diagnostic failure preserves native boot.
MODROOT=${0%/*}
[ "$MODROOT" = /data/adb/modules/rk3588_decode_stack ] || exit 0
c="$MODROOT/components/qms-check"
[ -f "$c/armed" ] && [ ! -L "$c/armed" ] || exit 0
mv "$c/armed" "$c/consumed" || exit 0
state=/data/adb/rk3588-decode-qms-check
[ ! -L "$state" ] || exit 0
umask 077
mkdir -p "$state";chmod 0700 "$state"
boot=$(cat /proc/sys/kernel/random/boot_id)
{
 echo "BOOT $boot; diagnostic-only; no display commit or QMS enable"
 for service in vendor.gralloc-4-0 vendor.hwcomposer-2-1 surfaceflinger;do
  pid=$(getprop "init.svc_debug_pid.$service")
  echo "SERVICE $service pid=$pid"
  case "$pid" in ''|*[!0-9]*) ;; *) grep -e '^State:' -e '^PPid:' "/proc/$pid/status" ;; esac
 done
 if ! (cd "$c" && sha256sum -c checksums.sha256);then echo BAD_CHECKSUM;exit 0;fi
 if [ -e /sys/module/rk_qms_owner_probe ] || [ -e /sys/module/rk_decode_qms ];then echo REFUSE_EXISTING_OWNER;exit 0;fi
 if timeout 8 insmod "$c/rk_qms_owner_probe.ko";then
  for p in /sys/module/rk_qms_owner_probe/parameters/owner*;do echo "${p##*/}=$(cat "$p")";done
  if rmmod rk_qms_owner_probe;then echo READ_ONLY_PROBE_UNLOADED;else echo PROBE_UNLOAD_FAILED;fi
 else echo READ_ONLY_LOAD_REFUSED;fi
 echo END_NO_BOOT_OR_DISPLAY_SELECTION_CHANGE
} > "$state/$boot.txt" 2>&1
exit 0
