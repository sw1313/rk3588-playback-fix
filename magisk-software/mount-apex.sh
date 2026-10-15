#!/system/bin/sh
# Invoked by the unified late-start service, after APEX activation.
set -eu
C=${0%/*}
P=/apex/com.android.media.swcodec/lib64
cd "$C"
sha256sum -c contracts/platform.sha256 >/dev/null
sha256sum -c contracts/original-apex.sha256 >/dev/null
for name in libcodec2_soft_av1dec_gav1.so libcodec2_soft_vp8dec.so libcodec2_soft_vp9dec.so; do
 grep "  apex/lib64/$name$" checksums.sha256 | sha256sum -c - >/dev/null
done
stop media.swcodec
mounted=''
success=0
cleanup() {
 if [ "$success" != 1 ]; then
  for name in $mounted; do umount "$P/$name" || true; done
  start media.swcodec || true
 fi
}
trap cleanup EXIT INT TERM
for name in libcodec2_soft_av1dec_gav1.so libcodec2_soft_vp8dec.so libcodec2_soft_vp9dec.so; do
 mount -o bind "$C/apex/lib64/$name" "$P/$name"
 mounted="$name $mounted"
done
start media.swcodec
attempt=0
while [ "$(getprop init.svc.media.swcodec)" != running ] && [ "$attempt" -lt 30 ]; do sleep 0.1; attempt=$((attempt+1)); done
[ "$(getprop init.svc.media.swcodec)" = running ]
timeout 15 /system/bin/env LD_LIBRARY_PATH="$P:/system/lib64" "$C/bin/factory_probe64" "$P"
success=1
echo "PASS APEX software decoders mounted; firmware APEX and shared encoder backend retained."
