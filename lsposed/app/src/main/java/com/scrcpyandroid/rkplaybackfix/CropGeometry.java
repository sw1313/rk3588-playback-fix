package com.scrcpyandroid.rkplaybackfix;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Decoder crop metadata; independent of codec name and nominal resolution. */
final class CropGeometry {
    private static final Pattern CROP = Pattern.compile("Output crop parameters top=(\\d+) bottom=(\\d+) left=(\\d+) right=(\\d+), resulting dimensions width=(\\d+) height=(\\d+)");
    private static final Pattern BUFFER = Pattern.compile("Texture dimensions changed to (\\d+)x(\\d+)");
    int top, bottom, left, right, width, height, bufferWidth, bufferHeight;
    boolean cropKnown, bufferKnown;

    boolean observe(String prefix, String message) {
        if (prefix == null || message == null) return false;
        Matcher match;
        try {
            if (prefix.equals("ffmpeg/video") && message.contains("_mediacodec:")) {
                match = CROP.matcher(message);
                if (!match.find()) return false;
                top = Integer.parseInt(match.group(1)); bottom = Integer.parseInt(match.group(2));
                left = Integer.parseInt(match.group(3)); right = Integer.parseInt(match.group(4));
                width = Integer.parseInt(match.group(5)); height = Integer.parseInt(match.group(6));
                cropKnown = true;
                return true;
            }
            if (prefix.equals("vo/gpu/aimagereader") || prefix.equals("vo/gpu-next/aimagereader")) {
                match = BUFFER.matcher(message);
                if (!match.find()) return false;
                bufferWidth = Integer.parseInt(match.group(1)); bufferHeight = Integer.parseInt(match.group(2));
                bufferKnown = true;
                return true;
            }
        } catch (NumberFormatException ignored) { cropKnown = bufferKnown = false; }
        return false;
    }

    boolean matches(Integer videoWidth, Integer videoHeight, String hwdec) {
        return cropKnown && bufferKnown && "mediacodec".equals(hwdec)
            && videoWidth != null && videoHeight != null && videoWidth == width && videoHeight == height
            && width > 0 && height > 0 && bufferWidth <= 16384 && bufferHeight <= 16384
            && (top > 0 || left > 0) && top < height && left < width
            && bottom >= top && right >= left && bottom < bufferHeight && right < bufferWidth
            && (long) bottom - top + 1 == height && (long) right - left + 1 == width;
    }

    String key() { return width + "x" + height + "-" + bufferWidth + "x" + bufferHeight + "-" + left + "-" + top; }

    String shader() {
        // gpu can preprocess into the active-size texture; libplacebo may retain
        // the padded buffer. Test both pairs, rather than hard-coding 1080 rows.
        return "//!HOOK RGB\n//!BIND HOOKED\n//!DESC RK3588 reported crop " + key() + "\n"
            + "//!WHEN HOOKED.w " + width + " - ! HOOKED.h " + height + " - ! * "
            + "HOOKED.w " + bufferWidth + " - ! HOOKED.h " + bufferHeight + " - ! * +\n"
            + "vec4 hook() {\n"
            + "    bool activeSize = abs(HOOKED_size.x - " + width + ".0) < 0.5 && abs(HOOKED_size.y - " + height + ".0) < 0.5;\n"
            + "    bool padded = abs(HOOKED_size.x - " + bufferWidth + ".0) < 0.5 && abs(HOOKED_size.y - " + bufferHeight + ".0) < 0.5;\n"
            + "    return HOOKED_tex(HOOKED_pos + ((activeSize || padded) ? vec2(" + left + ".0, " + top + ".0) * HOOKED_pt : vec2(0.0)));\n}\n";
    }
}
