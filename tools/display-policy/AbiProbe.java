// SPDX-License-Identifier: MIT
package com.scrcpyandroid.displaypolicy;

import java.lang.reflect.*;
import java.util.Locale;

/** Reflection/formatting only: never constructs system services or invokes display JNI. */
public final class AbiProbe {
    public static void main(String[] args) throws Exception {
        ClassLoader loader = AbiProbe.class.getClassLoader();
        Class<?> modes = Class.forName("com.android.server.rkdisplay.RkDisplayModes", false, loader);
        Method setter = modes.getDeclaredMethod("nativeSetMode", int.class, int.class, String.class);
        if (!Modifier.isStatic(setter.getModifiers()) || !Modifier.isNative(setter.getModifiers()) || setter.getReturnType()!=void.class)
            throw new IllegalStateException("Native setter ABI");
        Class<?> director = Class.forName("com.android.server.display.DisplayModeDirector", false, loader);
        director.getDeclaredField("mLock"); director.getDeclaredField("mSupportedModesByDisplay");
        director.getDeclaredMethod("notifyDesiredDisplayModeSpecsChangedLocked");
        Class<?> specs = Class.forName(director.getName()+"$DesiredDisplayModeSpecs", false, loader);
        if (director.getDeclaredMethod("getDesiredDisplayModeSpecs", int.class).getReturnType()!=specs)
            throw new IllegalStateException("Desired specs ABI");
        Class<?> range = specs.getDeclaredField("primaryRefreshRateRange").getType();
        if (specs.getDeclaredField("appRequestRefreshRateRange").getType()!=range)
            throw new IllegalStateException("Range ABI");
        specs.getConstructor(int.class, boolean.class, range, range);
        range.getConstructor(float.class, float.class);
        Class<?> helper = Class.forName("com.android.server.rkdisplay.RkDisplayModeLocale", false, loader);
        helper.getMethod("desired", director, int.class);
        helper.getMethod("nativeSetMode", int.class, int.class, String.class);
        helper.getMethod("hdrGetModeState", String.class);
        helper.getMethod("hdrSetModeState", String.class, String.class);
        helper.getMethod("selectionProperty", String.class, String.class);
        Method builtIn = modes.getDeclaredMethod("nativeGetBuiltIn", int.class);
        if (!Modifier.isStatic(builtIn.getModifiers()) || !Modifier.isNative(builtIn.getModifiers()) || builtIn.getReturnType()!=int.class) throw new IllegalStateException("Connector query ABI");
        for (String name : new String[]{"nativeGetModeState", "nativeSetModeState"}) {
            Method m = name.equals("nativeGetModeState") ? modes.getDeclaredMethod(name, String.class) : modes.getDeclaredMethod(name, String.class, String.class);
            if (!Modifier.isStatic(m.getModifiers()) || !Modifier.isNative(m.getModifiers())) throw new IllegalStateException("Native HDR ABI");
        }
        Method format = helper.getMethod("format", String.class, Object[].class);
        Locale saved = Locale.getDefault();
        try {
            for (String tag : new String[]{"en-US","zh-CN","de-DE","fr-FR","ar-EG","fa-IR"}) {
                Locale.setDefault(Locale.forLanguageTag(tag));
                if (!"60.00".equals(format.invoke(null, "%.2f", new Object[]{60f})))
                    throw new IllegalStateException("Locale formatter "+tag);
            }
        } finally { Locale.setDefault(saved); }
        System.out.println("PASS exact display-policy ABI; six locales; no native display calls");
    }
}
