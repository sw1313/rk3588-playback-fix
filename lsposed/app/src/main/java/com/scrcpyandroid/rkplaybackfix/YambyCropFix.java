package com.scrcpyandroid.rkplaybackfix;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;

/** Opt-in compensation from the current decoder's reported crop and buffer. */
final class YambyCropFix {
    private final Class<?> lib;
    private final File directory;
    private final Consumer<String> log;
    private final BooleanSupplier enabled;
    private CropGeometry geometry = new CropGeometry();
    private boolean ready, changing;
    private String activePath;

    private YambyCropFix(Class<?> lib, String dataDir, Consumer<String> log, BooleanSupplier enabled) {
        this.lib = lib; this.directory = new File(dataDir, "files/rkplaybackfix");
        this.log = log; this.enabled = enabled;
    }

    static void install(ClassLoader loader, String dataDir, Consumer<String> log, BooleanSupplier enabled) {
        if (!enabled.getAsBoolean() || dataDir == null) return;
        try {
            Class<?> props = XposedHelpers.findClass("android.os.SystemProperties", null);
            if (!"rk3588".equals(XposedHelpers.callStaticMethod(props, "get", "ro.board.platform", ""))) return;
            Class<?> lib = XposedHelpers.findClass("is.xyz.mpv.MPVLib", loader);
            new YambyCropFix(lib, dataDir, log, enabled).hook();
            log.accept("Optional RK3588 adaptive crop correction installed");
        } catch (Throwable e) { log.accept("Crop correction adapter unavailable: " + e.getClass().getSimpleName()); }
    }

    private void hook() {
        XC_MethodHook reset = new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                synchronized (YambyCropFix.this) { ready = false; geometry = new CropGeometry(); activePath = null; }
            }
        };
        XposedBridge.hookAllMethods(lib, "create", reset);
        XposedBridge.hookAllMethods(lib, "destroy", reset);
        XposedBridge.hookAllMethods(lib, "init", new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if (!p.hasThrowable()) synchronized (YambyCropFix.this) { ready = true; reconcile(); }
            }
        });
        XposedBridge.hookAllMethods(lib, "logMessage", new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if (p.args.length != 3 || !(p.args[0] instanceof String) || !(p.args[2] instanceof String)) return;
                synchronized (YambyCropFix.this) {
                    if (ready && geometry.observe((String) p.args[0], (String) p.args[2])) reconcile();
                }
            }
        });
        XC_MethodHook preserve = new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if (p.hasThrowable()) return;
                boolean changed = p.args.length == 2 && "glsl-shaders".equals(p.args[0]);
                if (p.args.length == 1 && p.args[0] instanceof String[]) {
                    String[] command = (String[]) p.args[0];
                    changed = command.length >= 2 && "change-list".equals(command[0]) && "glsl-shaders".equals(command[1]);
                }
                if (changed) synchronized (YambyCropFix.this) { reconcile(); }
            }
        };
        XposedBridge.hookAllMethods(lib, "setOptionString", preserve);
        XposedBridge.hookAllMethods(lib, "setPropertyString", preserve);
        XposedBridge.hookAllMethods(lib, "command", preserve);
        XposedBridge.hookAllMethods(lib, "event", new XC_MethodHook() {
            @Override protected void afterHookedMethod(MethodHookParam p) {
                if (p.args.length != 1 || !(p.args[0] instanceof Integer)) return;
                int event = (Integer) p.args[0];
                synchronized (YambyCropFix.this) {
                    // START_FILE/END_FILE: discard previous video's metadata.
                    if (event == 6 || event == 7) geometry = new CropGeometry();
                    if (event == 6 || event == 7 || event == 17) reconcile();
                }
            }
        });
    }

    private void reconcile() {
        if (!ready || changing) return;
        changing = true;
        try {
            Object instance = XposedHelpers.getStaticObjectField(lib, "INSTANCE");
            Integer width = (Integer) XposedHelpers.callMethod(instance, "getPropertyInt", "video-params/w");
            Integer height = (Integer) XposedHelpers.callMethod(instance, "getPropertyInt", "video-params/h");
            String hwdec = (String) XposedHelpers.callMethod(instance, "getPropertyString", "hwdec-current");
            String current = (String) XposedHelpers.callMethod(instance, "getPropertyString", "glsl-shaders");
            String desired = null;
            if (enabled.getAsBoolean() && geometry.matches(width, height, hwdec)) {
                File shader = new File(directory, "crop-" + geometry.key() + ".glsl");
                desired = shader.getAbsolutePath();
                if (!desired.equals(activePath) || !shader.isFile()) {
                    if (!directory.isDirectory() && !directory.mkdirs()) throw new java.io.IOException("Shader directory unavailable");
                    try (FileOutputStream output = new FileOutputStream(shader)) {
                        output.write(geometry.shader().getBytes(StandardCharsets.UTF_8));
                    }
                }
            }
            if (activePath != null && !activePath.equals(desired) && contains(current, activePath)) {
                XposedHelpers.callMethod(instance, "command", (Object) new String[]{"change-list", "glsl-shaders", "remove", activePath});
            }
            if (desired != null && !contains(current, desired)) {
                XposedHelpers.callMethod(instance, "command", (Object) new String[]{"change-list", "glsl-shaders", "append", desired});
                log.accept("Crop correction requested: " + geometry.key());
            }
            activePath = desired;
        } catch (Throwable e) { log.accept("Crop correction update unavailable: " + e.getClass().getSimpleName()); }
        finally { changing = false; }
    }

    private static boolean contains(String list, String path) {
        if (list == null) return false;
        String entries = ":" + list.replace('\n', ':') + ":";
        return entries.contains(":" + path + ":");
    }
}
