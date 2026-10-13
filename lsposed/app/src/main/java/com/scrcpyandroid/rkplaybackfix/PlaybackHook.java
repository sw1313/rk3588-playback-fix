package com.scrcpyandroid.rkplaybackfix;

import android.util.Log;
import java.util.HashSet;
import java.util.Set;
import java.util.Enumeration;
import java.lang.reflect.Method;
import de.robv.android.xposed.IXposedHookLoadPackage;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XSharedPreferences;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;
import de.robv.android.xposed.callbacks.XC_LoadPackage;

public final class PlaybackHook implements IXposedHookLoadPackage {
    private static final String TAG = "RKPlaybackFix";
    private static final String MODULE = "com.scrcpyandroid.rkplaybackfix";
    private final Set<String> reported = new HashSet<>();
    private final Set<Class<?>> hooked = new HashSet<>();
    private final ThreadLocal<Boolean> discovering = new ThreadLocal<>();
    private static final String[] KNOWN_BINDINGS = {
        "is.xyz.mpv.MPVLib", "dev.jdtech.mpv.MPVLib",
        "com.github.damontecres.wholphin.mpv.MPVLib",
        "live.mehiz.mpvkt.mpv.MPVLib", "com.github.moneofone.mpv.MPVLib"
    };
    private XSharedPreferences prefs;
    private long lastReload;
    private boolean nonCopy;
    private boolean yambySettings;

    @Override public void handleLoadPackage(XC_LoadPackage.LoadPackageParam app) {
        // LSPosed supplies the user-selected scope. Never run in system processes or ourselves.
        if (MODULE.equals(app.packageName) || "android".equals(app.packageName)
                || app.packageName.startsWith("com.android.")) return;
        Class<?> props = XposedHelpers.findClass("android.os.SystemProperties", null);
        String board = (String) XposedHelpers.callStaticMethod(props, "get", "ro.board.platform", "");
        if (!"rk3588".equals(board)) { report("Unsupported platform: " + board); return; }
        prefs = new XSharedPreferences(MODULE, "fixes");
        refresh();
        if ("com.hush.yamby".equals(app.packageName)) {
            yambySettings = YambySettingsPatch.install(app.classLoader, this::report,
                () -> { refresh(); return prefs.getFile().canRead() && prefs.getBoolean("yamby_menu", true); });
        }
        for (String name : KNOWN_BINDINGS) {
            Class<?> lib = XposedHelpers.findClassIfExists(name, app.classLoader);
            if (lib != null) attach(lib);
        }
        scan(app.classLoader);
        if (hooked.isEmpty()) {
            // Some clients install their mpv extension in a separate dex/class loader later.
            XposedBridge.hookAllMethods(ClassLoader.class, "loadClass", new XC_MethodHook() {
                @Override protected void afterHookedMethod(MethodHookParam p) {
                    if (Boolean.TRUE.equals(discovering.get()) || p.hasThrowable()
                            || p.args.length == 0 || !(p.args[0] instanceof String)) return;
                    String name = (String) p.args[0];
                    if (candidate(name) && p.getResult() instanceof Class<?>) {
                        discovering.set(true);
                        try { attach((Class<?>) p.getResult()); } finally { discovering.remove(); }
                    }
                }
            });
            Class<?> dex = XposedHelpers.findClass("dalvik.system.BaseDexClassLoader", null);
            XposedBridge.hookAllConstructors(dex, new XC_MethodHook() {
                @Override protected void afterHookedMethod(MethodHookParam p) {
                    if (!p.hasThrowable() && p.thisObject instanceof ClassLoader) scan((ClassLoader) p.thisObject);
                }
            });
            report("Waiting for a supported mpv binding in " + app.packageName);
        }
        report("Loaded in " + app.processName + "; bindings=" + hooked.size()
            + "; noncopy=" + nonCopy + "; yambyMenu=" + yambySettings);
    }

    private static boolean candidate(String name) {
        return name.contains("mpv") || name.contains("MPV") || name.contains("Mpv");
    }

