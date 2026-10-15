package com.rk3588.lifecycle;
import android.app.Activity;
import android.os.*;
import android.media.*;
import android.widget.TextView;
import org.json.*;
import java.io.*;
import java.nio.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
public class AsyncActivity extends Activity {
 public void onCreate(Bundle b){super.onCreate(b);TextView t=new TextView(this);t.setText("4K asynchronous lifecycle regression");setContentView(t);new Thread(()->runTests()).start();}
 void runTests(){JSONObject report=new JSONObject();JSONArray rows=new JSONArray();String name=getIntent().getStringExtra("case");if(name==null)name="async";
  try{String codec=getIntent().getStringExtra("codec");if(codec==null)codec="OMX.c2.rk.avc.decoder";String clip=getIntent().getStringExtra("clip");int rounds=getIntent().getIntExtra("rounds",20);
   for(int r=0;r<rounds;r++){
    final AtomicBoolean stopping=new AtomicBoolean(false),end=new AtomicBoolean(false);final AtomicInteger frames=new AtomicInteger(),queued=new AtomicInteger();final CountDownLatch ready=new CountDownLatch(1);final StringBuilder errors=new StringBuilder();
    MediaExtractor ex=new MediaExtractor();ex.setDataSource(clip);MediaFormat f=null;for(int k=0;k<ex.getTrackCount();k++){MediaFormat a=ex.getTrackFormat(k);if(a.getString("mime").startsWith("video/")){ex.selectTrack(k);f=a;break;}}
    f.setInteger("color-format",0x7f420888);f.setInteger("operating-rate",32767);f.setInteger("priority",1);f.setInteger("latency",0);
    MediaCodec c=MediaCodec.createByCodecName(codec);HandlerThread ht=new HandlerThread("AsyncProbeCallback");ht.start();
    c.setCallback(new MediaCodec.Callback(){
     public void onInputBufferAvailable(MediaCodec mc,int ix){if(stopping.get()||end.get())return;try{ByteBuffer in=mc.getInputBuffer(ix);int n=ex.readSampleData(in,0);long pts=ex.getSampleTime();if(n<0){end.set(true);mc.queueInputBuffer(ix,0,0,0,4);}else{mc.queueInputBuffer(ix,0,n,pts,0);queued.incrementAndGet();ex.advance();}}catch(Exception e){if(!stopping.get())errors.append(e.toString());ready.countDown();}}
     public void onOutputBufferAvailable(MediaCodec mc,int ix,MediaCodec.BufferInfo info){if(stopping.get())return;try{if(info.size>0&&frames.incrementAndGet()>=6)ready.countDown();mc.releaseOutputBuffer(ix,false);}catch(Exception e){if(!stopping.get())errors.append(e.toString());ready.countDown();}}
     public void onOutputFormatChanged(MediaCodec mc,MediaFormat f){}
     public void onError(MediaCodec mc,MediaCodec.CodecException e){errors.append(e.getDiagnosticInfo());ready.countDown();}
    },new Handler(ht.getLooper()));
    c.configure(f,null,null,0);c.start();boolean got=ready.await(5,TimeUnit.SECONDS);Thread.sleep((r%11)*3);stopping.set(true);
    long start=System.nanoTime();c.stop();c.release();ht.quitSafely();ht.join(2000);ex.release();
    JSONObject row=new JSONObject();row.put("round",r);row.put("frames",frames.get());row.put("queued",queued.get());row.put("stopMs",(System.nanoTime()-start)/1000000);row.put("error",errors.toString());row.put("pass",got&&frames.get()>=6&&errors.length()==0);rows.put(row);
    if(!row.getBoolean("pass"))throw new RuntimeException(row.toString());
   }
   report.put("pass",true);
  }catch(Throwable e){try{report.put("pass",false);report.put("error",e.toString());}catch(Exception ignored){}}
  try{report.put("rounds",rows);File file=new File(getExternalFilesDir(null),name+".json");try(FileOutputStream o=new FileOutputStream(file)){o.write(report.toString(2).getBytes("UTF-8"));}android.util.Log.i("RKLifecycle",report.toString());}catch(Exception e){android.util.Log.e("RKLifecycle","report",e);}runOnUiThread(()->finish());
 }
}
