package itz.glace.deskhalo;

import android.app.Activity;
import android.os.Bundle;
import android.graphics.*;
import android.view.WindowManager;
import android.util.Log;
import org.json.*;
import java.net.*;
import java.io.*;
import java.util.*;
import java.util.concurrent.*;

public class VrActivity extends Activity {
    static { System.loadLibrary("deskhalo"); }
    private native void nativeStart();
    private native void nativeStop();
    private native void nativeUpload(int slot, Bitmap bitmap);
    private native void nativeSettings(int screens,float width,float distance,float keyboardY,float keyboardZ,boolean passthrough,int videoWidth,int videoHeight);

    private native void nativeMeasureKeyboard(boolean enabled);
    private native void nativeKeyboardBounds(float width,float depth,boolean resetPose);
    private final KeyboardFrame keyboardFrame=new KeyboardFrame();
    private volatile long measureStarted=0;
    private boolean frameLatched=false;
    private long frameReleased=0;
    private volatile float keyboardWidth=0,keyboardDepth=0;
    public float[] onKeyboardFrame(float[] points){
        long now=android.os.SystemClock.elapsedRealtime();
        if(KeyboardFrame.measure(points)==null){
            if(frameReleased==0)frameReleased=now;
            if(now-frameReleased>700)frameLatched=false;
            keyboardFrame.reset();return null;
        }
        frameReleased=0;
        if(frameLatched)return null;
        float[] result=keyboardFrame.update(points,now);
        if(result!=null){frameLatched=true;keyboardWidth=result[7];keyboardDepth=result[8];runOnUiThread(()->{status=String.format(Locale.US,"Keyboard fitted: %.1f × %.1f cm",keyboardWidth*100,keyboardDepth*100);saveProfile("last");});}
        else status=points==null?"Keep both hands visible":String.format(Locale.US,"Frame keyboard with thumbs + index fingers · hold %d%%",(int)(keyboardFrame.progress*100));
        return result;
    }
    private void measureKeyboard(){
        nativeMeasureKeyboard(false);keyboardFrame.reset();frameLatched=false;measureStarted=android.os.SystemClock.elapsedRealtime();keyboardVisible=true;
        status="Make two L shapes: thumbs inward, index fingers forward. Hold 1.2 seconds.";
        nativeMeasureKeyboard(true);
    }
    private native void nativeOptions(boolean keyboard, int refresh);
    private native void nativeCursor(int slot,float u,float v,boolean visible);
    private boolean keyboardVisible=true,gaming=false;
    private int profileSlot=0;private AudioRelay audio;private boolean pcSound=false,mic=false;
    private String refreshStatus="Checking headset refresh rate";
    public void onRefreshRate(float rate){runOnUiThread(()->refreshStatus="Headset "+Math.round(rate)+" Hz");}
    private volatile HttpURLConnection inputConnection;
    private volatile boolean menuVisible=false;
    private float distance=1.25f, keyboardY=-.48f, keyboardZ=-.65f;
    private boolean passthrough=true;
    private Typeface thin,light;
    private final SurfaceTexture[] textures=new SurfaceTexture[3];
    private final android.view.Surface[] videoSurfaces=new android.view.Surface[3];
    private final java.util.concurrent.atomic.AtomicBoolean[] fresh={new java.util.concurrent.atomic.AtomicBoolean(),new java.util.concurrent.atomic.AtomicBoolean(),new java.util.concurrent.atomic.AtomicBoolean()};
    private final HttpURLConnection[] connections=new HttpURLConnection[3];
    private volatile int generation=0;
    private int tab=0;
    private volatile int[] effective={1280,720,60};
    private final android.os.HandlerThread frameCallbacks=new android.os.HandlerThread("DeskHalo-frames",android.os.Process.THREAD_PRIORITY_DISPLAY);
    private final java.util.ArrayList<Runnable> actions=new java.util.ArrayList<>();
    public synchronized void onVideoTexture(int slot,int texture){
        textures[slot]=new SurfaceTexture(texture);
        textures[slot].setOnFrameAvailableListener(t->fresh[slot].set(true),new android.os.Handler(frameCallbacks.getLooper()));
        videoSurfaces[slot]=new android.view.Surface(textures[slot]);
    }
    public boolean updateVideo(int slot,float[] transform){
        if(textures[slot]==null||!fresh[slot].getAndSet(false))return false;
        textures[slot].updateTexImage(); textures[slot].getTransformMatrix(transform); return true;
    }
    public synchronized void releaseVideoTextures(){for(int i=0;i<3;i++){if(videoSurfaces[i]!=null)videoSurfaces[i].release();if(textures[i]!=null)textures[i].release();videoSurfaces[i]=null;textures[i]=null;}}
    public void onMenuChanged(boolean visible){menuVisible=visible;}
    public void onPassthroughStatus(String message){Log.i("DeskHalo",message);driverStatus=message;}
    private void settings(){effective=VideoCapabilities.choose(widths[resolution],heights[resolution],fps,screens);nativeSettings(screens,screenWidth,distance,keyboardY,keyboardZ,passthrough,effective[0],effective[1]);nativeOptions(keyboardVisible,Math.max(90,fps));nativeKeyboardBounds(keyboardWidth,keyboardDepth,false);saveProfile("last");}
    private void restartStreams(){settings();generation++; synchronized(this){for(HttpURLConnection c:connections)if(c!=null)c.disconnect();}}