    private void scan(ClassLoader loader) {
        if (loader == null || Boolean.TRUE.equals(discovering.get())) return;
        discovering.set(true);
        try {
            Object pathList = XposedHelpers.getObjectField(loader, "pathList");
            Object[] elements = (Object[]) XposedHelpers.getObjectField(pathList, "dexElements");
            Set<String> names = new HashSet<>();
            for (Object element : elements) {
                Object dex = XposedHelpers.getObjectField(element, "dexFile");
                if (dex == null) continue;
                @SuppressWarnings("unchecked") Enumeration<String> entries =
                    (Enumeration<String>) XposedHelpers.callMethod(dex, "entries");
                while (entries.hasMoreElements()) {
                    String name = entries.nextElement();
                    if (candidate(name) && names.size() < 128) names.add(name);
                }
            }
            // Loading is done after enumeration, avoiding class-loader changes mid-iteration.
            for (String name : names) {
                Class<?> lib = XposedHelpers.findClassIfExists(name, loader);
                if (lib != null) attach(lib);
            }
        } catch (Throwable e) {
            report("Dex discovery unavailable: " + e.getClass().getSimpleName());
        } finally { discovering.remove(); }
    }

    private synchronized void attach(Class<?> lib) {
        if (hooked.contains(lib)) return;
        Method[] methods;
        try { methods = lib.getDeclaredMethods(); } catch (Throwable ignored) { return; }
        boolean setter = false;
        for (Method m : methods) if (setter(m) && optionIndex(m.getParameterTypes()) >= 0) setter = true;
        if (!setter) return;
        hooked.add(lib);
        XC_MethodHook options = new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                int index = optionIndex(((Method) p.method).getParameterTypes());
                if (index < 0 || !(p.args[index] instanceof String) || !(p.args[index + 1] instanceof String)) return;
                refresh();
                String key = (String) p.args[index], value = (String) p.args[index + 1];
                if ("hwdec".equals(key)) {
                    String replacement = value;
                    if (nonCopy && !yambySettings && (value.contains("mediacodec-copy") || value.startsWith("auto"))) {
                        replacement = "mediacodec";
                        p.args[index + 1] = replacement;
                    }
                    report("hwdec request=" + value + " effective=" + replacement);
                } else if ("vo".equals(key)) {
                    report("vo request=" + value + " effective=" + p.args[index + 1]);
                }
            }
            @Override protected void afterHookedMethod(MethodHookParam p) {
                int index = optionIndex(((Method) p.method).getParameterTypes());
                if (index >= 0 && ("hwdec".equals(p.args[index]) || "vo".equals(p.args[index]))) {
                    if (p.hasThrowable()) report("setter failed: " + p.getThrowable().getClass().getSimpleName());
                    else if (p.getResult() instanceof Integer && ((Integer) p.getResult()) < 0)
                        report("setter rejected " + p.args[index] + ": " + p.getResult());
                }
            }
        };
        XC_MethodHook getter = new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if (p.args.length == 1 && "hwdec-current".equals(p.args[0]) && !p.hasThrowable())
                    report("actual hwdec=" + p.getResult());
            }
        };
        int count = 0;
        for (Method m : methods) {
            if (setter(m) && optionIndex(m.getParameterTypes()) >= 0) {
                XposedBridge.hookMethod(m, options); count++;
            } else if ("getPropertyString".equals(m.getName()) && m.getParameterTypes().length == 1
                    && m.getParameterTypes()[0] == String.class && m.getReturnType() == String.class) {
                XposedBridge.hookMethod(m, getter);
            }
        }
        report("Binding attached: " + lib.getName() + "; setters=" + count);
    }

    private static boolean setter(Method method) {
        String name = method.getName();
        return name.equals("setOptionString") || name.equals("setPropertyString")
            || name.equals("mpvSetOptionString") || name.equals("mpvSetPropertyString")
            || name.equals("mpv_set_option_string") || name.equals("mpv_set_property_string");
    }

    private static int optionIndex(Class<?>[] types) {
        if (types.length == 2 && types[0] == String.class && types[1] == String.class) return 0;
        if (types.length == 3 && (types[0] == long.class || types[0] == Long.class)
                && types[1] == String.class && types[2] == String.class) return 1;
        return -1;
    }

    private synchronized void refresh() {
        long now = android.os.SystemClock.elapsedRealtime();
        if (lastReload != 0 && now - lastReload < 1000) return;
        lastReload = now;
        prefs.reload();
        boolean readable = prefs.getFile().canRead();
        nonCopy = readable && prefs.getBoolean("noncopy", false);
        if (!readable) report("Preferences unavailable; diagnostic only. Open module settings after activation.");
    }

    private synchronized void report(String message) {
        // Only codec/render options are logged. No URLs, filenames, tokens or subtitles.
        if (reported.size() >= 64 || !reported.add(message)) return;
        Log.i(TAG, message);
        XposedBridge.log(TAG + ": " + message);
    }
}
