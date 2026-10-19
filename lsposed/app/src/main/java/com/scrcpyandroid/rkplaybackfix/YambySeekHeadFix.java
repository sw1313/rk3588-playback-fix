// SPDX-License-Identifier: MIT
package com.scrcpyandroid.rkplaybackfix;

import android.content.pm.ApplicationInfo;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import java.io.EOFException;
import java.io.FileInputStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.Locale;
import java.util.Map;
import java.util.WeakHashMap;
import java.util.function.Consumer;

/** Resolve real recursive indexes using the player's existing ranged input.
 * Peeking is transactional: no read cursor, cue, or visited-node commit on IO
 * failure. Native Cues parsing and the original return-to-Cluster flow remain.
 */
final class YambySeekHeadFix {
    private static final String BASE_SHA = "cdf2fc35e770cd1bd8792ecd65f388c88ff1b2465596623966435c81a1d08be2";
    private static final int CLUSTER = 0x1f43b675, SEEK = 0x4dbb;
    private final Field cues, segment, segmentSize, sent, enabled, seekId, seekPosition, returnPosition, seekForCues, holderPosition;
    private final Method peek, resetPeek, position, length, startCluster;
    private final Consumer<String> report;
    private final Map<Object, Session> sessions = new WeakHashMap<>();
    private static final class Session {
        final LinkedHashSet<Long> pending = new LinkedHashSet<>();
        final HashSet<Long> visited = new HashSet<>();
        int phase; // 0 native, 1 armed, 2 head lookup, 3 validate Cues, 4 resolved/abandoned
        int bytes;
        long expected = -1, clusterPosition, clusterSize;
        void add(long p) { if (p >= 0 && !visited.contains(p) && pending.size() + visited.size() < 64) pending.add(p); }
        long next() {
            Iterator<Long> i = pending.iterator();
            if (!i.hasNext()) return -1;
            long value = i.next(); i.remove(); return value;
        }
    }
    private YambySeekHeadFix(Class<?> ex, Class<?> in, Class<?> holder, Consumer<String> report) throws Exception {
        this.report = report;
        cues = field(ex, "cuesContentPosition", long.class);
        segment = field(ex, "segmentContentPosition", long.class);
        segmentSize = field(ex, "segmentContentSize", long.class);
        sent = field(ex, "sentSeekMap", boolean.class);
        enabled = field(ex, "seekForCuesEnabled", boolean.class);
        seekId = field(ex, "seekEntryId", int.class);
        seekPosition = field(ex, "seekEntryPosition", long.class);
        returnPosition = field(ex, "seekPositionAfterBuildingCues", long.class);
        seekForCues = field(ex, "seekForCues", boolean.class);
        holderPosition = field(holder, "ۥۖۗ۬۠ۤۜ", long.class);
        peek = in.getMethod("ۥۛۡۘۥۘۥ", byte[].class, int.class, int.class);
        resetPeek = in.getMethod("ۥۚۥۘۧ۠ۨ");
        position = in.getMethod("getPosition"); length = in.getMethod("getLength");
        startCluster = ex.getDeclaredMethod("startMasterElement", int.class, long.class, long.class);
        for (Method method : new Method[]{peek, resetPeek, position, length, startCluster}) method.setAccessible(true);
        if (peek.getReturnType() != void.class || resetPeek.getReturnType() != void.class ||
                position.getReturnType() != long.class || length.getReturnType() != long.class)
            throw new IllegalArgumentException("ExtractorInput ABI differs");
    }
    private static Field field(Class<?> c, String name, Class<?> type) throws Exception {
        Field f = c.getDeclaredField(name);
        if (f.getType() != type) throw new IllegalArgumentException("Extractor field ABI differs");
        f.setAccessible(true); return f;
    }
    private static Object invoke(Method m, Object target, Object... args) throws Throwable {
        try { return m.invoke(target, args); }
        catch (InvocationTargetException e) { throw e.getCause(); }
    }
    private synchronized Session session(Object ex) { return sessions.computeIfAbsent(ex, ignored -> new Session()); }
    private static String hash(String path) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        try (FileInputStream in = new FileInputStream(path)) {
            byte[] b = new byte[65536]; int n;
            while ((n = in.read(b)) != -1) digest.update(b, 0, n);
        }
        StringBuilder out = new StringBuilder();
        for (byte b : digest.digest()) out.append(String.format(Locale.ROOT, "%02x", b & 255));
        return out.toString();
    }
    static void install(ApplicationInfo app, ClassLoader loader, boolean selected, Consumer<String> report) {
        if (!selected) { report.accept("Recursive SeekHead fix off; original extractor retained"); return; }
        ArrayList<XC_MethodHook.Unhook> hooks = new ArrayList<>();
        try {
            if (app == null || !BASE_SHA.equals(hash(app.sourceDir))) throw new IllegalArgumentException("APK fingerprint differs");
            Class<?> ex = Class.forName("h1.ۥۘۡۘۥۨ", false, loader);
            Class<?> in = Class.forName("p0.ۥۚۧۤۤۡ", false, loader);
            Class<?> holder = Class.forName("p0.ۥۜۧ۫ۗۛ", false, loader);
            YambySeekHeadFix fix = new YambySeekHeadFix(ex, in, holder, report);
            Method read = ex.getDeclaredMethod("read", in, holder);
            if (read.getReturnType() != int.class) throw new IllegalArgumentException("Extractor read ABI differs");
            hooks.add(XposedBridge.hookMethod(ex.getDeclaredMethod("endMasterElement", int.class), new XC_MethodHook() {
                @Override protected void beforeHookedMethod(MethodHookParam p) throws Throwable {
                    if ((int)p.args[0] != SEEK || fix.sent.getBoolean(p.thisObject)) return;
                    if (fix.seekId.getInt(p.thisObject) == SeekHeadIndex.HEAD)
                        fix.session(p.thisObject).add(fix.seekPosition.getLong(p.thisObject));
                }
            }));
            hooks.add(XposedBridge.hookMethod(fix.startCluster, new XC_MethodHook() {
                @Override protected void beforeHookedMethod(MethodHookParam p) throws Throwable {
                    if ((int)p.args[0] != CLUSTER || fix.sent.getBoolean(p.thisObject) ||
                            !fix.enabled.getBoolean(p.thisObject) || fix.cues.getLong(p.thisObject) != -1) return;
                    Session s = fix.session(p.thisObject);
                    if (s.phase != 0) return;
                    long next = s.next();
                    if (next < 0) return;
                    s.clusterPosition = (long)p.args[1]; s.clusterSize = (long)p.args[2];
                    s.expected = next; s.phase = 1;
                    // Borrow only the native seek transport. The real Cues
                    // offset replaces this before the EBML reader consumes it.
                    fix.cues.setLong(p.thisObject, next);
                }
            }));
            hooks.add(XposedBridge.hookMethod(read, new XC_MethodHook() {
                @Override protected void beforeHookedMethod(MethodHookParam p) {
                    // Xposed logs and swallows thrown hook exceptions. Explicitly
                    // preserve the Extractor IOException contract for the loader.
                    try { fix.beforeRead(p); } catch (Throwable error) { p.setThrowable(error); }
                }
                @Override protected void afterHookedMethod(MethodHookParam p) throws Throwable {
                    if (p.hasThrowable() || !Integer.valueOf(1).equals(p.getResult())) return;
                    Session s = fix.session(p.thisObject);
                    if (s.phase == 1 && fix.holderPosition.getLong(p.args[1]) == s.expected) s.phase = 2;
                }
            }));
            hooks.add(XposedBridge.hookMethod(ex.getDeclaredMethod("seek", long.class, long.class), new XC_MethodHook() {
                @Override protected void beforeHookedMethod(MethodHookParam p) throws Throwable {
                    if ((long)p.args[0] != 0 || fix.sent.getBoolean(p.thisObject)) return;
                    Session s = fix.session(p.thisObject);
                    if (s.phase != 0) fix.cues.setLong(p.thisObject, -1);
                    synchronized (fix) { fix.sessions.remove(p.thisObject); }
                }
            }));
            hooks.add(XposedBridge.hookMethod(ex.getDeclaredMethod("release"), new XC_MethodHook() {
                @Override protected void afterHookedMethod(MethodHookParam p) {
                    synchronized (fix) { fix.sessions.remove(p.thisObject); }
                }
            }));
            report.accept("Recursive SeekHead candidate armed; pinned extractor, native Cues parser retained");
        } catch (Throwable e) {
            for (XC_MethodHook.Unhook h : hooks) h.unhook();
            report.accept("Recursive SeekHead skipped: " + e.getClass().getSimpleName() + "; original extractor retained");
        }
    }
    private void fallback(XC_MethodHook.MethodHookParam p, Session s) throws Throwable {
        s.phase = 4;
        cues.setLong(p.thisObject, -1); returnPosition.setLong(p.thisObject, -1);
        seekForCues.setBoolean(p.thisObject, false);
        XposedBridge.invokeOriginalMethod(startCluster, p.thisObject, new Object[]{CLUSTER, s.clusterPosition, s.clusterSize});
        holderPosition.setLong(p.args[1], s.clusterPosition); p.setResult(1);
        report.accept("Recursive SeekHead unresolved within bounds; original unseekable behavior retained");
    }
    private void beforeRead(XC_MethodHook.MethodHookParam p) throws Throwable {
        Session s = session(p.thisObject);
        if (s.phase != 2 && s.phase != 3) return;
        Object input = p.args[0];
        long at = (long)invoke(position, input), inputLength = (long)invoke(length, input);
        long segmentStart = segment.getLong(p.thisObject), size = segmentSize.getLong(p.thisObject);
        if (at != s.expected || !SeekHeadIndex.validPosition(at, segmentStart, size, inputLength)) {
            fallback(p, s); return;
        }
        try {
            invoke(resetPeek, input);
            if (s.phase == 3) {
                byte[] id = new byte[4]; invoke(peek, input, id, 0, id.length);
                long value = 0; for (byte b : id) value = (value << 8) | (b & 255);
                if (value != SeekHeadIndex.CUES) throw new SeekHeadIndex.Invalid("Cues target differs");
                // No synthetic seekability: the original parser now reads the
                // actual Cues and emits its own validated per-track SeekMap.
                cues.setLong(p.thisObject, at); s.phase = 4;
                report.accept("Recursive SeekHead resolved to real Cues; heads=" + s.visited.size());
                return;
            }
            byte[] prefix = new byte[12]; invoke(peek, input, prefix, 0, prefix.length);
            int count = SeekHeadIndex.headBytes(prefix);
            if (count > 1048576 - s.bytes || s.visited.size() >= 64) throw new SeekHeadIndex.Invalid("index traversal budget");
            byte[] data = new byte[count]; invoke(resetPeek, input); invoke(peek, input, data, 0, data.length);
            // Commit only after the entire peek and parse succeeded. A network
            // IOException leaves both the cursor and this node retriable.
            s.bytes += count;
            java.util.List<SeekHeadIndex.Link> links = SeekHeadIndex.parse(data, segmentStart, size, inputLength);
            s.visited.add(at);
            long found = -1;
            for (SeekHeadIndex.Link link : links) {
                if (link.id == SeekHeadIndex.CUES && found == -1) found = link.position;
                else if (link.id == SeekHeadIndex.HEAD) s.add(link.position);
            }
            long next = found != -1 ? found : s.next();
            if (next == -1) { fallback(p, s); return; }
            s.expected = next; s.phase = found == -1 ? 2 : 3;
            holderPosition.setLong(p.args[1], next); p.setResult(1);
        } catch (SeekHeadIndex.Invalid | EOFException invalid) {
            s.visited.add(at);
            long next = s.next();
            if (next == -1) fallback(p, s);
            else { s.expected = next; s.phase = 2; holderPosition.setLong(p.args[1], next); p.setResult(1); }
        } finally {
            invoke(resetPeek, input);
        }
    }
}
