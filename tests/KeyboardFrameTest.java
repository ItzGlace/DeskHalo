
package itz.glace.deskhalo;
public final class KeyboardFrameTest{
 static void require(boolean x,String text){if(!x)throw new AssertionError(text);}
 static float[] frame(){return new float[]{-.2f,0,0,-.2f,0,-.12f,-.2f,0,.01f,-.12f,0,.005f,.2f,0,0,.2f,0,-.12f,.2f,0,.01f,.12f,0,.005f};}
 public static void main(String[] args){
  float[] p=frame(),m=KeyboardFrame.measure(p);
  require(m!=null,"L-shaped hands should be measured");
  require(Math.abs(m[7]-.4f)<.001,"40 cm width");require(Math.abs(m[8]-.125f)<.001,"12.5 cm depth");
  require(Math.abs(m[6]-.7071068f)<.001&&Math.abs(m[3]+.7071068f)<.001,"keyboard lies horizontally facing up");
  KeyboardFrame k=new KeyboardFrame();require(k.update(p,100)==null,"no instant snap");
  require(k.update(p,1200)==null,"not stable for 1.2 s yet");require(k.update(p,1301)!=null,"stable frame snaps");
  k.reset();k.update(p,100);require(k.update(null,1000)==null,"lost tracking rejected");require(k.update(p,1400)==null,"reacquisition restarts hold");
  float[] moved=p.clone();for(int i=0;i<24;i+=3)moved[i]+=.03f;require(k.update(moved,2800)==null,"moving hands reset hold");
  float[] invalid=p.clone();invalid[3]=Float.NaN;require(KeyboardFrame.measure(invalid)==null,"NaN rejected");
  invalid=p.clone();invalid[9]=-.2f;invalid[11]=-.07f;require(KeyboardFrame.measure(invalid)==null,"closed thumb rejected");
  invalid=p.clone();for(int i=12;i<24;i+=3)invalid[i]-=.35f;require(KeyboardFrame.measure(invalid)==null,"tiny frame rejected");
  float[] rotated=p.clone();for(int i=0;i<24;i+=3){rotated[i]=-p[i+2]+1;rotated[i+1]=p[i+1]+.7f;rotated[i+2]=p[i]-.5f;}
  m=KeyboardFrame.measure(rotated);require(m!=null&&Math.abs(m[7]-.4f)<.001&&Math.abs(m[8]-.125f)<.001,"rotated frame preserves physical dimensions");
  float[] converging=frame();converging[3]=-.12f;converging[15]=.12f;require(KeyboardFrame.measure(converging)!=null,"inward-sloping index fingers from reference image accepted");
  System.out.println("KeyboardFrame: all assertions passed");
 }
}
