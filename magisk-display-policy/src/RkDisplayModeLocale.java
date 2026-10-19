// SPDX-License-Identifier: MIT
package com.android.server.rkdisplay;

import android.view.Display;
import android.util.SparseArray;
import android.util.Log;
import com.android.server.display.DisplayModeDirector;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** System-server adapter for the exact API32 Rockchip mode-setting path.
 * No player hooks, global locale changes, fabricated HWC results or counters.
 * The generated services overlay disappears on component disable + reboot.
 */
public final class RkDisplayModeLocale {
    private static final String TAG = "RKDisplayPolicy";
    private static final Pattern MODE = Pattern.compile("^([0-9]+)x([0-9]+)@([0-9]+(?:\\.[0-9]+)?)-.+$");
    private static volatile DisplayModeDirector director;
    private static String lastReported = "";
    private static final String HDR = "vendor.hwc.hdr_force_disable";
    private static final String HDR_SAVED = "persist.vendor.rkdisplay.hdr_force_disable";

    public static String format(String pattern, Object[] values) {
        return String.format(Locale.ROOT, pattern, values);
    }

    private static Field field(Class<?> type, String name) throws Exception {
        Field f = type.getDeclaredField(name); f.setAccessible(true); return f;
    }

    private static String property(String key) throws Exception {
        return (String) Class.forName("android.os.SystemProperties")
            .getMethod("get", String.class, String.class).invoke(null, key, "");
    }

    /** This firmware writes HDMI selection to the connector-specific key but
     * getCurMode still checks resolution.main for the Auto sentinel. Correct
     * that property read only on primary HDMI; keep the factory mode query and
     * formatting, so explicit choices still report the actual hardware mode. */
    public static String selectionProperty(String key, String fallback) {
        String original;
        try {
            Method get = Class.forName("android.os.SystemProperties").getMethod("get", String.class, String.class);
            original = (String) get.invoke(null, key, fallback);
            if (!"persist.vendor.resolution.main".equals(key)) return original;
            try {
                int iface = (Integer) nativeState("nativeGetBuiltIn", new Class<?>[]{int.class}, 0);
                if (iface != 11) return original; // Never apply HDMI policy to DP/TV.
                String selected = property("persist.vendor.resolution.HDMI-A-0");
                return "Auto".equals(selected) || MODE.matcher(selected).matches() ? selected : original;
            } catch (Exception e) {
                Log.e(TAG, "Selection key adapter declined; original property retained", e);
                return original;
            }
        } catch (Exception e) { throw new IllegalStateException("Display selection read failed", e); }
    }

    private static Object nativeState(String method, Class<?>[] types, Object... args) throws Exception {
        Method m = Class.forName("com.android.server.rkdisplay.RkDisplayModes").getDeclaredMethod(method, types);
        m.setAccessible(true); return m.invoke(null, args);
    }

    /** The factory HWC defaults an absent disable property to 0, whereas the
     * stock menu compares the HAL's empty string to "0" and reports off. Read
     * the actual live value first; default only an absent value, never an
     * explicit disable. No HDR signal/capability or output state is invented. */
    public static String hdrGetModeState(String key) {
        try {
            String live = (String) nativeState("nativeGetModeState", new Class<?>[]{String.class}, key);
            if (!HDR.equals(key) || (live != null && !live.isEmpty())) return live;
            return "0"; // Same absent-property default as the original HWC.
        } catch (Exception e) { throw new IllegalStateException("HDR setting read failed", e); }
    }

    /** Preserve the real HAL setter, then save only a successful explicit HDR
     * choice. A persistence failure is reported; it cannot claim saved state. */
    public static int hdrSetModeState(String key, String value) {
        try {
            int result = (Integer) nativeState("nativeSetModeState", new Class<?>[]{String.class, String.class}, key, value);
            if (result != 0 || !HDR.equals(key) || !("0".equals(value) || "1".equals(value))) return result;
            int saved = (Integer) nativeState("nativeSetModeState", new Class<?>[]{String.class, String.class}, HDR_SAVED, value);
            if (saved != 0) Log.e(TAG, "HDR choice applied but persistence failed", new IllegalStateException("HAL result " + saved));
            return saved;
        } catch (Exception e) { throw new IllegalStateException("HDR setting save failed", e); }
    }

