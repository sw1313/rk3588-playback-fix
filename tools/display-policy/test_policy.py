"""Compile the real policy adapter with fake Android/server objects on the JVM.
No device settings, native display calls or global device locale are used.
"""
import argparse, subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
FILES={
'android/util/Log.java':'''package android.util; public class Log {public static int i(String a,String b){return 0;}public static int e(String a,String b,Throwable c){return 0;}}''',
'android/util/SparseArray.java':'''package android.util; import java.util.HashMap;public class SparseArray<T> {private final HashMap<Integer,T> m=new HashMap<>();public T get(int i){return m.get(i);}public void put(int i,T v){m.put(i,v);}}''',
'android/view/Display.java':'''package android.view;public class Display {public static class Mode {private int id,w,h;private float r;public Mode(int i,int x,int y,float z){id=i;w=x;h=y;r=z;}public int getModeId(){return id;}public int getPhysicalWidth(){return w;}public int getPhysicalHeight(){return h;}public float getRefreshRate(){return r;}}}''',
'android/os/SystemProperties.java':'''package android.os;public class SystemProperties {public static String mode="Auto";public static final java.util.HashMap<String,String> properties=new java.util.HashMap<>();public static String get(String k,String def){return k.equals("persist.vendor.resolution.HDMI-A-0")?mode:properties.getOrDefault(k,def);}}''',
'com/android/server/display/DisplayModeDirector.java':'''package com.android.server.display;
import android.view.Display;import android.util.SparseArray;
public class DisplayModeDirector {
 public final Object mLock=new Object();public final SparseArray<Display.Mode[]> mSupportedModesByDisplay=new SparseArray<>();
 public int notified;public boolean notifyFail;
 public static class Range {public final float min,max;public Range(float a,float b){min=a;max=b;}}
 public static class DesiredDisplayModeSpecs {public int baseModeId;public boolean allowGroupSwitching;public final Range primaryRefreshRateRange,appRequestRefreshRateRange;public DesiredDisplayModeSpecs(int id,boolean g,Range p,Range a){baseModeId=id;allowGroupSwitching=g;primaryRefreshRateRange=p;appRequestRefreshRateRange=a;}}
 public final DesiredDisplayModeSpecs nativeSpecs=new DesiredDisplayModeSpecs(99,true,new Range(24,120),new Range(24,120));
 public DesiredDisplayModeSpecs getDesiredDisplayModeSpecs(int i){return nativeSpecs;}
 private void notifyDesiredDisplayModeSpecsChangedLocked(){if(!Thread.holdsLock(mLock))throw new AssertionError("missing lock");if(notifyFail)throw new IllegalStateException("injected");notified++;}
}''',
'com/android/server/rkdisplay/RkDisplayModes.java':'''package com.android.server.rkdisplay;import android.os.SystemProperties;public class RkDisplayModes {public static int builtIn=11;public static boolean failBuiltIn;private static int nativeGetBuiltIn(int display){if(failBuiltIn)throw new IllegalStateException("injected");return builtIn;}public static int calls,lastDisplay,lastIface;public static String lastMode;public static boolean fail;public static final java.util.HashMap<String,String> states=new java.util.HashMap<>();public static int stateCalls;public static String failStateKey="";private static String nativeGetModeState(String key){return states.containsKey(key)?states.get(key):"";}private static int nativeSetModeState(String key,String value){stateCalls++;if(key.equals(failStateKey))return -5;states.put(key,value);return 0;}private static void nativeSetMode(int d,int i,String m){if(fail)throw new IllegalStateException("injected native failure");calls++;lastDisplay=d;lastIface=i;lastMode=m;if(d==0)SystemProperties.mode=m;}}''',
'PolicyTest.java':'''import java.util.Locale;import android.view.Display;import android.os.SystemProperties;import com.android.server.display.DisplayModeDirector;import com.android.server.rkdisplay.RkDisplayModeLocale;import com.android.server.rkdisplay.RkDisplayModes;
public class PolicyTest {
 static int cases;static void check(boolean b){if(!b)throw new AssertionError("case "+cases);cases++;}
 public static void main(String[] args){
  Locale saved=Locale.getDefault();try {
   for(String tag:new String[]{"en-US","zh-CN","de-DE","fr-FR","ar-EG","fa-IR"}) {Locale.setDefault(Locale.forLanguageTag(tag));check(RkDisplayModeLocale.format("%.2f",new Object[]{60f}).equals("60.00"));}
   Locale.setDefault(Locale.GERMANY);check(!String.format("%.2f",60f).equals(RkDisplayModeLocale.format("%.2f",new Object[]{60f})));
   DisplayModeDirector d=new DisplayModeDirector();Display.Mode[] modes={new Display.Mode(1,3840,2160,60f),new Display.Mode(2,3840,2160,59.94f),new Display.Mode(3,1920,1080,120f)};d.mSupportedModesByDisplay.put(0,modes);
   SystemProperties.mode="Auto";check(RkDisplayModeLocale.desired(d,0)==d.nativeSpecs);
   SystemProperties.mode="3840x2160@60.00-4016-4104-4400-2168-2178-2250-5-594000";
   DisplayModeDirector.DesiredDisplayModeSpecs s=RkDisplayModeLocale.desired(d,0);check(s.baseModeId==1&&!s.allowGroupSwitching&&s.primaryRefreshRateRange.min==60&&s.primaryRefreshRateRange.max==60&&s.appRequestRefreshRateRange.min==60&&s.appRequestRefreshRateRange.max==60);check(d.nativeSpecs.baseModeId==99&&d.nativeSpecs.primaryRefreshRateRange.min==24);
   SystemProperties.mode="3840x2160@59.94-1";s=RkDisplayModeLocale.desired(d,0);check(s.baseModeId==2&&Math.abs(s.primaryRefreshRateRange.min-59.94f)<0.001);
   SystemProperties.mode="3840x2160@49.00-1";check(RkDisplayModeLocale.desired(d,0)==d.nativeSpecs);
   SystemProperties.mode="invalid";check(RkDisplayModeLocale.desired(d,0)==d.nativeSpecs);
   SystemProperties.mode="3840x2160@60.00-1";check(RkDisplayModeLocale.desired(d,1)==d.nativeSpecs);
   d.mSupportedModesByDisplay.put(0,new Display.Mode[]{modes[0],new Display.Mode(5,3840,2160,60f)});check(RkDisplayModeLocale.desired(d,0)==d.nativeSpecs);d.mSupportedModesByDisplay.put(0,modes);
   RkDisplayModeLocale.nativeSetMode(0,11,"Auto");check(RkDisplayModes.calls==1&&RkDisplayModes.lastDisplay==0&&RkDisplayModes.lastIface==11&&RkDisplayModes.lastMode.equals("Auto")&&d.notified==1);check(RkDisplayModeLocale.desired(d,0)==d.nativeSpecs);
   d.notifyFail=true;RkDisplayModeLocale.nativeSetMode(0,11,"Auto");check(RkDisplayModes.calls==2);d.notifyFail=false;
   RkDisplayModes.fail=true;boolean failed=false;try {RkDisplayModeLocale.nativeSetMode(0,11,"Auto");}catch(IllegalStateException expected){failed=true;}check(failed&&RkDisplayModes.calls==2);
   String h="vendor.hwc.hdr_force_disable", savedHdr="persist.vendor.rkdisplay.hdr_force_disable";
   check(RkDisplayModeLocale.hdrGetModeState(h).equals("0"));
   RkDisplayModes.states.put(h,"1");check(RkDisplayModeLocale.hdrGetModeState(h).equals("1"));
   RkDisplayModes.states.put(h,"0");check(RkDisplayModeLocale.hdrGetModeState(h).equals("0"));
   RkDisplayModes.states.put(h,"invalid");check(RkDisplayModeLocale.hdrGetModeState(h).equals("invalid"));
   check(RkDisplayModeLocale.hdrGetModeState("other.key").equals(""));
   check(RkDisplayModeLocale.hdrSetModeState(h,"1")==0&&RkDisplayModes.states.get(h).equals("1")&&RkDisplayModes.states.get(savedHdr).equals("1"));
   check(RkDisplayModeLocale.hdrSetModeState(h,"0")==0&&RkDisplayModes.states.get(savedHdr).equals("0"));
   RkDisplayModes.failStateKey=h;int priorCalls=RkDisplayModes.stateCalls;
   check(RkDisplayModeLocale.hdrSetModeState(h,"1")==-5&&RkDisplayModes.stateCalls==priorCalls+1&&RkDisplayModes.states.get(savedHdr).equals("0"));
   RkDisplayModes.failStateKey=savedHdr;check(RkDisplayModeLocale.hdrSetModeState(h,"1")==-5&&RkDisplayModes.states.get(h).equals("1")&&RkDisplayModes.states.get(savedHdr).equals("0"));
   RkDisplayModes.failStateKey="";priorCalls=RkDisplayModes.stateCalls;
   check(RkDisplayModeLocale.hdrSetModeState(h,"2")==0&&RkDisplayModes.stateCalls==priorCalls+1&&RkDisplayModes.states.get(savedHdr).equals("0"));
   priorCalls=RkDisplayModes.stateCalls;check(RkDisplayModeLocale.hdrSetModeState("other.key","1")==0&&RkDisplayModes.stateCalls==priorCalls+1);
   String mainKey="persist.vendor.resolution.main", auxKey="persist.vendor.resolution.aux";
   SystemProperties.properties.put(mainKey,"old-mode");SystemProperties.mode="Auto";
   check(RkDisplayModeLocale.selectionProperty(mainKey,"NULL").equals("Auto"));
   SystemProperties.mode="3840x2160@60.00-4016-4104-4400-2168-2178-2250-5-594000";
   check(RkDisplayModeLocale.selectionProperty(mainKey,"NULL").equals(SystemProperties.mode));
   RkDisplayModes.builtIn=10;check(RkDisplayModeLocale.selectionProperty(mainKey,"NULL").equals("old-mode"));RkDisplayModes.builtIn=11;
   SystemProperties.properties.put(auxKey,"secondary");check(RkDisplayModeLocale.selectionProperty(auxKey,"NULL").equals("secondary"));
   check(RkDisplayModeLocale.selectionProperty("unknown.key","fallback").equals("fallback"));
   SystemProperties.mode="invalid";check(RkDisplayModeLocale.selectionProperty(mainKey,"NULL").equals("old-mode"));
   SystemProperties.mode="";check(RkDisplayModeLocale.selectionProperty(mainKey,"NULL").equals("old-mode"));
   SystemProperties.mode="Auto";RkDisplayModes.failBuiltIn=true;check(RkDisplayModeLocale.selectionProperty(mainKey,"NULL").equals("old-mode"));RkDisplayModes.failBuiltIn=false;
   check(SystemProperties.properties.get(mainKey).equals("old-mode")&&SystemProperties.mode.equals("Auto"));
   System.out.println("PASS policy cases="+cases+"; fake Android objects; no device IO");
  }finally{Locale.setDefault(saved);}
 }
}'''
}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--work',type=Path,required=True);a=p.parse_args()
    a.work.mkdir(parents=True,exist_ok=False);sources=[]
    for name,text in FILES.items():
        f=a.work/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text(text,encoding='utf-8');sources.append(f)
    classes=a.work/'classes';classes.mkdir()
    sources.append(ROOT/'magisk-display-policy/src/RkDisplayModeLocale.java')
    subprocess.run(['javac','--release','8','-d',str(classes),*map(str,sources)],check=True)
    r=subprocess.run(['java','-cp',str(classes),'PolicyTest'],capture_output=True,text=True)
    (a.work/'RUN.txt').write_text(r.stdout+r.stderr,encoding='utf-8');print(r.stdout,r.stderr);r.check_returncode()

if __name__=='__main__':main()
