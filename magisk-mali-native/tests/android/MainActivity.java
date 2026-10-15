package com.sw1313.malitest;
import android.app.Activity;
import android.os.Bundle;
import android.opengl.GLSurfaceView;
import android.opengl.GLES30;
import android.widget.LinearLayout;
import android.widget.TextView;
import org.json.JSONObject;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;
public class MainActivity extends Activity {
 static {System.loadLibrary("rkmalitest");}
 static native int checkVulkan(String log);
 GLSurfaceView view;TextView text;volatile int vk=-1;String renderer="",version="";int frames=0,errors=0,program=0;long start;
 @Override public void onCreate(Bundle b){super.onCreate(b);LinearLayout layout=new LinearLayout(this);layout.setOrientation(1);text=new TextView(this);layout.addView(text);view=new GLSurfaceView(this);view.setEGLContextClientVersion(3);layout.addView(view,new LinearLayout.LayoutParams(-1,0,1));setContentView(layout);start=System.nanoTime();
 view.setRenderer(new GLSurfaceView.Renderer(){
  int shader(int type,String code){int s=GLES30.glCreateShader(type);GLES30.glShaderSource(s,code);GLES30.glCompileShader(s);int[] ok={0};GLES30.glGetShaderiv(s,GLES30.GL_COMPILE_STATUS,ok,0);if(ok[0]!=1)throw new RuntimeException(GLES30.glGetShaderInfoLog(s));return s;}
  public void onSurfaceCreated(GL10 unused,EGLConfig config){renderer=GLES30.glGetString(GLES30.GL_RENDERER);version=GLES30.glGetString(GLES30.GL_VERSION);program=GLES30.glCreateProgram();GLES30.glAttachShader(program,shader(GLES30.GL_VERTEX_SHADER,"#version 300 es\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0,1);}"));GLES30.glAttachShader(program,shader(GLES30.GL_FRAGMENT_SHADER,"#version 300 es\nprecision highp float;out vec4 c;void main(){c=vec4(0,1,0,1);}"));GLES30.glLinkProgram(program);int[] ok={0};GLES30.glGetProgramiv(program,GLES30.GL_LINK_STATUS,ok,0);if(ok[0]!=1)throw new RuntimeException(GLES30.glGetProgramInfoLog(program));}
  public void onSurfaceChanged(GL10 unused,int width,int height){GLES30.glViewport(0,0,width,height);}
  public void onDrawFrame(GL10 unused){GLES30.glUseProgram(program);GLES30.glDrawArrays(GLES30.GL_TRIANGLES,0,3);frames++;if(frames%60==0){ByteBuffer pixel=ByteBuffer.allocateDirect(4);GLES30.glReadPixels(32,32,1,1,GLES30.GL_RGBA,GLES30.GL_UNSIGNED_BYTE,pixel);if(pixel.get(0)!=0||(pixel.get(1)&255)!=255||pixel.get(2)!=0||(pixel.get(3)&255)!=255||GLES30.glGetError()!=0)errors++;writeReport();}}
 });new Thread(()->{vk=checkVulkan(new File(getFilesDir(),"vulkan.txt").getAbsolutePath());writeReport();}).start();}
 synchronized void writeReport(){try{JSONObject j=new JSONObject();j.put("renderer",renderer);j.put("version",version);j.put("frames",frames);j.put("pixelErrors",errors);j.put("vulkanCode",vk);j.put("elapsedSeconds",(System.nanoTime()-start)/1e9);j.put("pass",frames>=120&&errors==0&&vk==0);try(FileOutputStream out=new FileOutputStream(new File(getFilesDir(),"report.json"))){out.write(j.toString(2).getBytes("UTF-8"));}runOnUiThread(()->text.setText(version+" | frames "+frames+" | errors "+errors+" | Vulkan "+vk));}catch(Exception e){throw new RuntimeException(e);}}
 @Override protected void onPause(){super.onPause();view.onPause();}
 @Override protected void onResume(){super.onResume();if(view!=null)view.onResume();}
}
