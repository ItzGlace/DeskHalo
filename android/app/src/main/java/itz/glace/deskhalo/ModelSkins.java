
package itz.glace.deskhalo;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import java.io.File;

public final class ModelSkins {
    static {System.loadLibrary("deskhalo");}
    public static synchronized native String load(int slot,String path,String extension);
    public static synchronized native void clear(int slot);
    public static synchronized native void transform(int slot,float scale,float yaw);
    public static Bitmap decode(byte[] bytes){
        BitmapFactory.Options o=new BitmapFactory.Options();o.inJustDecodeBounds=true;
        BitmapFactory.decodeByteArray(bytes,0,bytes.length,o);
        if(o.outWidth<1||o.outHeight<1||o.outWidth>2048||o.outHeight>2048)return null;
        o.inJustDecodeBounds=false;o.inPreferredConfig=Bitmap.Config.ARGB_8888;o.inPremultiplied=false;
        return BitmapFactory.decodeByteArray(bytes,0,bytes.length,o);
    }
    public static synchronized String builtin(Context context,int slot){
        String asset=(slot<2?"generic-hand-":"oculus-touch-v2-")+(slot%2==0?"left.glb":"right.glb");
        File file=new File(context.getCacheDir(),asset);
        try(java.io.InputStream in=context.getAssets().open("models/"+asset);java.io.OutputStream out=new java.io.FileOutputStream(file)){
            byte[] buffer=new byte[16384];int n;while((n=in.read(buffer))!=-1)out.write(buffer,0,n);
        }catch(java.io.IOException e){return "Error: "+e.getMessage();}
        String result=load(slot,file.getAbsolutePath(),"glb");
        android.util.Log.i("DeskHalo","Built-in model "+asset+": "+result);
        return result;
    }
    public static synchronized String restore(Context context){
        android.content.SharedPreferences p=context.getSharedPreferences("skins",0);
        StringBuilder errors=new StringBuilder();
        for(int slot=0;slot<4;slot++){
            transform(slot,p.getFloat("scale"+slot,1),p.getFloat("yaw"+slot,0));
            String ext=p.getString("ext"+slot,"");
            String built=builtin(context,slot);
            if(built.startsWith("Error:"))errors.append(built).append("\n");
            if(!ext.isEmpty()){
                File file=new File(context.getFilesDir(),"skin-"+slot+"."+ext);
                String result=load(slot,file.getAbsolutePath(),ext);
                if(result.startsWith("Error:"))errors.append("Slot ").append(slot+1).append(": ").append(result).append("\n");
            }
        }
        return errors.toString();
    }
}
