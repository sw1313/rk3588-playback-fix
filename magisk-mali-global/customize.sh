#!/system/bin/sh
[ "$BOOTMODE" = true ] && [ "$API" = 32 ] && [ "$ARCH" = arm64 ] || abort "Requires verified API32 ARM64 firmware"
[ "$(getprop ro.board.platform)" = rk3588 ] && [ "$(uname -r)" = 5.10.66 ] || abort "Unsupported platform/kernel"
cd "$MODPATH" || abort "Module directory unavailable"
sha256sum -c checksums.sha256 >/dev/null 2>&1 || abort "Package integrity differs"
sh "$MODPATH/platform-check.sh" >/dev/null 2>&1 || abort "Original graphics dependencies or exact Decode SF pair differ"
STATE=/data/adb/rk3588-mali-global
mkdir -p "$STATE/cache" "$STATE/original/lib/egl" "$STATE/original/lib64/egl" || abort "Cannot create private rollback/cache state"
chmod 700 "$STATE"
sha() { result=$(sha256sum "$1") || return 1; echo "${result%% *}"; }
while read -r expected original; do
 rel=${original#/vendor/}
 saved="$STATE/original/$rel"
 if [ "$(sha "$original")" = "$expected" ]; then
  [ -f "$saved" ] || cp "$original" "$saved" || abort "Cannot retain original GPU"
 else
  active=/data/adb/modules/rk3588_mali_global
  [ -f "$active/active.sha256" ] && sha256sum -c "$active/active.sha256" >/dev/null 2>&1 || abort "Unrecognized active GPU"
 fi
 [ "$(sha "$saved")" = "$expected" ] || abort "Original GPU identity differs"
done < contracts/original-mali.sha256
while read -r abi bytes expected url; do
 cache="$STATE/cache/$expected.so"
 if [ ! -f "$cache" ] || [ "$(sha "$cache")" != "$expected" ]; then
  ui_print "Downloading pinned upstream native Mali ($abi); TLS and SHA256 verification enabled."
  CLASSPATH="$MODPATH/bin/fetch-driver.jar" timeout 150 /system/bin/app_process /system/bin FetchDriver "$url" "$cache" "$expected" "$bytes" || abort "Pinned driver download failed"
 fi
 [ "$(sha "$cache")" = "$expected" ] || abort "Downloaded driver hash differs"
 case "$abi" in arm) dir=lib;; arm64) dir=lib64;; *) abort "Unexpected ABI";; esac
 mkdir -p "$MODPATH/payload/$dir/egl"
 cp "$cache" "$MODPATH/payload/$dir/egl/libGLES_mali.so" || abort "Cannot stage native Mali"
done < contracts/driver-sources.txt
read -r bytes expected url < contracts/csf-source.txt
cache="$STATE/cache/$expected.bin"
if [ ! -f "$cache" ] || [ "$(sha "$cache")" != "$expected" ]; then
 ui_print "Downloading pinned CSF firmware; kernel upgrade remains off by default."
 CLASSPATH="$MODPATH/bin/fetch-driver.jar" timeout 150 /system/bin/app_process /system/bin FetchDriver "$url" "$cache" "$expected" "$bytes" || abort "Pinned CSF download failed"
fi
[ "$(sha "$cache")" = "$expected" ] || abort "CSF hash differs"
sha256sum -c kernel.sha256 >/dev/null 2>&1 || abort "Kernel upgrade payload differs"
[ -f "$STATE/kernel-mode" ] || printf 'stock\n' > "$STATE/kernel-mode"
[ -f "$STATE/dvfs-mode" ] || printf 'off\n' > "$STATE/dvfs-mode"
[ -f "$STATE/cold-mode" ] || printf 'off\n' > "$STATE/cold-mode"
[ -f "$STATE/fence-mode" ] || printf 'off\n' > "$STATE/fence-mode"
sha256sum -c payload.sha256 >/dev/null 2>&1 || abort "Complete GPU payload differs"
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/boot-stage.sh" 0 0 0755
for f in mode-control.sh action.sh assemble.sh post-fs-data.sh service.sh uninstall.sh webui.sh kernel-state.sh kernel-boot.sh kernel-transition.sh kernel-verify.sh dvfs-state.sh dvfs-boot.sh cold-state.sh cold-verify.sh bin/cold_boot64 bin/graphics_probe32 bin/graphics_probe64 bin/kernel_controller bin/kernel_health bin/kernel_scan; do set_perm "$MODPATH/$f" 0 0 0755; done
for abi in 32 64; do
 [ "$abi" = 64 ] && dir=lib64 || dir=lib
 prefix="$MODPATH/payload/$dir:/vendor/$dir:/system/$dir:/apex/com.android.vndk.v32/$dir"
 for api in gl vk; do
  timeout 20 /system/bin/env LD_LIBRARY_PATH="$prefix" "$MODPATH/bin/graphics_probe$abi" "$MODPATH/payload/$dir/egl/libGLES_mali.so" "$api" || abort "Native GPU render preflight failed"
 done
done
legacy=/data/adb/modules/rk3588_mali_native
if [ -f "$legacy/mode-control.sh" ]; then
 # The old controller may return 1 when deleting an already absent property.
 # Verify its resulting state rather than interpreting that as failed cleanup.
 sh "$legacy/mode-control.sh" off
 [ -z "$(getprop ro.gfx.driver.0)" ] && [ ! -s /data/adb/rk3588-mali-native/owned-apps ] || abort "Cannot restore legacy app selection"
 touch "$legacy/disable"
fi
[ -z "$(getprop ro.gfx.driver.0)" ] && [ -z "$(getprop ro.gfx.driver.1)" ] || abort "An unrelated per-app GPU override is configured"
[ -f "$STATE/mode" ] || echo on > "$STATE/mode"
sh "$MODPATH/assemble.sh" || abort "Cannot assemble global GPU overlay"
# Installation is not a boot attempt; the first real boot sets this marker.
rm -f "$STATE/boot-pending"
ui_print "Global g18p0 prepared. Cold binding is OFF by default; requires a separately prepared private exact-root boot profile. Selecting cold binding writes and verifies boot. Reboot applies settings."
