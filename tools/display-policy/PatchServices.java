// SPDX-License-Identifier: MIT
package com.scrcpyandroid.displaypolicy;

import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.zip.*;
import org.jf.dexlib2.*;
import org.jf.dexlib2.iface.*;
import org.jf.dexlib2.iface.instruction.*;
import org.jf.dexlib2.iface.instruction.formats.Instruction35c;
import org.jf.dexlib2.iface.reference.*;
import org.jf.dexlib2.immutable.*;
import org.jf.dexlib2.immutable.reference.*;
import org.jf.dexlib2.builder.*;
import org.jf.dexlib2.builder.instruction.*;
import org.jf.dexlib2.dexbacked.*;
import org.jf.dexlib2.writer.io.MemoryDataStore;
import org.jf.dexlib2.writer.pool.DexPool;

/** Exact-firmware, semantic DEX transform. No redistributed firmware bytes. */
public final class PatchServices {
    static final String PIN = "922c443c3a2b609b7f7f3755bbfbeb90f3c0b0c141829f3b6151371c2aa607b6";
    static final String HELPER = "Lcom/android/server/rkdisplay/RkDisplayModeLocale;";
    static final String MODES = "Lcom/android/server/rkdisplay/RkDisplayModes;";
    static final String DIRECTOR = "Lcom/android/server/display/DisplayModeDirector;";
    static final String SPECS = DIRECTOR.substring(0, DIRECTOR.length()-1)+"$DesiredDisplayModeSpecs;";
    static int formats, setters, policies, hdrReaders, hdrWriters, selectionReads;

