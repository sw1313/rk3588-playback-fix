package com.scrcpyandroid.rkplaybackfix;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;

/** Adapter verified against Yamby 2.1.0.11 (20010012). Unknown layouts are left alone. */
final class YambySettingsPatch {
    private static final String REGISTRY = "rc.\u06e5\u06dc\u06db\u06e7\u06e6\u06e6";
    private static final String FACTORY = "\u06e5\u06e4\u06ec\u06e0\u06e0\u06e7";
    private static final String INSTANCE = "\u06e5\u06d6\u06d9\u06df\u06d9";
    private static final String GETTER = "\u06e5\u06e4\u06d8\u06e8\u06e8\u06e4";
    private static final String PLAYER = "xc.\u06e5\u06e1\u06e4\u06e1\u06d8\u06e7";
    private static final String NORMALIZE = "\u06e5\u06dc\u06dc\u06d8\u06d9\u06d7\u06e4";
    private static final String FLOW = "s5.\u06e5\u06d6\u06d7\u06ec\u06e0\u06e4\u06dc";

    static boolean install(ClassLoader loader, Consumer<String> log, BooleanSupplier enabled) {
        try {
            Class<?> registry = XposedHelpers.findClass(REGISTRY, loader);
            Method factory = XposedHelpers.findMethodExact(registry, FACTORY, boolean.class);
            Class<?> player = XposedHelpers.findClass(PLAYER, loader);
            Method normalize = XposedHelpers.findMethodExact(player, NORMALIZE, boolean.class, boolean.class);
            Class<?> flow = XposedHelpers.findClass(FLOW, loader);
            Method setValue = XposedHelpers.findMethodExact(flow, "setValue", Object.class);
            ThreadLocal<Object> decoderBeingNormalized = new ThreadLocal<>();
            XposedBridge.hookMethod(factory, new XC_MethodHook() {
                @Override protected void afterHookedMethod(MethodHookParam p) {
                    if (!enabled.getAsBoolean() || p.hasThrowable() || !(p.getResult() instanceof List<?>)) return;
                    List<?> original = (List<?>) p.getResult();
                    Object hw = null;
                    boolean sw = false;
                    for (Object entry : original) {
                        String text = String.valueOf(entry);
                        if (text.contains("HW+")) return;
                        if (text.contains("mediacodec-copy") && text.contains("HW")) hw = entry;
                        if (text.contains("SW")) sw = true;
                    }
                    if (hw == null || !sw) { log.accept("Yamby codec list shape unknown; unchanged"); return; }
                    try {
                        String text = hw.toString();
                        boolean valueFirst = text.indexOf("mediacodec-copy") < text.indexOf("HW");
                        Object hwPlus = XposedHelpers.newInstance(hw.getClass(),
                            valueFirst ? "mediacodec" : "HW+", valueFirst ? "HW+" : "mediacodec");
                        ArrayList<Object> choices = new ArrayList<>(original);
                        choices.add(0, hwPlus);
                        p.setResult(choices);
                        log.accept("Yamby default decoder menu: HW+, HW, SW");
                    } catch (Throwable e) { log.accept("Yamby menu adapter failed: " + e.getClass().getSimpleName()); }
                }
            });
            XposedBridge.hookMethod(normalize, new XC_MethodHook() {
                @Override protected void beforeHookedMethod(MethodHookParam p) {
                    if (!enabled.getAsBoolean()) return;
                    try {
                        Object singleton = XposedHelpers.getStaticObjectField(registry, INSTANCE);
                        Object decoder = XposedHelpers.callMethod(singleton, GETTER);
                        if ("mediacodec".equals(XposedHelpers.callMethod(decoder, "getValue")))
                            decoderBeingNormalized.set(decoder);
                    } catch (Throwable e) { log.accept("Yamby default guard unavailable: " + e.getClass().getSimpleName()); }
                }
                @Override protected void afterHookedMethod(MethodHookParam p) { decoderBeingNormalized.remove(); }
            });
            XposedBridge.hookMethod(setValue, new XC_MethodHook() {
                @Override protected void beforeHookedMethod(MethodHookParam p) {
                    // Only suppress the player's automatic downgrade of this exact default.
                    // Menu selections and manual per-video HW/SW changes pass through normally.
                    if (decoderBeingNormalized.get() == p.thisObject && "mediacodec-copy".equals(p.args[0])) {
                        p.setResult(null);
                        log.accept("Preserved Yamby HW+ default during player initialization");
                    }
                }
            });
            log.accept("Yamby default settings adapter installed");
            return true;
        } catch (Throwable e) {
            log.accept("Yamby version requires a settings adapter: " + e.getClass().getSimpleName());
            return false;
        }
    }
}
