package com.scrcpyandroid.rkplaybackfix;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;
import java.util.concurrent.atomic.AtomicBoolean;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;

/** Opt-in correction for the observed RK3588 1920x1104 buffer with crop-top=4. */
final class YambyCropFix {
    private static final String SHADER =
        "//!HOOK RGB\n//!BIND HOOKED\n//!DESC RK3588 1080p crop-top=4 correction\n"
        + "//!WHEN HOOKED.w 1920 - ! HOOKED.h 1080 - ! *\n"
        + "vec4 hook() {\n"
        + "    bool affected = abs(HOOKED_size.x - 1920.0) < 0.5\n"
        + "        && (abs(HOOKED_size.y - 1104.0) < 0.5 || abs(HOOKED_size.y - 1080.0) < 0.5);\n"
        + "    return HOOKED_tex(HOOKED_pos + (affected ? vec2(0.0, 4.0) * HOOKED_pt : vec2(0.0)));\n"
        + "}\n";

    static void install(ClassLoader loader, String dataDir, Consumer<String> log, BooleanSupplier enabled) {
        if (!enabled.getAsBoolean() || dataDir == null) return;
        try {
            Class<?> props = XposedHelpers.findClass("android.os.SystemProperties", null);
            if (!"rk3588".equals(XposedHelpers.callStaticMethod(props, "get", "ro.board.platform", ""))) return;
            Class<?> lib = XposedHelpers.findClass("is.xyz.mpv.MPVLib", loader);
            File shader = new File(dataDir, "files/rkplaybackfix/crop-top4.glsl");
            String path = shader.getAbsolutePath();
            ThreadLocal<Boolean> changing = new ThreadLocal<>();
            AtomicBoolean ready = new AtomicBoolean();
            XposedBridge.hookAllMethods(lib, "create", new XC_MethodHook() {
                @Override protected void beforeHookedMethod(MethodHookParam p) { ready.set(false); }
            });
            XposedBridge.hookAllMethods(lib, "destroy", new XC_MethodHook() {
                @Override protected void beforeHookedMethod(MethodHookParam p) { ready.set(false); }
            });
            XposedBridge.hookAllMethods(lib, "init", new XC_MethodHook() {
                @Override protected void beforeHookedMethod(MethodHookParam p) {
                    if (!enabled.getAsBoolean()) return;
                    try {
                        File directory = shader.getParentFile();
                        if (!directory.isDirectory() && !directory.mkdirs()) throw new java.io.IOException("Shader directory unavailable");
                        try (FileOutputStream output = new FileOutputStream(shader)) {
                            output.write(SHADER.getBytes(StandardCharsets.UTF_8));
                        }
                    } catch (Throwable e) { log.accept("Crop correction unavailable: " + e.getClass().getSimpleName()); }
                }
                @Override protected void afterHookedMethod(MethodHookParam p) {
                    if (p.hasThrowable()) return;
                    ready.set(true);
                    appendIfMissing(p.thisObject, path, shader, changing, log, enabled);
                }
            });
            XC_MethodHook preserve = new XC_MethodHook() {
                @Override protected void afterHookedMethod(MethodHookParam p) {
                    if (!ready.get() || p.hasThrowable() || Boolean.TRUE.equals(changing.get())) return;
                    boolean shadersChanged = p.args.length == 2 && "glsl-shaders".equals(p.args[0]);
                    if (p.args.length == 1 && p.args[0] instanceof String[]) {
                        String[] command = (String[]) p.args[0];
                        shadersChanged = command.length >= 2 && "change-list".equals(command[0]) && "glsl-shaders".equals(command[1]);
                    }
                    if (shadersChanged) appendIfMissing(p.thisObject, path, shader, changing, log, enabled);
                }
            };
            XposedBridge.hookAllMethods(lib, "setOptionString", preserve);
            XposedBridge.hookAllMethods(lib, "setPropertyString", preserve);
            XposedBridge.hookAllMethods(lib, "command", preserve);
            XposedBridge.hookAllMethods(lib, "event", new XC_MethodHook() {
                @Override protected void afterHookedMethod(MethodHookParam p) {
                    if (!ready.get() || p.args.length != 1 || !Integer.valueOf(17).equals(p.args[0])) return;
                    try {
                        Object instance = XposedHelpers.getStaticObjectField(lib, "INSTANCE");
                        appendIfMissing(instance, path, shader, changing, log, enabled);
                    } catch (Throwable e) { log.accept("Crop video check unavailable: " + e.getClass().getSimpleName()); }
                }
            });
            log.accept("Optional RK3588 crop correction installed");
        } catch (Throwable e) { log.accept("Crop correction adapter unavailable: " + e.getClass().getSimpleName()); }
    }

    private static void appendIfMissing(Object instance, String path, File shader,
            ThreadLocal<Boolean> changing, Consumer<String> log, BooleanSupplier enabled) {
        if (!enabled.getAsBoolean() || !shader.isFile() || Boolean.TRUE.equals(changing.get())) return;
        changing.set(true);
        try {
            String current = (String) XposedHelpers.callMethod(instance, "getPropertyString", "glsl-shaders");
            Integer width = (Integer) XposedHelpers.callMethod(instance, "getPropertyInt", "video-params/w");
            Integer height = (Integer) XposedHelpers.callMethod(instance, "getPropertyInt", "video-params/h");
            String format = (String) XposedHelpers.callMethod(instance, "getPropertyString", "video-format");
            String hwdec = (String) XposedHelpers.callMethod(instance, "getPropertyString", "hwdec-current");
            if (width != null && height != null && format != null && hwdec != null) {
                log.accept("Crop video check: " + width + "x" + height + "; format=" + format + "; hwdec=" + hwdec);
                boolean supportedCodec = "hevc".equals(format) || "h264".equals(format);
                if (width != 1920 || height != 1080 || !supportedCodec || !"mediacodec".equals(hwdec)) {
                    if (current != null && current.contains(path))
                        XposedHelpers.callMethod(instance, "command", (Object) new String[]{"change-list", "glsl-shaders", "remove", path});
                    return;
                }
            }
            if (current == null || !current.contains(path)) {
                XposedHelpers.callMethod(instance, "command", (Object) new String[]{"change-list", "glsl-shaders", "append", path});
            }
            String applied = (String) XposedHelpers.callMethod(instance, "getPropertyString", "glsl-shaders");
            log.accept(applied != null && applied.contains(path)
                ? "RK3588 crop shader active for this player" : "Crop shader update requested for this player");
        } catch (Throwable e) { log.accept("Crop shader update failed: " + e.getClass().getSimpleName()); }
        finally { changing.remove(); }
    }
}