    private native void nativeRecenter();
    private final ScheduledExecutorService workers=Executors.newScheduledThreadPool(7);
    private volatile boolean alive;private volatile boolean streamPaused=true;
    public void onSessionFocus(boolean focused){streamPaused=!focused;runOnUiThread(()->{if(!alive)return;if(!focused)restartStreams();if(audio!=null)audio.set(focused&&pcSound,focused&&mic);});}
    private volatile int fps=60,resolution=0,layout=100,screens=1,displayCount=1,virtualCount=0;
    private volatile float screenWidth=1.5f;
    private volatile String status="Connecting to DeskHalo Host…",driverStatus="Extra desktops require a Windows virtual display driver";
    private volatile int[] pressed=new int[0];
    private String base,code;
    private final int[] widths={1280,1920,2560,3840,7680},heights={720,1080,1440,2160,4320};
    private final int[] source={0,1,2};
    private final long[] frames={0,0,0},startTime={0,0,0};
    private final float[] measured={0,0,0};
    private int selected=0;
    private volatile String pointerRequest=null;
    public void onDesktopPointer(int slot,float u,float v,boolean down){if(slot<0||slot>2)return;pointerRequest="/pointer?id="+source[slot]+"&u="+u+"&v="+v+"&down="+(down?1:0);}
    private void sendPointer(){String next=pointerRequest;pointerRequest=null;if(next!=null&&alive)try{request(next,true);}catch(Exception ignored){}}

