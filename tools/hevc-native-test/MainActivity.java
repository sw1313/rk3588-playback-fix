// SPDX-License-Identifier: MIT
// Isolated late-profile HEVC display/HDR pause-seek acceptance fixture.
package com.rk3588.nativehdrtest;
import android.app.Activity;
import android.os.*;
import android.media.*;
import android.graphics.Bitmap;
import android.view.*;
import android.widget.*;
import android.util.Log;
import java.io.*;
import java.nio.ByteBuffer;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import org.json.*;
import android.hardware.display.*;
public class MainActivity extends Activity implements SurfaceHolder.Callback {
 private SurfaceView view; private TextView status; private volatile boolean cancelled;
 private final Handler ui=new Handler(Looper.getMainLooper());
 private ImageReader virtualReader;private VirtualDisplay virtualDisplay;private android.app.Presentation presentation;
 private volatile int virtualFrames=0,virtualR=-1,virtualG=-1,virtualB=-1;
 private void startVirtualProbe(){
  virtualReader=ImageReader.newInstance(640,360,android.graphics.PixelFormat.RGBA_8888,2);
  virtualReader.setOnImageAvailableListener(reader->{try(Image image=reader.acquireLatestImage()){
   if(image!=null){Image.Plane plane=image.getPlanes()[0];ByteBuffer b=plane.getBuffer();int offset=32*plane.getRowStride()+32*plane.getPixelStride();virtualR=b.get(offset)&255;virtualG=b.get(offset+1)&255;virtualB=b.get(offset+2)&255;virtualFrames++;}
  }},ui);
  virtualDisplay=((DisplayManager)getSystemService(DISPLAY_SERVICE)).createVirtualDisplay("HdrCompatProbe",640,360,160,virtualReader.getSurface(),DisplayManager.VIRTUAL_DISPLAY_FLAG_OWN_CONTENT_ONLY|DisplayManager.VIRTUAL_DISPLAY_FLAG_PRESENTATION);
  presentation=new android.app.Presentation(this,virtualDisplay.getDisplay());TextView text=new TextView(presentation.getContext());text.setBackgroundColor(android.graphics.Color.rgb(160,32,96));text.setTextColor(-1);text.setText("SDR virtual compatibility");text.setGravity(android.view.Gravity.CENTER);presentation.setContentView(text);presentation.show();
 }
 protected void onCreate(Bundle state){super.onCreate(state);getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON); getWindow().getDecorView().setSystemUiVisibility(5894);
  android.widget.FrameLayout layout=new android.widget.FrameLayout(this);view=new SurfaceView(this);layout.addView(view,new FrameLayout.LayoutParams(-1,-1));status=new TextView(this);status.setTextColor(-1);status.setTextSize(18);layout.addView(status,new FrameLayout.LayoutParams(-2,-2));setContentView(layout);view.getHolder().addCallback(this);if(getIntent().getBooleanExtra("virtual_probe",false))startVirtualProbe();
 }
 public void surfaceCreated(SurfaceHolder holder){new Thread(()->playlist(holder.getSurface()),"decoder-test").start();}
 public void surfaceChanged(SurfaceHolder h,int f,int w,int he){} public void surfaceDestroyed(SurfaceHolder h){cancelled=true;}
 protected void onDestroy(){cancelled=true;if(presentation!=null)presentation.dismiss();if(virtualDisplay!=null)virtualDisplay.release();if(virtualReader!=null)virtualReader.close();super.onDestroy();}
 private void note(String text){Log.i("RKDecoderTest",text);ui.post(()->status.setText(text));}
 private String staticInfo(MediaFormat f){return bytes(f,"hdr-static-info");}
 private String bytes(MediaFormat f,String key){if(!f.containsKey(key))return "";ByteBuffer b=f.getByteBuffer(key).duplicate();StringBuilder out=new StringBuilder();while(b.hasRemaining())out.append(String.format("%02x",b.get()&255));return out.toString();}
 private JSONObject fmt(MediaFormat f)throws Exception{JSONObject j=new JSONObject();String[] keys={"width","height","stride","slice-height","color-format","color-standard","color-transfer","color-range"};for(String k:keys)if(f.containsKey(k))j.put(k,f.getInteger(k));j.put("hdr-static-info",staticInfo(f));j.put("text",f.toString());return j;}
 private void save(JSONObject j,String name){try{File dir=getExternalFilesDir(null);try(FileOutputStream f=new FileOutputStream(new File(dir,name+".json"))){f.write(j.toString(2).getBytes("UTF-8"));}}catch(Exception e){Log.e("RKDecoderTest","report",e);}}
 private JSONObject screenshot(String name)throws Exception{
  JSONObject j=new JSONObject();CountDownLatch done=new CountDownLatch(1);Bitmap image=Bitmap.createBitmap(1280,720,Bitmap.Config.ARGB_8888);
  ui.post(()->PixelCopy.request(view,image,result->{try{j.put("pixelcopy",result);if(result==0){long min=255,max=0,sum=0,nonzero=0;int green=0;for(int y=0;y<720;y+=2)for(int x=0;x<1280;x+=2){int c=image.getPixel(x,y),r=(c>>16)&255,g=(c>>8)&255,b=c&255;int l=(r+g+b)/3;min=Math.min(min,l);max=Math.max(max,l);sum+=l;if(l!=0)nonzero++;if(g>r+40&&g>b+40)green++;}j.put("min",min);j.put("max",max);j.put("sum",sum);j.put("nonzero",nonzero);j.put("green_pixels",green);try(FileOutputStream out=new FileOutputStream(new File(getExternalFilesDir(null),name+".png"))){image.compress(Bitmap.CompressFormat.PNG,100,out);}}}catch(Exception e){try{j.put("error",e.toString());}catch(Exception ignored){}}finally{done.countDown();}},ui));
  if(!done.await(3,TimeUnit.SECONDS))j.put("timeout",true);return j;
 }
 private void playlist(Surface surface){String paths=getIntent().getStringExtra("playlist");if(paths==null){test(surface);return;}String root=getIntent().getStringExtra("case");int index=0;for(String path:paths.split(",")){if(cancelled)break;getIntent().putExtra("clip",path);getIntent().putExtra("case",root+"-"+index++);test(surface);}}
 private void test(Surface surface){String clip=getIntent().getStringExtra("clip"),codecName=getIntent().getStringExtra("codec"),name=getIntent().getStringExtra("case");if(name==null)name="test";
  try{try(FileOutputStream phase=new FileOutputStream(new File(getExternalFilesDir(null),"phase.txt"))){phase.write(name.getBytes("UTF-8"));}}catch(Exception ignored){}
  JSONObject report=new JSONObject();JSONArray perFrame=new JSONArray();boolean shortTest=getIntent().getBooleanExtra("short",false);MediaExtractor ex=new MediaExtractor();MediaCodec codec=null;int frames=0,afterSeek=0;boolean paused=false,sought=false,eos=false;
  try{report.put("case",name);report.put("codec",codecName);report.put("clip",new File(clip).getName());ex.setDataSource(clip);int track=0;while(track<ex.getTrackCount()&&!ex.getTrackFormat(track).getString("mime").startsWith("video/"))track++;if(track>=ex.getTrackCount())throw new IOException("No video track");ex.selectTrack(track);MediaFormat input=ex.getTrackFormat(track);report.put("input",fmt(input));note("START "+name+" "+codecName);codec=MediaCodec.createByCodecName(codecName);input.removeKey(MediaFormat.KEY_PROFILE);String key=null;for(String k:codec.getSupportedVendorParameters())if(k.contains("fbc-disable")){if(key!=null)throw new IllegalStateException("ambiguous FBC key");key=k;}if(key==null)throw new IllegalStateException("missing FBC key");input.setInteger(key,1);report.put("configured_profile_omitted",true);report.put("linear_request_key",key);if(getIntent().getBooleanExtra("explicit_p010",false)){input.setInteger(MediaFormat.KEY_COLOR_FORMAT,54);report.put("explicit_p010",true);}codec.configure(input,surface,null,0);codec.start();
   MediaCodec.BufferInfo info=new MediaCodec.BufferInfo();boolean inputEos=false;long origin=ex.getSampleTime(),anchor=System.nanoTime(),deadline=System.nanoTime()+15000000000L;
   while(!cancelled&&!eos&&System.nanoTime()<deadline){
    if(!inputEos){int in=codec.dequeueInputBuffer(1000);if(in>=0){ByteBuffer buf=codec.getInputBuffer(in);int size=ex.readSampleData(buf,0);long pts=ex.getSampleTime();if(size<0){size=0;pts=0;inputEos=true;}codec.queueInputBuffer(in,0,size,pts,inputEos?MediaCodec.BUFFER_FLAG_END_OF_STREAM:0);if(!inputEos)ex.advance();}}
    int out=codec.dequeueOutputBuffer(info,1000);if(out==MediaCodec.INFO_OUTPUT_FORMAT_CHANGED){MediaFormat f=codec.getOutputFormat();report.put("output",fmt(f));note("FORMAT "+name+" "+f);}else if(out>=0){
     eos=(info.flags&MediaCodec.BUFFER_FLAG_END_OF_STREAM)!=0;
     if(!eos || info.size>0){MediaFormat frameFormat=codec.getOutputFormat(out);JSONObject frame=new JSONObject();frame.put("pts",info.presentationTimeUs);frame.put("plus",bytes(frameFormat,"hdr10-plus-info"));frame.put("static",staticInfo(frameFormat));perFrame.put(frame);}
     if((info.flags&MediaCodec.BUFFER_FLAG_CODEC_CONFIG)==0 && (info.size>0||!eos)){long target=anchor+(info.presentationTimeUs-origin)*1000L;long wait=target-System.nanoTime();if(wait>0&&wait<2000000000L){Thread.sleep(wait/1000000L,(int)(wait%1000000L));}codec.releaseOutputBuffer(out,true);frames++;afterSeek++;}
     else codec.releaseOutputBuffer(out,false);
     if(!shortTest&&!paused&&frames>=20){long begin=System.nanoTime();Thread.sleep(250);anchor+=System.nanoTime()-begin;paused=true;note("PAUSE_RESUME "+name);}
     if(!shortTest&&!sought&&frames>=40&&!eos){codec.flush();ex.seekTo(1000000L,MediaExtractor.SEEK_TO_PREVIOUS_SYNC);origin=ex.getSampleTime();anchor=System.nanoTime();inputEos=false;sought=true;afterSeek=0;note("SEEK_FLUSH "+name);}
    }
   }
   report.put("per_frame",perFrame);report.put("frames",frames);report.put("after_seek_frames",afterSeek);report.put("paused",paused);report.put("sought",sought);report.put("eos",eos);Thread.sleep(150);report.put("capture",screenshot(name));report.put("pass",!cancelled&&eos&&(shortTest?frames>=10:frames>=60&&afterSeek>=20&&paused&&sought));note("RESULT "+name+" "+report.getBoolean("pass")+" frames="+frames+" eos="+eos);
  }catch(Throwable e){try{report.put("pass",false);report.put("error",e.toString());}catch(Exception ignored){}Log.e("RKDecoderTest","FAIL "+name,e);note("FAIL "+name+" "+e);}
  finally{if(codec!=null){try{codec.stop();}catch(Exception ignored){}try{codec.release();}catch(Exception ignored){}}ex.release();try{if(virtualReader!=null){JSONObject virtual=new JSONObject();virtual.put("frames",virtualFrames);virtual.put("red",virtualR);virtual.put("green",virtualG);virtual.put("blue",virtualB);virtual.put("passed",virtualFrames>0&&virtualR==160&&virtualG==32&&virtualB==96);report.put("SDR_virtual_display",virtual);}}catch(Exception ignored){}save(report,name);}
 }
}
