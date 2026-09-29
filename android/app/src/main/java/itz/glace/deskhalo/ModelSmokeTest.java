
package itz.glace.deskhalo;
import android.content.Context;
import android.util.Log;
import java.io.*;
final class ModelSmokeTest {
 static void run(Context context){
  if(!BuildConfig.DEBUG)return;
  for(String name:new String[]{"triangle.obj","triangle.glb","triangle.fbx","invalid.glb"}){
   File temp=null;
   try{
    temp=File.createTempFile("model-test-","."+name.substring(name.lastIndexOf('.')+1),context.getCacheDir());
    try(InputStream in=context.getAssets().open("model-tests/"+name);OutputStream out=new FileOutputStream(temp)){byte[] b=new byte[4096];int n;while((n=in.read(b))!=-1)out.write(b,0,n);}
    String result=ModelSkins.load(4,temp.getAbsolutePath(),name.substring(name.lastIndexOf('.')+1));
    boolean pass=name.startsWith("invalid")?result.startsWith("Error:"):result.equals("Validated model");
    Log.i("DeskHalo","Model test "+name+" "+(pass?"PASS":"FAIL")+" "+result);
   }catch(Exception e){Log.e("DeskHalo","Model test "+name+" FAIL",e);}
   finally{if(temp!=null)temp.delete();}
  }
 }
}