    /** Delegate the same native setter, then notify the ordinary mode director.
     * Failure is reported as a setting error; no success is invented. */
    public static void nativeSetMode(int display, int iface, String mode) {
        try {
            Class<?> c = Class.forName("com.android.server.rkdisplay.RkDisplayModes");
            Method nativeSetter = c.getDeclaredMethod("nativeSetMode", int.class, int.class, String.class);
            nativeSetter.setAccessible(true); nativeSetter.invoke(null, display, iface, mode);
        } catch (Exception e) {
            throw new IllegalStateException("Rockchip mode selection failed", e);
        }
        try {
            DisplayModeDirector d = director;
            if (display == 0 && d != null) {
                Object lock = field(d.getClass(), "mLock").get(d);
                synchronized (lock) {
                    Method notify = d.getClass().getDeclaredMethod("notifyDesiredDisplayModeSpecsChangedLocked");
                    notify.setAccessible(true); notify.invoke(d);
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "Native selection saved; policy notification failed", e);
        }
    }

    /** Preserve the director's native calculation for Auto. For an explicit
     * supported selection, express real size/rate constraints through its
     * desired specs. SurfaceFlinger filters size against this base mode and
     * receives the true fixed range; native HWC configuration is untouched. */
    public static DisplayModeDirector.DesiredDisplayModeSpecs desired(DisplayModeDirector d, int display) {
        director = d;
        DisplayModeDirector.DesiredDisplayModeSpecs specs = null;
        try {
            Method original = d.getClass().getDeclaredMethod("getDesiredDisplayModeSpecs", int.class);
            original.setAccessible(true);
            specs = (DisplayModeDirector.DesiredDisplayModeSpecs) original.invoke(d, display);
            if (display != 0) return specs;
            String selected = property("persist.vendor.resolution.HDMI-A-0");
            Matcher match = MODE.matcher(selected);
            if (!match.matches()) return specs; // Auto/unknown is never silently locked.
            int width = Integer.parseInt(match.group(1)), height = Integer.parseInt(match.group(2));
            float rate = Float.parseFloat(match.group(3));
            Object lock = field(d.getClass(), "mLock").get(d);
            Display.Mode chosen = null;
            synchronized (lock) {
                SparseArray<?> supported = (SparseArray<?>) field(d.getClass(), "mSupportedModesByDisplay").get(d);
                Display.Mode[] modes = (Display.Mode[]) supported.get(display);
                if (modes != null) for (Display.Mode mode : modes) {
                    if (mode.getPhysicalWidth() == width && mode.getPhysicalHeight() == height &&
                        Math.abs(mode.getRefreshRate() - rate) < 0.005f) {
                        if (chosen != null && chosen.getModeId() != mode.getModeId()) return specs;
                        chosen = mode;
                    }
                }
            }
            if (chosen == null) return specs; // Never substitute a different mode.
            Class<?> type = specs.getClass();
            Field primary = field(type, "primaryRefreshRateRange");
            // Construct a separate result. Never mutate the native result or
            // final range fields, including when an ABI check fails halfway.
            Class<?> range = primary.getType();
            Object p = range.getConstructor(float.class, float.class).newInstance(chosen.getRefreshRate(), chosen.getRefreshRate());
            Object a = range.getConstructor(float.class, float.class).newInstance(chosen.getRefreshRate(), chosen.getRefreshRate());
            DisplayModeDirector.DesiredDisplayModeSpecs fixed =
                (DisplayModeDirector.DesiredDisplayModeSpecs) type.getConstructor(int.class, boolean.class, range, range)
                    .newInstance(chosen.getModeId(), false, p, a);
            if (!selected.equals(lastReported)) {
                Log.i(TAG, "Fixed selection applied: " + width + "x" + height + "@" + chosen.getRefreshRate());
                lastReported = selected;
            }
            return fixed;
        } catch (Exception e) {
            if (specs != null) {
                Log.e(TAG, "Policy adapter declined; native specs retained", e);
                return specs;
            }
            throw new IllegalStateException("Exact-firmware display policy mismatch", e);
        }
    }
}
