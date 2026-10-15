#!/system/bin/sh
[ "$BOOTMODE" = true ] && [ "$API" = 32 ] && [ "$ARCH" = arm64 ] || abort "Requires verified running API32 ARM64 firmware."
[ "$(getprop ro.board.platform)" = rk3588 ] || abort "Requires RK3588."
cd "$MODPATH" || abort "Software component unavailable."
sha256sum -c checksums.sha256 >/dev/null 2>&1 || abort "Software payload verification failed."
sha256sum -c contracts/platform.sha256 >/dev/null 2>&1 || abort "Software Codec2 dependency ABI differs."
while read -r expected target; do
 observed=$(sha256sum "$target"); observed=${observed%% *}
 candidate=$(sha256sum "$MODPATH/apex/lib64/${target##*/}"); candidate=${candidate%% *}
 [ "$observed" = "$expected" ] || [ "$observed" = "$candidate" ] || abort "Unknown APEX decoder: $target"
done < contracts/original-apex.sha256
xml=/vendor/etc/media_codecs_google_c2.xml
observed=$(sha256sum "$xml"); observed=${observed%% *}
mkdir -p system/vendor/etc
case "$observed" in
 c3ec15d733f8ba8128239d548d4c73487f0e2bff02d1bd030ca837bb1dc309ce)
  sed '/<MediaCodec name="c2.android.av1.decoder"/s/enabled="false"/enabled="true"/' "$xml" > system/vendor/etc/media_codecs_google_c2.xml ;;
 ea14149b401d6f2c3404acb02f8244260a14b1179659a905e49ece338e6224bf)
  cp "$xml" system/vendor/etc/media_codecs_google_c2.xml ;;
 *) abort "Unknown software codec XML baseline." ;;
esac
echo 'ea14149b401d6f2c3404acb02f8244260a14b1179659a905e49ece338e6224bf  system/vendor/etc/media_codecs_google_c2.xml' | sha256sum -c - >/dev/null || abort "Generated codec XML differs."
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/bin/factory_probe64" 0 0 0755
timeout 15 /system/bin/env LD_LIBRARY_PATH=/apex/com.android.media.swcodec/lib64:/system/lib64 "$MODPATH/bin/factory_probe64" "$MODPATH/apex/lib64" || abort "Software factory preflight failed."
ui_print "Software decoders verified. Full P010 requires the selected Codec2 compatibility component."
