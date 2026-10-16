#!/system/bin/sh
set -eu
MODDIR=${0%/*}
. "$MODDIR/kernel-state.sh"
fail(){ atomic_state kernel-mode stock || true;atomic_state kernel-recovery "$1" || true;resetprop -n vendor.rkmalikernel.ready failed;exit 1; }
if [ "$(cat "$STATE/kernel-boot-plan" 2>/dev/null)" != next ] || [ "$(read_mode)" != on ];then resetprop -n vendor.rkmalikernel.ready stock;exit 0;fi
boot=$(cat /proc/sys/kernel/random/boot_id)
[ "$(cat "$STATE/kernel-pending" 2>/dev/null)" = "$boot" ] || fail missing_boot_latch
kernel_supported || fail unsupported_kernel
[ "$(getprop vendor.rkmaliglobal.ready)" = active ] || fail userspace_unverified
cd "$MODDIR"
sha256sum -c kernel.sha256 >/dev/null 2>&1 || fail kernel_payload_mismatch
[ ! -L "$RUN" ] || fail invalid_runtime_directory
mkdir -p "$RUN" "$STATE/kernel-previous";chmod 0700 "$RUN" "$STATE/kernel-previous"
for name in controller health health.logcat health.kmsg health.restored resource-handover-boot.log phase hot-transition.log verify.log original-firmware-path result active-controller.lock expected-firmware-path actual-firmware-path trial-boot;do
 if [ -f "$RUN/$name" ];then mv -f "$RUN/$name" "$STATE/kernel-previous/$name";fi
done
umask 077
: > "$RUN/resource-handover-boot.log"
sync
FW=/data/local/tmp/rk3588-mali-firmware
[ ! -L "$FW" ] || fail invalid_firmware_directory
mkdir -p "$FW";chown 0:0 "$FW";chmod 0755 "$FW"
read -r bytes expected url < "$MODDIR/contracts/csf-source.txt"
cache="$STATE/cache/$expected.bin"
[ "$(sha256sum "$cache" | cut -d ' ' -f 1)" = "$expected" ] || fail csf_cache_mismatch
cp "$cache" "$FW/rk_next_csffw.bin";chown 0:0 "$FW/rk_next_csffw.bin";chmod 0644 "$FW/rk_next_csffw.bin"
resetprop -n vendor.rkmalikernel.ready checking
"$MODDIR/bin/kernel_health" watch "$RUN/health" "$RUN/health.restored" 90 > "$RUN/health-launch.log" 2>&1 &
watcher=$!
if "$MODDIR/bin/kernel_controller" composer-kmd-trial "$RUN/controller" "$RUN/health";then
 if grep -q 'complete qualified=1' "$RUN/controller" && [ "$(cat "$RUN/result" 2>/dev/null)" = AUTOMATED_PASS ];then
  atomic_state kernel-active-boot "$boot"
  rm -f "$STATE/kernel-pending" "$STATE/kernel-recovery";sync
  resetprop -n vendor.rkmalikernel.ready active
  exit 0
 fi
fi
kill "$watcher" 2>/dev/null || true
fail kernel_trial_failed
