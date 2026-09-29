package itz.glace.deskhalo;
import android.app.Activity;
import android.os.Bundle;
import android.content.*;
import android.provider.*;
import android.net.Uri;
import android.widget.*;
import android.database.Cursor;
import org.json.*;
import java.net.*;
import java.io.*;
import java.util.concurrent.*;

public class TransferActivity extends Activity {
    private String base,code;private TextView status;private LinearLayout files;
    private final ExecutorService worker=Executors.newSingleThreadExecutor();
    @Override public void onCreate(Bundle state){super.onCreate(state);base=getIntent().getStringExtra("base");code=getIntent().getStringExtra("code");if(base==null||code==null){finish();return;}
        LinearLayout root=new LinearLayout(this);root.setOrientation(1);root.setPadding(36,28,36,28);root.setBackgroundColor(0xff14161b);
        TextView title=new TextView(this);title.setText("DeskHalo · Files");title.setTextSize(30);title.setTextColor(0xffffbd45);root.addView(title);
        status=new TextView(this);status.setTextColor(0xffeeeeee);status.setText("PC → Quest: put files in Documents / DeskHalo Transfers / To Quest.\nQuest downloads are saved in Download / DeskHalo.");root.addView(status);
        Button send=new Button(this);send.setText("Send a Quest file to PC");send.setOnClickListener(v->startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("*/*").addCategory(Intent.CATEGORY_OPENABLE),10));root.addView(send);
        Button refresh=new Button(this);refresh.setText("Refresh PC files");refresh.setOnClickListener(v->refresh());root.addView(refresh);
        Button back=new Button(this);back.setText("Return to workspace");back.setOnClickListener(v->finish());root.addView(back);
        files=new LinearLayout(this);files.setOrientation(1);root.addView(files);ScrollView scroll=new ScrollView(this);scroll.addView(root);setContentView(scroll);refresh();
    }
    private void status(String value){runOnUiThread(()->{if(!isFinishing())status.setText(value);});}
    private HttpURLConnection open(String path)throws Exception{HttpURLConnection c=(HttpURLConnection)new URL(base+path).openConnection();c.setRequestProperty("X-DeskHalo-Code",code);c.setConnectTimeout(3000);c.setReadTimeout(15000);return c;}
    private static String encode(String text)throws Exception{return URLEncoder.encode(text,"UTF-8");}
    private void refresh(){worker.execute(()->{HttpURLConnection c=null;try{c=open("/files");if(c.getResponseCode()!=200)throw new IOException("Host response "+c.getResponseCode());ByteArrayOutputStream out=new ByteArrayOutputStream();try(InputStream in=c.getInputStream()){copy(in,out,1024*1024);}JSONArray list=new JSONArray(out.toString("UTF-8"));runOnUiThread(()->{files.removeAllViews();for(int i=0;i<list.length();i++){JSONObject f=list.optJSONObject(i);if(f==null)continue;String name=f.optString("name");Button button=new Button(this);button.setText("Download · "+name);button.setOnClickListener(v->download(name));files.addView(button);}});}catch(Exception e){status(e.getMessage());}finally{if(c!=null)c.disconnect();}});}
    private void download(String name){worker.execute(()->{HttpURLConnection c=null;Uri uri=null;try{if(name.contains("/")||name.contains("\\")||name.length()>180)throw new IOException("Invalid filename");status("Downloading "+name);c=open("/files/download?name="+encode(name));if(c.getResponseCode()!=200)throw new IOException("Download response "+c.getResponseCode());ContentValues values=new ContentValues();values.put(MediaStore.Downloads.DISPLAY_NAME,name);values.put(MediaStore.Downloads.MIME_TYPE,"application/octet-stream");values.put(MediaStore.Downloads.RELATIVE_PATH,"Download/DeskHalo");values.put(MediaStore.Downloads.IS_PENDING,1);uri=getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI,values);if(uri==null)throw new IOException("Cannot create download");try(InputStream in=c.getInputStream();OutputStream out=getContentResolver().openOutputStream(uri)){copy(in,out,2L*1024*1024*1024);}values.clear();values.put(MediaStore.Downloads.IS_PENDING,0);getContentResolver().update(uri,values,null,null);uri=null;status("Saved to Download/DeskHalo: "+name);}catch(Exception e){status("Download failed: "+e.getMessage());}finally{if(uri!=null)getContentResolver().delete(uri,null,null);if(c!=null)c.disconnect();}});}
    @Override protected void onActivityResult(int request,int result,Intent data){super.onActivityResult(request,result,data);if(request!=10||result!=RESULT_OK||data==null||data.getData()==null)return;Uri uri=data.getData();worker.execute(()->{HttpURLConnection c=null;try{String name="Quest-file";try(Cursor cursor=getContentResolver().query(uri,new String[]{OpenableColumns.DISPLAY_NAME},null,null,null)){if(cursor!=null&&cursor.moveToFirst())name=cursor.getString(0);}status("Sending "+name);c=open("/files/upload?name="+encode(name));c.setRequestMethod("POST");c.setDoOutput(true);c.setChunkedStreamingMode(65536);c.setRequestProperty("Content-Type","application/octet-stream");try(InputStream in=getContentResolver().openInputStream(uri);OutputStream out=c.getOutputStream()){copy(in,out,2L*1024*1024*1024);}if(c.getResponseCode()!=200)throw new IOException("Host response "+c.getResponseCode());status("Sent to PC: "+name);}catch(Exception e){status("Send failed: "+e.getMessage());}finally{if(c!=null)c.disconnect();}});}
    private static void copy(InputStream in,OutputStream out,long limit)throws IOException{byte[] bytes=new byte[65536];long total=0;int n;while((n=in.read(bytes))>0){total+=n;if(total>limit)throw new IOException("File exceeds size limit");out.write(bytes,0,n);}}
    @Override public void onDestroy(){worker.shutdown();super.onDestroy();}
}