    private final Paint paint=new Paint(3);
    @Override public void onCreate(Bundle state){
        super.onCreate(state);workers.execute(()->{ModelSmokeTest.run(this);String error=ModelSkins.restore(this);if(!error.isEmpty())status=error;}); getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        String host=getIntent().getStringExtra("host"); code=getIntent().getStringExtra("code");
        if(host==null||code==null){finish();return;}
        base="http://"+host+(host.contains(":")?"":":47654");
        thin=Typeface.createFromAsset(getAssets(),"fonts/IBMPlexSans-ExtraLight.ttf");light=Typeface.createFromAsset(getAssets(),"fonts/IBMPlexSans-Light.ttf");loadProfile("last",false);if(BuildConfig.DEBUG){int test=getIntent().getIntExtra("testFps",0);if(test==60||test==90||test==120){fps=test;resolution=Math.max(0,Math.min(4,getIntent().getIntExtra("testResolution",0)));screens=1;gaming=true;}}alive=true;audio=new AudioRelay(base,code);frameCallbacks.start(); nativeStart(); settings();drawCursor();
        workers.execute(this::refreshDisplays);
        for(int i=0;i<3;i++){final int slot=i;workers.execute(()->stream(slot));}
        workers.execute(this::inputLoop);workers.scheduleWithFixedDelay(this::sendPointer,0,20,TimeUnit.MILLISECONDS);
        workers.scheduleWithFixedDelay(this::connectionTests,1,2,TimeUnit.SECONDS);
        getWindow().getDecorView().post(drawLoop);
    }
    private int[] lastKeys={-1}; private int lastLayout=-1;
    private long lastKeyboardMinute=-1;
    private final Runnable drawLoop=new Runnable(){public void run(){if(!alive)return;if(menuVisible)drawControls();if(keyboardVisible&&(!Arrays.equals(lastKeys,pressed)||lastLayout!=layout||lastKeyboardMinute!=System.currentTimeMillis()/60000)){drawKeyboard();lastKeyboardMinute=System.currentTimeMillis()/60000;lastKeys=pressed.clone();lastLayout=layout;}getWindow().getDecorView().postDelayed(this,100);}};
    private int[] ints(JSONArray a)throws JSONException{int[] r=new int[a.length()];for(int i=0;i<r.length;i++)r[i]=a.getInt(i);return r;}
    private byte[] request(String path,boolean post)throws Exception{
        HttpURLConnection c=(HttpURLConnection)new URL(base+path).openConnection();
        c.setConnectTimeout(2000);c.setReadTimeout(post?25000:4000);c.setRequestProperty("X-DeskHalo-Code",code);
        if(post)c.setRequestMethod("POST");
        try{
            int response=c.getResponseCode();
            if(response!=200){String message="Host response "+response;try(InputStream error=c.getErrorStream()){if(error!=null){byte[] bytes=new byte[4096];int n=error.read(bytes);if(n>0)message=new JSONObject(new String(bytes,0,n)).optString("error",message);}}catch(Exception ignored){}throw new IOException(message);}
            try(InputStream in=c.getInputStream();ByteArrayOutputStream out=new ByteArrayOutputStream()){
                byte[] b=new byte[16384];int n;while((n=in.read(b))!=-1){out.write(b,0,n);if(out.size()>16*1024*1024)throw new IOException("Frame too large");}return out.toByteArray();
            }
        }finally{c.disconnect();}
    }

