// SPDX-License-Identifier: MIT
package com.scrcpyandroid.displaypolicy;

import java.nio.file.Files;
import java.nio.file.Paths;

/** Opens classes without constructing services, registering Binder or invoking display JNI. */
public final class LoadServicesProbe {
    public static void main(String[] args) throws Exception {
        if (args.length != 1) throw new IllegalArgumentException("system server classpath");
        Class<?> pcl = Class.forName("dalvik.system.PathClassLoader");
        ClassLoader loader = (ClassLoader) pcl.getConstructor(String.class, ClassLoader.class)
            .newInstance(args[0], ClassLoader.getSystemClassLoader().getParent());
        Class.forName("com.android.server.SystemServer", false, loader);
        Class.forName("com.android.server.rkdisplay.RkDisplayModeLocale", false, loader);
        for (String line : Files.readAllLines(Paths.get("/proc/self/maps"))) {
            if (line.contains("services.jar") || line.contains("services.odex") ||
                line.contains("services.vdex") || line.contains("ethernet-service.odex")) {
                System.out.println("MAP " + line);
            }
        }
        System.out.println("PASS_CLASS_LOAD_ONLY_NO_SYSTEM_SERVICE_CREATED");
    }
}
