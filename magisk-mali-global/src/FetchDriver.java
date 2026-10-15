import java.io.*;
import java.net.*;
import java.security.*;
import javax.net.ssl.HttpsURLConnection;
public final class FetchDriver {
 static String hex(byte[] data){StringBuilder b=new StringBuilder();for(byte v:data)b.append(String.format("%02x",v&255));return b.toString();}
 public static void main(String[] args)throws Exception{
  if(args.length!=4)throw new IllegalArgumentException("URL destination SHA256 bytes");
  URL url=new URL(args[0]);
  if(!"https".equals(url.getProtocol())||!"raw.githubusercontent.com".equals(url.getHost())||!url.getPath().startsWith("/khadas/android_vendor_rockchip_common/"))throw new SecurityException("Unexpected upstream source");
  long expectedSize=Long.parseLong(args[3]);if(expectedSize<=0||expectedSize>100000000)throw new IOException("Unexpected driver size");
  File dest=new File(args[1]),temp=new File(args[1]+".part");
  HttpsURLConnection c=(HttpsURLConnection)url.openConnection();c.setInstanceFollowRedirects(false);c.setConnectTimeout(20000);c.setReadTimeout(30000);c.setRequestProperty("User-Agent","RK3588-Mali-Installer/0.2");
  if(c.getResponseCode()!=200)throw new IOException("Upstream HTTP status "+c.getResponseCode());
  MessageDigest digest=MessageDigest.getInstance("SHA-256");long size=0;
  try(InputStream in=c.getInputStream();FileOutputStream out=new FileOutputStream(temp)){
   byte[] buf=new byte[131072];int n;while((n=in.read(buf))!=-1){size+=n;if(size>expectedSize)throw new IOException("Driver exceeds pinned size");digest.update(buf,0,n);out.write(buf,0,n);}out.getFD().sync();
  }finally{c.disconnect();}
  if(size!=expectedSize||!hex(digest.digest()).equals(args[2])){temp.delete();throw new IOException("Driver integrity differs from pinned upstream");}
  if(!temp.renameTo(dest)){temp.delete();throw new IOException("Cannot finalize driver download");}
  System.out.println("Pinned upstream driver fetched and SHA256 verified");
 }
}