    private void refreshDisplays(){
        try{
            JSONArray displays=new JSONObject(new String(request("/hello",false))).getJSONArray("displays");
            displayCount=displays.length();screens=Math.max(1,Math.min(screens,displayCount));selected=Math.min(selected,screens-1);
            for(int i=0;i<screens;i++){
                boolean duplicate=false;for(int other=0;other<i;other++)if(source[other]==source[i])duplicate=true;
                if(source[i]>=displayCount||duplicate){
                    for(int candidate=0;candidate<displayCount;candidate++){
                        boolean used=false;for(int other=0;other<i;other++)if(source[other]==candidate)used=true;
                        if(!used){source[i]=candidate;break;}
                    }
                }
            }
            virtualCount=0;for(int i=0;i<displays.length();i++)if(displays.getJSONObject(i).optBoolean("isVirtual",false))virtualCount++;
            status="Connected • "+displayCount+" Windows display(s)";
        }catch(Exception e){status=e.getMessage();}
    }
    private void stream(int slot){
        while(alive&&!Thread.currentThread().isInterrupted()){
            HttpURLConnection connection=null;
            try{
                android.view.Surface surface; synchronized(this){surface=videoSurfaces[slot];}
                if(streamPaused||slot>=screens||source[slot]>=displayCount||surface==null){Thread.sleep(150);continue;}
                final int epoch=generation;final int[] mode=effective.clone();
                connection=(HttpURLConnection)new URL(base+"/video?id="+source[slot]+"&width="+mode[0]+"&height="+mode[1]+"&fps="+mode[2]+"&cursor=1&gaming="+(gaming?1:0)).openConnection();
                connection.setConnectTimeout(3000);connection.setReadTimeout(4000);connection.setRequestProperty("X-DeskHalo-Code",code);
                synchronized(this){connections[slot]=connection;}
                if(connection.getResponseCode()!=200)throw new IOException("Video unavailable: HTTP "+connection.getResponseCode());
                status="H.264 live • "+connection.getHeaderField("X-DeskHalo-Encoder");
                try(InputStream input=connection.getInputStream()){
                    AvcStream.play(input,surface,mode[0],mode[1],mode[2],()->alive&&!streamPaused&&generation==epoch&&!Thread.currentThread().isInterrupted(),()->{
                        long now=System.nanoTime();frames[slot]++;if(startTime[slot]==0)startTime[slot]=now;
                        if(now-startTime[slot]>=1000000000L){measured[slot]=frames[slot]*1000000000f/(now-startTime[slot]);Log.d("DeskHalo","Panel "+slot+" decoded FPS "+Math.round(measured[slot]));frames[slot]=0;startTime[slot]=now;}
                    });
                }
            }catch(Exception e){if(alive){status=e.getMessage();Log.w("DeskHalo","Stream "+slot,e);try{Thread.sleep(500);}catch(InterruptedException stop){return;}}}
            finally{if(connection!=null)connection.disconnect();synchronized(this){connections[slot]=null;}}
        }
    }
    public void onNativeError(String error){Log.e("DeskHalo",error);runOnUiThread(()->{status=error;android.widget.Toast.makeText(this,error,1).show();});}
    public void onVrClick(float x,float y){runOnUiThread(()->{
        if(y>=95&&y<147){tab=Math.min(6,Math.max(0,(int)(x/(1024f/7))));drawControls();return;}
        if(y>=175&&y<470){int col=x<512?0:1,row=(int)((y-175)/100),index=row*2+col;if(index<actions.size())actions.get(index).run();settings();drawControls();}
    });}
    private void createDisplays(int requested){workers.execute(()->{try{
        request("/virtual?count="+requested,true);virtualCount=requested;refreshDisplays();
        if(requested>0&&displayCount>1){screens=Math.min(3,displayCount);for(int i=0;i<screens;i++)source[i]=i;}
        driverStatus="Windows extended • "+displayCount+" displays";restartStreams();settings();
    }catch(Exception e){driverStatus=e.getMessage()+" • Set up extra displays on PC";}});}
    private void text(Canvas c,String s,float x,float y,int size,int color){paint.setColor(color);paint.setTextSize(size);paint.setTypeface(size>=24?thin:light);c.drawText(s==null?"":s,x,y,paint);}
    private void box(Canvas c,float x,float y,float w,float h,int color){paint.setColor(color);c.drawRoundRect(x,y,x+w,y+h,20,20,paint);}
    private void tile(Canvas c,String title,String value,Runnable action){int index=actions.size();int x=24+(index%2)*500,y=175+(index/2)*100;actions.add(action);box(c,x,y,476,84,0xff2a2d34);text(c,title,x+20,y+27,16,0xffbfc4cb);text(c,value,x+20,y+65,30,0xffffc65c);}
    private void drawControls(){
        actions.clear();Bitmap b=Bitmap.createBitmap(1024,600,Bitmap.Config.ARGB_8888);Canvas c=new Canvas(b);box(c,0,0,1024,600,0xf21f2126);
        text(c,"DeskHalo",28,53,38,0xffe2e5e9);text(c,"YOUR WORKSPACE",730,48,17,0xffffbd45);
        String[] tabs={"Workspace","Stream","Keyboard","Displays","Profiles","Transfer","Appearance"};for(int i=0;i<7;i++){if(i==tab)box(c,i*146.28f+4,95,138,52,0xff3b3426);text(c,tabs[i],i*146.28f+10,130,18,i==tab?0xffffc65c:0xffbfc4cb);}
        if(tab==0){
            tile(c,"MOVE SCREEN CLOSER",String.format(Locale.US,"−  %.2f m",distance),()->distance=Math.max(.6f,distance-.15f));
            tile(c,"MOVE SCREEN FARTHER","+  Distance",()->distance=Math.min(3.5f,distance+.15f));
            tile(c,"SCREEN SIZE","−  Smaller",()->screenWidth=Math.max(.6f,screenWidth-.15f));
            tile(c,"SCREEN SIZE",String.format(Locale.US,"+  %.2f m wide",screenWidth),()->screenWidth=Math.min(3.5f,screenWidth+.15f));
            tile(c,"ENVIRONMENT",passthrough?"Passthrough on":"Passthrough off",()->passthrough=!passthrough);
            tile(c,"POSITION","Recenter workspace",()->nativeRecenter());
        }else if(tab==1){
            tile(c,"FRAME RATE",fps+" FPS",()->{fps=fps==15?30:fps==30?60:fps==60?90:fps==90?120:15;restartStreams();});
            tile(c,"RESOLUTION",heights[resolution]+"p → "+effective[1]+"p",()->{resolution=(resolution+1)%widths.length;restartStreams();});
            tile(c,"GAMING MODE",gaming?"On · low latency":"Off · balanced",()->{gaming=!gaming;restartStreams();});tile(c,"RECEIVED",Math.round(measured[0])+" FPS",()->{});tile(c,"DISPLAY REFRESH",refreshStatus,()->{});tile(c,"APPLY VIRTUAL DESKTOP MODE",widths[resolution]+" × "+heights[resolution],()->workers.execute(()->{try{JSONObject result=new JSONObject(new String(request("/display-mode?id="+source[selected]+"&width="+widths[resolution]+"&height="+heights[resolution]+"&fps="+Math.max(30,fps),true)));status=result.getString("message");refreshDisplays();restartStreams();}catch(Exception e){status=e.getMessage();}}));
        }else if(tab==2){
            tile(c,"KEYBOARD LAYOUT",layout+"%",()->layout=layout==100?80:layout==80?60:100);
            tile(c,"KEYBOARD",keyboardVisible?"Hide keyboard":"Show keyboard",()->keyboardVisible=!keyboardVisible);
            tile(c,"FIT TO REAL KEYBOARD","Measure with fingers",this::measureKeyboard);tile(c,"KEYBOARD BOUNDS",keyboardWidth>0?String.format(Locale.US,"%.0f × %.0f cm · reset",keyboardWidth*100,keyboardDepth*100):"Default · reset position",()->{nativeMeasureKeyboard(false);keyboardWidth=keyboardDepth=0;nativeKeyboardBounds(0,0,true);});
            tile(c,"DISTANCE","Closer",()->keyboardZ=Math.min(-.3f,keyboardZ+.05f));tile(c,"DISTANCE","Farther",()->keyboardZ=Math.max(-1.3f,keyboardZ-.05f));
        }else if(tab==3){
            tile(c,"VR PANELS","+  Panel ("+screens+"/3)",()->{if(screens<3){if(displayCount<=screens){createDisplays(Math.min(3,virtualCount+1));return;}source[screens]=screens;screens++;restartStreams();}});tile(c,"VR PANELS","−  Panel",()->{screens=Math.max(1,screens-1);selected=Math.min(selected,screens-1);restartStreams();});
            tile(c,"SELECT PANEL","Panel "+(selected+1),()->selected=(selected+1)%screens);tile(c,"SOURCE","Windows display "+(source[selected]+1),()->{for(int step=1;step<=displayCount;step++){int candidate=(source[selected]+step)%displayCount;boolean used=false;for(int other=0;other<screens;other++)if(other!=selected&&source[other]==candidate)used=true;if(!used){source[selected]=candidate;break;}}restartStreams();});
            tile(c,"EXTEND WINDOWS","+  Desktop",()->createDisplays(Math.min(3,virtualCount+1)));tile(c,"EXTEND WINDOWS","−  Desktop",()->createDisplays(Math.max(0,virtualCount-1)));
        }
        if(tab==4){
            tile(c,"PROFILE SLOT","Profile "+(profileSlot+1),()->profileSlot=(profileSlot+1)%3);
            tile(c,"SAVE CURRENT","Save to slot",()->{saveProfile("slot"+profileSlot);status="Saved profile "+(profileSlot+1);});
            tile(c,"LOAD PROFILE","Restore slot",()->loadProfile("slot"+profileSlot,true));
            tile(c,"GAMING PRESET","720p · 120 FPS",()->{resolution=0;fps=120;screens=1;selected=0;gaming=true;keyboardVisible=false;restartStreams();});
            tile(c,"DESKTOP PRESET","1080p · 60 FPS",()->{resolution=1;fps=60;gaming=false;keyboardVisible=true;restartStreams();});
            tile(c,"PERSISTENCE","Last setup auto-saves",()->{});
        }
        if(tab==5){tile(c,"FILES","Send / receive files",()->startActivity(new android.content.Intent(this,TransferActivity.class).putExtra("base",base).putExtra("code",code)));tile(c,"PC SOUND",pcSound?"On · click to mute":"Off · click to listen",()->{pcSound=!pcSound;if(pcSound)mic=false;audio.set(pcSound,mic);});tile(c,"QUEST MICROPHONE",mic?"On · click to mute":"Off · relay to PC",()->{if(mic){mic=false;audio.set(pcSound,false);}else if(checkSelfPermission(android.Manifest.permission.RECORD_AUDIO)!=android.content.pm.PackageManager.PERMISSION_GRANTED)requestPermissions(new String[]{android.Manifest.permission.RECORD_AUDIO},44);else{mic=true;pcSound=false;audio.set(false,true);}});tile(c,"MICROPHONE ROUTE","Plays through PC speakers",()->{});tile(c,"HAND CONTROLS","Left pinch: menu",()->{});tile(c,"KEYBOARD FIT","Keyboard → Measure",()->tab=2);}
        if(tab==6){tile(c,"CUSTOM HANDS / CONTROLLERS","Import OBJ · GLB · FBX",()->startActivity(new android.content.Intent(this,SkinActivity.class)));tile(c,"CONTROLLER FEEDBACK","Live buttons + thumbsticks",()->{});tile(c,"IDLE CONTROLLERS","10 seconds → 10% opacity",()->{});tile(c,"HAND FIT","Articulated OpenXR rigs",()->{});tile(c,"KEYBOARD HEIGHT","↑ Raise",()->keyboardY=Math.min(-.15f,keyboardY+.05f));tile(c,"KEYBOARD HEIGHT","↓ Lower",()->keyboardY=Math.max(-1.1f,keyboardY-.05f));}
        text(c,tab==3?driverStatus:status,28,520,17,0xffbfc4cb);text(c,"Left ≡ / left pinch: menu · Right trigger / pinch: select",28,563,18,0xff8a8f98);
        nativeUpload(3,b);b.recycle();
    }
    private long lastConnectionTest=0;
    private void connectionTests(){if(!alive)return;try{JSONObject request=new JSONObject(new String(request("/connection-tests",false)));long epoch=request.getLong("generation");if(epoch==0||epoch==lastConnectionTest)return;lastConnectionTest=epoch;JSONArray addresses=request.getJSONArray("addresses");StringBuilder result=new StringBuilder();
        for(int i=0;i<addresses.length()&&alive;i++){String address=addresses.getString(i);if(!address.matches("[0-9.]+"))continue;long start=System.nanoTime();HttpURLConnection c=null;try{c=(HttpURLConnection)new URL("http://"+address+":47654/probe").openConnection();c.setRequestProperty("X-DeskHalo-Code",code);c.setConnectTimeout(1500);c.setReadTimeout(2000);if(c.getResponseCode()!=200)throw new IOException("HTTP "+c.getResponseCode());long first=System.nanoTime();int count=0,n;byte[] buffer=new byte[8192];try(InputStream in=c.getInputStream()){while((n=in.read(buffer))>=0)count+=n;}double seconds=Math.max(.001,(System.nanoTime()-first)/1e9);result.append(address.equals("127.0.0.1")?"USB":"Network "+address).append(": reachable · ").append(Math.round((first-start)/1e6)).append(" ms · ").append(String.format(Locale.US,"%.1f",count*8/seconds/1e6)).append(" Mbps probe\n");}catch(Exception e){result.append(address).append(": unavailable (not connected or blocked)\n");}finally{if(c!=null)c.disconnect();}}
        result.append("Measured from Quest. Other hotspot bands require connecting Quest to that band.");Log.i("DeskHalo","Connection tests: "+result.toString());JSONObject report=new JSONObject();report.put("generation",epoch);report.put("text",result.toString());byte[] bytes=report.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8);HttpURLConnection c=(HttpURLConnection)new URL(base+"/connection-tests").openConnection();try{c.setRequestMethod("POST");c.setConnectTimeout(2000);c.setReadTimeout(2000);c.setRequestProperty("X-DeskHalo-Code",code);c.setRequestProperty("Content-Type","application/json");c.setDoOutput(true);c.setFixedLengthStreamingMode(bytes.length);try(OutputStream out=c.getOutputStream()){out.write(bytes);}c.getResponseCode();}finally{c.disconnect();}
    }catch(Exception ignored){}}
    private void inputLoop(){while(alive){HttpURLConnection connection=null;try{
        connection=(HttpURLConnection)new URL(base+"/input").openConnection();inputConnection=connection;connection.setRequestProperty("X-DeskHalo-Code",code);connection.setConnectTimeout(2000);connection.setReadTimeout(3000);
        if(connection.getResponseCode()!=200)throw new IOException("Input stream unavailable");
        try(BufferedReader reader=new BufferedReader(new InputStreamReader(connection.getInputStream()))){String line;while(alive&&(line=reader.readLine())!=null){JSONObject state=new JSONObject(line);pressed=ints(state.getJSONArray("keys"));int id=state.getInt("id");for(int slot=0;slot<3;slot++)nativeCursor(slot,(float)state.getDouble("u"),(float)state.getDouble("v"),state.getBoolean("visible")&&source[slot]==id&&slot<screens);}}
    }catch(Exception e){for(int slot=0;slot<3;slot++)nativeCursor(slot,0,0,false);try{Thread.sleep(300);}catch(InterruptedException ignored){return;}}finally{if(connection!=null)connection.disconnect();inputConnection=null;}}}
    private void drawCursor(){Bitmap b=Bitmap.createBitmap(64,64,Bitmap.Config.ARGB_8888);Canvas c=new Canvas(b);Path arrow=new Path();arrow.moveTo(2,2);arrow.lineTo(2,48);arrow.lineTo(14,36);arrow.lineTo(23,58);arrow.lineTo(31,54);arrow.lineTo(22,32);arrow.lineTo(40,32);arrow.close();Paint p=new Paint(3);p.setColor(Color.BLACK);p.setStyle(Paint.Style.STROKE);p.setStrokeWidth(5);c.drawPath(arrow,p);p.setColor(Color.WHITE);p.setStyle(Paint.Style.FILL);c.drawPath(arrow,p);nativeUpload(6,b);b.recycle();}
    private void saveProfile(String name){try{JSONObject p=new JSONObject();p.put("fps",fps);p.put("resolution",resolution);p.put("width",screenWidth);p.put("distance",distance);p.put("keyboardY",keyboardY);p.put("keyboardZ",keyboardZ);p.put("keyboardWidth",keyboardWidth);p.put("keyboardDepth",keyboardDepth);p.put("keyboardVisible",keyboardVisible);p.put("layout",layout);p.put("screens",screens);p.put("passthrough",passthrough);p.put("gaming",gaming);p.put("source",new JSONArray(source));getSharedPreferences("profiles",0).edit().putString(name,p.toString()).apply();}catch(JSONException ignored){}}
    private void loadProfile(String name,boolean restart){try{String saved=getSharedPreferences("profiles",0).getString(name,null);if(saved==null){if(restart)status="This profile slot is empty";return;}JSONObject p=new JSONObject(saved);fps=p.getInt("fps");if(fps!=15&&fps!=30&&fps!=60&&fps!=90&&fps!=120)fps=60;resolution=Math.max(0,Math.min(widths.length-1,p.getInt("resolution")));screenWidth=(float)p.getDouble("width");distance=(float)p.getDouble("distance");keyboardY=(float)p.getDouble("keyboardY");keyboardZ=(float)p.getDouble("keyboardZ");keyboardWidth=(float)p.optDouble("keyboardWidth",0);keyboardDepth=(float)p.optDouble("keyboardDepth",0);keyboardVisible=p.getBoolean("keyboardVisible");layout=p.getInt("layout");screens=Math.max(1,Math.min(3,p.getInt("screens")));selected=0;passthrough=p.getBoolean("passthrough");gaming=p.getBoolean("gaming");JSONArray a=p.getJSONArray("source");for(int i=0;i<3;i++)source[i]=Math.max(0,a.getInt(i));if(restart){restartStreams();status="Loaded profile "+(profileSlot+1);}}catch(Exception e){status="Could not load profile";}}
    private boolean down(int vk){for(int k:pressed)if(k==vk)return true;return false;}
    private void key(Canvas c,String label,int vk,float x,float y,float w){boolean on=down(vk);box(c,x,y,w-4,47,on?0xffffbd45:0xff292c33);text(c,label,x+9,y+30,label.length()>5?13:17,on?0xff141414:0xffd7dae2);}
    private void row(Canvas c,String[] labels,int[] codes,float x,float y,float step){for(int i=0;i<labels.length;i++)key(c,labels[i],codes[i],x+i*step,y,step);}
    private void drawKeyboard(){
        int boardWidth=layout==100?1280:layout==80?1000:790;
        Bitmap b=Bitmap.createBitmap(boardWidth,layout==60?305:359,Bitmap.Config.ARGB_8888);Canvas c=new Canvas(b);c.drawColor(0xff17191e);
        text(c,android.text.format.DateFormat.getTimeFormat(this).format(new java.util.Date()),boardWidth-95,20,15,0xffa6a9b2);
        float y=layout==60?30:84;
        if(layout!=60)row(c,new String[]{"Esc","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12"},new int[]{27,112,113,114,115,116,117,118,119,120,121,122,123},20,30,60);

        row(c,new String[]{"`","1","2","3","4","5","6","7","8","9","0","−","=","Back"},new int[]{192,49,50,51,52,53,54,55,56,57,48,189,187,8},20,y,54);
        row(c,new String[]{"Tab","Q","W","E","R","T","Y","U","I","O","P","[","]","\\"},new int[]{9,81,87,69,82,84,89,85,73,79,80,219,221,220},20,y+54,54);
        row(c,new String[]{"Caps","A","S","D","F","G","H","J","K","L",";","'","Enter"},new int[]{20,65,83,68,70,71,72,74,75,76,186,222,13},20,y+108,58);
        row(c,new String[]{"Shift","Z","X","C","V","B","N","M",",",".","/","Shift"},new int[]{160,90,88,67,86,66,78,77,188,190,191,161},20,y+162,63);
        key(c,"Ctrl",162,20,y+216,68);key(c,"Win",91,88,y+216,68);key(c,"Alt",164,156,y+216,68);key(c,"Space",32,224,y+216,330);key(c,"Alt",165,554,y+216,68);key(c,"Menu",93,622,y+216,68);key(c,"Ctrl",163,690,y+216,82);
        if(layout!=60){row(c,new String[]{"Prt","Scr","Pause"},new int[]{44,145,19},800,30,58);row(c,new String[]{"Ins","Home","PgUp"},new int[]{45,36,33},800,y,58);row(c,new String[]{"Del","End","PgDn"},new int[]{46,35,34},800,y+54,58);key(c,"↑",38,858,y+162,58);row(c,new String[]{"←","↓","→"},new int[]{37,40,39},800,y+216,58);}
        if(layout==100){row(c,new String[]{"Num","/","*","−"},new int[]{144,111,106,109},996,y,64);row(c,new String[]{"7","8","9","+"},new int[]{103,104,105,107},996,y+54,64);row(c,new String[]{"4","5","6","+"},new int[]{100,101,102,107},996,y+108,64);row(c,new String[]{"1","2","3","Ent"},new int[]{97,98,99,13},996,y+162,64);key(c,"0",96,996,y+216,128);key(c,".",110,1124,y+216,64);key(c,"Ent",13,1188,y+216,64);}
        nativeUpload(4,b);b.recycle();
    }
    @Override public void onRequestPermissionsResult(int request,String[] permissions,int[] results){super.onRequestPermissionsResult(request,permissions,results);if(request==44&&results.length>0&&results[0]==android.content.pm.PackageManager.PERMISSION_GRANTED){mic=true;pcSound=false;audio.set(false,true);}}
    @Override public void onPause(){super.onPause();if(audio!=null)audio.set(false,false);}
    @Override public void onResume(){super.onResume();if(audio!=null)audio.set(!streamPaused&&pcSound,!streamPaused&&mic);}
    @Override public void onDestroy(){if(audio!=null)audio.close();saveProfile("last");alive=false;if(inputConnection!=null)inputConnection.disconnect();restartStreams();getWindow().getDecorView().removeCallbacks(drawLoop);workers.shutdownNow();try{workers.awaitTermination(3,TimeUnit.SECONDS);}catch(InterruptedException ignored){}nativeStop();frameCallbacks.quitSafely();super.onDestroy();}
}
