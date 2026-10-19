package com.scrcpyandroid.rkplaybackfix;

import android.content.pm.ApplicationInfo;
import android.os.Build;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.*;
import java.util.function.Consumer;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import org.json.JSONObject;

/** Boot/app-start latched native client fix, scoped to a pinned Yamby ABI.
 * Original APK/libraries are never written. It keeps mediacodec-copy and P010;
 * it does not rewrite HW to HW+, quantize pixels, or skip synchronization.
 */
final class YambyP010CopyFix {
    private static final String ENTRY = "assets/p010/libavcodec.so";
    private static final String CONTRACT = "assets/p010/contract.json";
    private static boolean attempted, loaded, preparing;

    static void install(ApplicationInfo app, ClassLoader loader, String modulePath,
                        boolean enabled, Consumer<String> report) {
        if (!enabled) { report.accept("Main10 copy fix off; original native loader retained"); return; }
        if (app == null || modulePath == null || Build.VERSION.SDK_INT != 32 || !android.os.Process.is64Bit()
                || !"rk3588".equals(property("ro.board.platform"))
                || !"1".equals(property("debug.rkcodec2.surface_upgrade"))
                || !"1".equals(property("debug.rkcodec2.p010_pool"))) {
            report.accept("Main10 copy fix skipped: firmware/Decode Stack prerequisite differs"); return;
        }
        XposedBridge.hookAllMethods(Runtime.class, "loadLibrary0", new XC_MethodHook() {
            @Override protected void beforeHookedMethod(MethodHookParam p) {
                if (p.args.length == 0 || !(p.args[p.args.length - 1] instanceof String)) return;
                String name = (String)p.args[p.args.length - 1];
                if (!name.equals("mpv") && !name.equals("player") && !name.equals("avcodec")) return;
                synchronized (YambyP010CopyFix.class) {
                    if (preparing) return;
                    if (!attempted) {
                        attempted = true; preparing = true;
                        try { preload(app, loader, modulePath); loaded = true; report.accept("Main10 P010 copy client active; original HW selection retained"); }
                        catch (Throwable e) { report.accept("Main10 copy fix unavailable: " + e.getClass().getSimpleName() + "; original loader retained"); }
                        finally { preparing = false; }
                    }
                    // An explicit System.loadLibrary(avcodec) must not load a
                    // second codec library after the validated preload.
                    if (loaded && name.equals("avcodec")) p.setResult(null);
                }
            }
        });
        report.accept("Main10 copy fix armed; native ABI preflight pending library load");
    }

    private static String property(String key) {
        try { return (String)XposedHelpers.callStaticMethod(
                XposedHelpers.findClass("android.os.SystemProperties", null), "get", key, ""); }
        catch (Throwable e) { return ""; }
    }
    private static String digest(InputStream stream) throws Exception {
        MessageDigest sha = MessageDigest.getInstance("SHA-256"); byte[] buf = new byte[65536];
        int n; while ((n = stream.read(buf)) != -1) sha.update(buf, 0, n);
        StringBuilder s = new StringBuilder(); for (byte b : sha.digest()) s.append(String.format(Locale.ROOT,"%02x",b & 255)); return s.toString();
    }
    private static String originalHash(ApplicationInfo app, String library) throws Exception {
        ArrayList<String> apks = new ArrayList<>(); apks.add(app.sourceDir);
        if (app.splitSourceDirs != null) Collections.addAll(apks, app.splitSourceDirs);
        for (String path : apks) try (ZipFile zip = new ZipFile(path)) {
            ZipEntry e = zip.getEntry("lib/arm64-v8a/" + library);
            if (e != null) try (InputStream in = zip.getInputStream(e)) { return digest(in); }
        }
        throw new IOException("Missing pinned ARM64 library");
    }
    private static void preload(ApplicationInfo app, ClassLoader loader, String modulePath) throws Exception {
        // Fail closed if avcodec was already loaded before our hook. Never
        // replace callbacks inside an active decoder or an existing native map.
        try (BufferedReader maps = new BufferedReader(new FileReader("/proc/self/maps"))) {
            String line; while ((line = maps.readLine()) != null)
                if (line.contains("libavcodec.so")) throw new IllegalStateException("Restart required");
        }
        try (ZipFile zip = new ZipFile(modulePath)) {
            ZipEntry ce = zip.getEntry(CONTRACT), binary = zip.getEntry(ENTRY);
            if (ce == null || binary == null || ce.getSize() > 8192 || binary.getSize() <= 0 || binary.getSize() > 32*1024*1024)
                throw new IOException("Payload unavailable");
            byte[] text;
            try (InputStream in = zip.getInputStream(ce); ByteArrayOutputStream out = new ByteArrayOutputStream()) {
                byte[] buf = new byte[4096]; int n;
                while ((n = in.read(buf)) != -1) { if (out.size()+n>8192) throw new IOException("Oversize contract");out.write(buf,0,n); }
                text = out.toByteArray();
            }
            JSONObject contract = new JSONObject(new String(text, StandardCharsets.UTF_8));
            JSONObject framework = contract.getJSONObject("framework_libraries");
            for (String path : new String[]{"/system/lib64/libsfplugin_ccodec.so","/system/lib64/libcp10.so"})
                try (InputStream in = new FileInputStream(path)) {
                    if (!digest(in).equals(framework.getString(path))) throw new SecurityException("Framework pool ABI differs");
                }
            JSONObject expected = contract.getJSONObject("original_libraries");
            for (String name : new String[]{"libavcodec.so","libavutil.so","libswresample.so","libmpv.so"})
                if (!originalHash(app,name).equals(expected.getString(name))) throw new SecurityException("Original library hash differs");
            String sha = contract.getString("payload_sha256");
            if (!sha.matches("[a-f0-9]{64}")) throw new SecurityException("Payload hash invalid");
            File base = new File(app.dataDir,"code_cache/rk-main10-copy/"+sha);
            if (!base.isDirectory() && !base.mkdirs()) throw new IOException("Cache unavailable");
            File library = new File(base,"libavcodec.so");
            if (!library.exists()) {
                File temp = File.createTempFile("codec-",".tmp",base);
                try {
                    try (InputStream in = zip.getInputStream(binary); FileOutputStream out = new FileOutputStream(temp)) {
                        byte[] buf = new byte[65536]; int n;long total=0;
                        while ((n=in.read(buf))!=-1) { total+=n;if(total>32*1024*1024)throw new IOException("Oversize binary");out.write(buf,0,n); }
                        out.getFD().sync();
                    }
                    try (InputStream in = new FileInputStream(temp)) { if(!digest(in).equals(sha))throw new SecurityException("Payload differs"); }
                    if (!temp.setReadOnly() || !temp.renameTo(library)) throw new IOException("Cannot finalize payload");
                } finally { if (temp.exists()) temp.delete(); }
            }
            try (InputStream in = new FileInputStream(library)) { if(!digest(in).equals(sha))throw new SecurityException("Cached payload differs"); }
            // Android 12 Runtime.nativeLoad places the library in the target
            // app classloader namespace. Its remaining FFmpeg libs stay original.
            Object error = XposedHelpers.callStaticMethod(Runtime.class,"nativeLoad",library.getAbsolutePath(),loader);
            if (error != null) throw new UnsatisfiedLinkError("Validated native preload rejected");
        }
    }
}