    static String hash(byte[] bytes) throws Exception {
        StringBuilder s = new StringBuilder();
        for (byte b : MessageDigest.getInstance("SHA-256").digest(bytes)) s.append(String.format(Locale.ROOT,"%02x",b&255));
        return s.toString();
    }
    static byte[] read(InputStream in) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream(); byte[] b = new byte[32768]; int n;
        while ((n=in.read(b))!=-1) out.write(b,0,n); return out.toByteArray();
    }
    static byte[] patch(byte[] bytes) throws Exception {
        Opcodes opcodes = Opcodes.forApi(32);
        DexBackedDexFile dex = new DexBackedDexFile(opcodes,bytes);
        List<ClassDef> classes = new ArrayList<>(); boolean changed = false;
        for (ClassDef c : dex.getClasses()) {
            if (c.getType().equals(HELPER)) throw new IOException("Already patched");
            boolean target = c.getType().equals(MODES) || c.getType().equals("Lcom/android/server/display/DisplayManagerService$DesiredDisplayModeSpecsObserver;");
            if (!target) { classes.add(c); continue; }
            List<Method> methods = new ArrayList<>(); boolean classChanged = false;
            for (Method m : c.getMethods()) {
                if (m.getImplementation()==null) { methods.add(m); continue; }
                MutableMethodImplementation code = new MutableMethodImplementation(m.getImplementation());
                boolean methodChanged = false;
                for (int i=0;i<code.getInstructions().size();i++) {
                    Instruction ins = code.getInstructions().get(i);
                    if (!(ins instanceof Instruction35c)) continue;
                    Instruction35c old = (Instruction35c)ins;
                    if (!(old.getReference() instanceof MethodReference)) continue;
                    MethodReference ref = (MethodReference)old.getReference();
                    ImmutableMethodReference next = null;
                    if (c.getType().equals(MODES) && m.getName().equals("setMode")) {
                        if (ref.getDefiningClass().equals("Ljava/lang/String;") && ref.getName().equals("format") && ref.getParameterTypes().equals(Arrays.asList("Ljava/lang/String;","[Ljava/lang/Object;"))) {
                            next = new ImmutableMethodReference(HELPER,"format",Arrays.asList("Ljava/lang/String;","[Ljava/lang/Object;"),"Ljava/lang/String;"); formats++;
                        } else if (ref.getDefiningClass().equals(MODES) && ref.getName().equals("nativeSetMode")) {
                            next = new ImmutableMethodReference(HELPER,"nativeSetMode",Arrays.asList("I","I","Ljava/lang/String;"),"V"); setters++;
                        }
                    } else if (c.getType().equals(MODES) && m.getName().equals("getCurMode") && ref.getDefiningClass().equals("Landroid/os/SystemProperties;") && ref.getName().equals("get") && ref.getParameterTypes().equals(Arrays.asList("Ljava/lang/String;","Ljava/lang/String;"))) {
                        next = new ImmutableMethodReference(HELPER,"selectionProperty",Arrays.asList("Ljava/lang/String;","Ljava/lang/String;"),"Ljava/lang/String;"); selectionReads++;
                    } else if (c.getType().equals(MODES) && m.getName().equals("isHDR10Status") && ref.getDefiningClass().equals(MODES) && ref.getName().equals("nativeGetModeState")) {
                        next = new ImmutableMethodReference(HELPER,"hdrGetModeState",Collections.singletonList("Ljava/lang/String;"),"Ljava/lang/String;"); hdrReaders++;
                    } else if (c.getType().equals(MODES) && m.getName().equals("setHDR10Enabled") && ref.getDefiningClass().equals(MODES) && ref.getName().equals("nativeSetModeState")) {
                        next = new ImmutableMethodReference(HELPER,"hdrSetModeState",Arrays.asList("Ljava/lang/String;","Ljava/lang/String;"),"I"); hdrWriters++;
                    } else if (ref.getDefiningClass().equals(DIRECTOR) && ref.getName().equals("getDesiredDisplayModeSpecs") && ref.getParameterTypes().equals(Collections.singletonList("I"))) {
                        next = new ImmutableMethodReference(HELPER,"desired",Arrays.asList(DIRECTOR,"I"),SPECS); policies++;
                    }
                    if (next!=null) {
                        code.replaceInstruction(i,new BuilderInstruction35c(Opcode.INVOKE_STATIC,old.getRegisterCount(),old.getRegisterC(),old.getRegisterD(),old.getRegisterE(),old.getRegisterF(),old.getRegisterG(),next));
                        methodChanged = classChanged = changed = true;
                    }
                }
                methods.add(methodChanged ? new ImmutableMethod(m.getDefiningClass(),m.getName(),m.getParameters(),m.getReturnType(),m.getAccessFlags(),m.getAnnotations(),m.getHiddenApiRestrictions(),code) : m);
            }
            classes.add(classChanged ? new ImmutableClassDef(c.getType(),c.getAccessFlags(),c.getSuperclass(),c.getInterfaces(),c.getSourceFile(),c.getAnnotations(),c.getFields(),methods) : c);
        }
        if (!changed) return bytes;
        DexPool pool = new DexPool(opcodes); for (ClassDef c : classes) pool.internClass(c);
        MemoryDataStore store = new MemoryDataStore(); pool.writeTo(store);
        return Arrays.copyOf(store.getBuffer(),store.getSize());
    }
    public static void main(String[] args) throws Exception {
        if (args.length!=3) throw new IllegalArgumentException("original.jar output.jar helper.dex");
        Path input=Paths.get(args[0]), output=Paths.get(args[1]);
        if (!hash(Files.readAllBytes(input)).equals(PIN)) throw new IOException("Firmware services.jar SHA mismatch");
        if (Files.exists(output)) throw new IOException("Output already exists");
        Map<String,byte[]> entries = new LinkedHashMap<>();
        try (ZipFile zip=new ZipFile(input.toFile())) {
            Enumeration<? extends ZipEntry> all=zip.entries();
            while(all.hasMoreElements()) {ZipEntry e=all.nextElement(); if(e.isDirectory())continue;
                byte[] data=read(zip.getInputStream(e)); if(e.getName().matches("classes[0-9]*\\.dex"))data=patch(data);
                entries.put(e.getName(),data);
            }
        }
        if(formats!=4 || setters!=2 || policies!=1 || hdrReaders!=1 || hdrWriters!=1 || selectionReads!=2 || entries.containsKey("classes3.dex")) throw new IOException("Unexpected call sites "+formats+"/"+setters+"/"+policies+"/"+hdrReaders+"/"+hdrWriters+"/"+selectionReads);
        entries.put("classes3.dex",Files.readAllBytes(Paths.get(args[2])));
        try (ZipOutputStream zip=new ZipOutputStream(Files.newOutputStream(output,StandardOpenOption.CREATE_NEW))) {
            for(Map.Entry<String,byte[]> e:entries.entrySet()) {ZipEntry item=new ZipEntry(e.getKey());item.setTime(0);zip.putNextEntry(item);zip.write(e.getValue());zip.closeEntry();}
        }
        System.out.println("PATCHED locale=4 native_delegate=2 desired_specs=1 hdr_get=1 hdr_set=1 selection_read=2; no global locale changes");
    }
}
