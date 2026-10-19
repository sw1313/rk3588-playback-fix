// SPDX-License-Identifier: MIT
package com.rk3588.exomodetest;
import android.app.Activity;
import android.os.*;
import android.view.*;
import android.graphics.Bitmap;
import com.google.android.exoplayer2.*;
import com.google.android.exoplayer2.decoder.DecoderCounters;
import org.json.*;
import java.io.*;

/** Real ExoPlayer with bounded window mode votes. Never edits system settings. */
public class MainActivity extends Activity {
 private ExoPlayer player;private final Handler handler=new Handler(Looper.getMainLooper());
 private final JSONArray phases=new JSONArray();private final JSONObject report=new JSONObject();
 private int index,firstFrames;private String label;private boolean stopped;
 private SurfaceView video;
 private int[] modeIds;
 private final int[] rates={24,60,30,60,24,60};
 @Override public void onCreate(Bundle b){super.onCreate(b);getWindow().addFlags(128);
  label=getIntent().getStringExtra("case");if(label==null||!label.matches("[A-Za-z0-9_-]+")){finish();return;}
  String requestedModes=getIntent().getStringExtra("modes");
  if(requestedModes!=null){
   if(!requestedModes.matches("[0-9]+(,[0-9]+){1,19}")){finish();return;}
   String[] values=requestedModes.split(",");modeIds=new int[values.length];
   for(int i=0;i<values.length;i++)modeIds[i]=Integer.parseInt(values[i]);
  }
  video=new SurfaceView(this);setContentView(video);
  player=new ExoPlayer.Builder(this).build();player.setVideoSurfaceView(video);
  player.addListener(new Player.Listener(){
   @Override public void onRenderedFirstFrame(){firstFrames++;}
   @Override public void onPlayerError(PlaybackException e){try{report.put("error",e.getErrorCodeName());save();}catch(Exception ignored){}}
  });
  String media=getIntent().getStringExtra("media");if(media==null){finish();return;}
  player.setMediaItem(MediaItem.fromUri(media));player.setRepeatMode(Player.REPEAT_MODE_ONE);player.prepare();player.play();handler.postDelayed(this::vote,2000);
 }
 private void vote(){if(stopped)return;
  if(index==(modeIds==null?rates.length:modeIds.length)){try{report.put("complete",true);report.put("first_frames",firstFrames);}catch(Exception ignored){}save();restore();return;}
  Display.Mode current=getDisplay().getMode(),target=null;
  for(Display.Mode m:getDisplay().getSupportedModes()){
   if(modeIds!=null?m.getModeId()==modeIds[index]:m.getPhysicalWidth()==current.getPhysicalWidth()&&m.getPhysicalHeight()==current.getPhysicalHeight()&&Math.abs(m.getRefreshRate()-rates[index])<.1f){target=m;break;}
  }
  if(target==null){try{report.put("error","MODE_UNAVAILABLE");}catch(Exception ignored){}save();return;}
  final int desired=target.getModeId(),width=target.getPhysicalWidth(),height=target.getPhysicalHeight();final float fps=target.getRefreshRate();WindowManager.LayoutParams a=getWindow().getAttributes();a.preferredDisplayModeId=desired;getWindow().setAttributes(a);
  handler.postDelayed(()->{try{
   JSONObject row=new JSONObject();Display.Mode actual=getDisplay().getMode();row.put("requested_fps",fps);row.put("desired_mode",desired);row.put("requested_width",width);row.put("requested_height",height);row.put("actual_width",actual.getPhysicalWidth());row.put("actual_height",actual.getPhysicalHeight());row.put("actual_mode",actual.getModeId());row.put("actual_fps",actual.getRefreshRate());row.put("playing",player.isPlaying());row.put("position_ms",player.getCurrentPosition());
   DecoderCounters c=player.getVideoDecoderCounters();if(c!=null){c.ensureUpdated();row.put("rendered",c.renderedOutputBufferCount);row.put("dropped",c.droppedBufferCount);}
   captureFrame(row);
  }catch(Exception e){try{report.put("error",e.getClass().getSimpleName());}catch(Exception ignored){}save();}},5500);
 }
 private void captureFrame(JSONObject row){
  Bitmap image=Bitmap.createBitmap(160,90,Bitmap.Config.ARGB_8888);
  try {PixelCopy.request(video,image,result->{try{
   row.put("pixel_copy_result",result);
   if(result==PixelCopy.SUCCESS){long sum=0,squares=0;int bright=0;
    for(int y=0;y<image.getHeight();y++)for(int x=0;x<image.getWidth();x++){
     int p=image.getPixel(x,y),v=Math.max((p>>>16)&255,Math.max((p>>>8)&255,p&255));sum+=v;squares+=(long)v*v;if(v>24)bright++;
    }
    int count=image.getWidth()*image.getHeight();row.put("sample_count",count);row.put("bright_pixels",bright);
    row.put("mean_peak_channel",(double)sum/count);row.put("variance_peak_channel",(double)squares/count-Math.pow((double)sum/count,2));
   }
   phases.put(row);report.put("phases",phases);save();index++;
   handler.postDelayed(this::vote,1500);
  }catch(Exception e){fail(e);}finally{image.recycle();}},handler);
  }catch(Exception e){image.recycle();fail(e);}
 }
 private void fail(Exception e){try{report.put("error",e.getClass().getSimpleName());}catch(Exception ignored){}save();}
 private void restore(){WindowManager.LayoutParams a=getWindow().getAttributes();a.preferredDisplayModeId=0;getWindow().setAttributes(a);}
 private void save(){try(FileOutputStream f=new FileOutputStream(new File(getExternalFilesDir(null),label+".json"))){f.write(report.toString(2).getBytes("UTF-8"));}catch(Exception ignored){}}
 @Override public void onDestroy(){stopped=true;handler.removeCallbacksAndMessages(null);restore();if(player!=null)player.release();super.onDestroy();}
}
