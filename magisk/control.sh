#!/system/bin/sh
STATE=/data/adb/rk3588-frameguard
ORIGINAL=$STATE/original.conf
SETTINGS=/system/bin/settings

supported() {
    [ "$(getprop ro.board.platform)" = rk3588 ] &&
    [ "$(getprop ro.build.version.sdk)" = 32 ]
}

save_original() {
    mkdir -p "$STATE" || return 1
    chmod 700 "$STATE"
    [ -f "$ORIGINAL" ] && return 0
    umask 077
    {
        echo "secure|match_content_frame_rate|$($SETTINGS --user 0 get secure match_content_frame_rate)"
        echo "system|min_refresh_rate|$($SETTINGS --user 0 get system min_refresh_rate)"
        echo "system|peak_refresh_rate|$($SETTINGS --user 0 get system peak_refresh_rate)"
    } > "$ORIGINAL.tmp" || return 1
    mv "$ORIGINAL.tmp" "$ORIGINAL"
}

restore_original() {
    [ -f "$ORIGINAL" ] || return 0
    while IFS='|' read -r ns key original; do
        case "$ns/$key" in
            secure/match_content_frame_rate) owned=0 ;;
            system/min_refresh_rate|system/peak_refresh_rate) owned=60 ;;
            *) echo "Ignoring unknown backup key"; continue ;;
        esac
        current=$($SETTINGS --user 0 get "$ns" "$key")
        # A later manual change belongs to the user, not this module.
        [ "$current" = "$owned" ] || { echo "Keeping manual change: $ns/$key"; continue; }
        case "$original" in
            null) $SETTINGS --user 0 delete "$ns" "$key" >/dev/null || return 1 ;;
            ''|*[!0-9.]*) echo "Invalid saved value: $ns/$key"; return 1 ;;
            *) $SETTINGS --user 0 put "$ns" "$key" "$original" || return 1 ;;
        esac
    done < "$ORIGINAL"
    echo "Original refresh-rate settings restored."
}

apply_guard() {
    supported || { echo "Unsupported device: requires RK3588 Android 12."; return 1; }
    save_original || return 1
    $SETTINGS --user 0 put system peak_refresh_rate 60 &&
    $SETTINGS --user 0 put system min_refresh_rate 60 &&
    $SETTINGS --user 0 put secure match_content_frame_rate 0 || {
        restore_original
        return 1
    }
    [ "$($SETTINGS --user 0 get secure match_content_frame_rate)" = 0 ] &&
    [ "$($SETTINGS --user 0 get system min_refresh_rate)" = 60 ] &&
    [ "$($SETTINGS --user 0 get system peak_refresh_rate)" = 60 ] || return 1
    echo "Frame Guard applied: 60Hz, automatic content mode switching off."
}

case "$1" in
    apply) apply_guard ;;
    restore) restore_original ;;
    status)
        echo "match=$($SETTINGS --user 0 get secure match_content_frame_rate)"
        echo "min=$($SETTINGS --user 0 get system min_refresh_rate)"
        echo "peak=$($SETTINGS --user 0 get system peak_refresh_rate)" ;;
esac
