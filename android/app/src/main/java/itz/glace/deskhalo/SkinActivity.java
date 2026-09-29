
package itz.glace.deskhalo;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.widget.*;
import android.view.View;
import java.io.*;
import java.util.Locale;
import java.util.concurrent.Executors;

public class SkinActivity extends Activity {
    private final java.util.concurrent.ExecutorService loader=Executors.newSingleThreadExecutor();
    private final String[] names={"Left hand","Right hand","Left controller","Right controller"};
    private TextView status;private LinearLayout body;private int selected,pendingSlot;private boolean busy;
    private android.content.SharedPreferences prefs;
    private void label(String text,int size){TextView t=new TextView(this);t.setText(text);t.setTextSize(size);t.setTextColor(0xffe3e5eb);t.setPadding(12,12,12,12);try{t.setTypeface(Typeface.createFromAsset(getAssets(),"fonts/IBMPlexSans-ExtraLight.ttf"));}catch(Exception ignored){}body.addView(t);}
    private void button(String text,Runnable action){Button b=new Button(this);b.setText(text);b.setTextColor(0xffffc65c);b.setOnClickListener(v->{if(!busy)action.run();});body.addView(b);}
    @Override public void onCreate(Bundle state){
        super.onCreate(state);prefs=getSharedPreferences("skins",0);
        ScrollView scroll=new ScrollView(this);body=new LinearLayout(this);body.setOrientation(LinearLayout.VERTICAL);body.setPadding(32,16,32,24);body.setBackgroundColor(0xff1f2126);scroll.addView(body);setContentView(scroll);
        label("DeskHalo · Hands & controllers",30);
        label("Import a self-contained OBJ, GLB or FBX. Up to 32 MB, 20,000 triangles, eight embedded textures (2048 px maximum). Export in metres, with wrist or grip at the origin.",17);
        Spinner slots=new Spinner(this);slots.setAdapter(new ArrayAdapter<>(this,android.R.layout.simple_spinner_dropdown_item,names));slots.setOnItemSelectedListener(new android.widget.AdapterView.OnItemSelectedListener(){public void onNothingSelected(android.widget.AdapterView<?> p){}public void onItemSelected(android.widget.AdapterView<?> p,View v,int position,long id){selected=position;showStatus();}});body.addView(slots);
        status=new TextView(this);status.setTextColor(0xffffc65c);status.setTextSize(20);status.setPadding(12,18,12,18);body.addView(status);
        button("Import model for selected slot",()->{pendingSlot=selected;Intent i=new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("*/*").addCategory(Intent.CATEGORY_OPENABLE);startActivityForResult(i,81);});
        button("Smaller  −10%",()->adjust(.9f,0));
        button("Larger  +10%",()->adjust(1.1f,0));
        button("Rotate model 90°",()->adjust(1,(float)(Math.PI/2)));
        button("Restore built-in appearance",()->{ModelSkins.builtin(this,selected);prefs.edit().remove("ext"+selected).remove("name"+selected).remove("scale"+selected).remove("yaw"+selected).apply();ModelSkins.transform(selected,1,0);showStatus();});
        label("Hand articulation uses OpenXR joint names and matching local bone axes. OBJ hands follow the wrist as a rigid model. A custom controller can name its parts trigger, squeeze, button_a/button_b (or x/y), thumbstick and menu for press animation. Press indicators remain visible on unnamed models.",17);
        button("Back to workspace",this::finish);showStatus();
    }
    private void showStatus(){if(status!=null&&!busy)status.setText(names[selected]+" · "+prefs.getString("name"+selected,"Built-in")+" · "+String.format(Locale.US,"%.0f%%",prefs.getFloat("scale"+selected,1)*100));}
    private void adjust(float multiplier,float angle){
        if(angle!=0&&prefs.getBoolean("rigged"+selected,false)){status.setText("Rigged hands use joint axes from the model; rotate the rig before export.");return;}
        float scale=Math.max(.25f,Math.min(4,prefs.getFloat("scale"+selected,1)*multiplier)),yaw=(prefs.getFloat("yaw"+selected,0)+angle)%(float)(Math.PI*2);
        ModelSkins.transform(selected,scale,yaw);prefs.edit().putFloat("scale"+selected,scale).putFloat("yaw"+selected,yaw).apply();showStatus();
    }
    @Override public void onActivityResult(int request,int result,Intent data){
        super.onActivityResult(request,result,data);if(request!=81||result!=RESULT_OK||data==null||data.getData()==null)return;
        final android.net.Uri uri=data.getData();final int slot=pendingSlot;busy=true;status.setText("Importing "+names[slot]+"…");
        loader.execute(()->{
            File temp=null;String message;
            try {
                String name="";try(android.database.Cursor c=getContentResolver().query(uri,new String[]{OpenableColumns.DISPLAY_NAME},null,null,null)){if(c!=null&&c.moveToFirst())name=c.getString(0);}
                int dot=name.lastIndexOf('.');String ext=dot>=0?name.substring(dot+1).toLowerCase(Locale.ROOT):"";
                if(!ext.equals("obj")&&!ext.equals("glb")&&!ext.equals("fbx"))throw new IOException("Choose an .obj, .glb or .fbx file");
                temp=File.createTempFile("skin-import-","."+ext,getCacheDir());long total=0;
                try(InputStream input=getContentResolver().openInputStream(uri);OutputStream output=new FileOutputStream(temp)){
                    if(input==null)throw new IOException("Cannot open selected model");byte[] buffer=new byte[65536];int n;
                    while((n=input.read(buffer))!=-1){total+=n;if(total>32L*1024*1024)throw new IOException("Model exceeds 32 MB");output.write(buffer,0,n);}
                }
                message=ModelSkins.load(slot,temp.getAbsolutePath(),ext);
                if(message.startsWith("Error:"))throw new IOException(message.substring(6));
                File destination=new File(getFilesDir(),"skin-"+slot+"."+ext);
                java.nio.file.Files.move(temp.toPath(),destination.toPath(),java.nio.file.StandardCopyOption.REPLACE_EXISTING);temp=null;
                prefs.edit().putString("ext"+slot,ext).putString("name"+slot,name).putBoolean("rigged"+slot,message.contains("articulated")).apply();
            }catch(Exception e){message="Import failed: "+e.getMessage();}
            finally{if(temp!=null)temp.delete();}
            final String resultMessage=message;
            runOnUiThread(()->{busy=false;if(!isDestroyed())status.setText(resultMessage);});
        });
    }
    @Override public void onDestroy(){loader.shutdown();super.onDestroy();}
}
