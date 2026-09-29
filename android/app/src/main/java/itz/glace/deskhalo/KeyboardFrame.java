
package itz.glace.deskhalo;

/** Measurements are metres in OpenXR local space. No Android dependencies. */
public final class KeyboardFrame {
    private float[] previous;
    private long stableSince;
    public synchronized void reset(){previous=null;stableSince=0;}
    public float progress;
    public synchronized float[] update(float[] points,long now){
        float[] m=measure(points);
        if(m==null){reset();progress=0;return null;}
        if(previous==null || distance(m,previous)>.012f ||
           Math.abs(m[7]-previous[7])>.015f || Math.abs(m[8]-previous[8])>.012f ||
           Math.abs(m[3]*previous[3]+m[4]*previous[4]+m[5]*previous[5]+m[6]*previous[6])<.998f){
            previous=m;stableSince=now;progress=0;return null;
        }
        progress=Math.min(1,(now-stableSince)/1200f);
        return progress>=1?m:null;
    }
    private static float distance(float[] a,float[] b){
        return length(new float[]{a[0]-b[0],a[1]-b[1],a[2]-b[2]});
    }
    private static float[] sub(float[] a,float[] b){return new float[]{a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
    private static float dot(float[] a,float[] b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
    private static float length(float[] a){return (float)Math.sqrt(dot(a,a));}
    private static float[] unit(float[] a){float n=length(a);return n<.00001f?null:new float[]{a[0]/n,a[1]/n,a[2]/n};}
    private static float[] cross(float[] a,float[] b){return new float[]{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
    // Each hand: index MCP, index tip, thumb metacarpal, thumb tip.
    public static float[] measure(float[] p){
        if(p==null||p.length!=24)return null;
        for(float v:p)if(!Float.isFinite(v))return null;
        float[][] q=new float[8][3];for(int i=0;i<8;i++)System.arraycopy(p,i*3,q[i],0,3);
        float[] x=unit(sub(q[4],q[0])),il=unit(sub(q[1],q[0])),ir=unit(sub(q[5],q[4])),
            tl=unit(sub(q[3],q[2])),tr=unit(sub(q[7],q[6]));
        if(x==null||il==null||ir==null||tl==null||tr==null)return null;
        if(dot(il,ir)<.35f || Math.abs(dot(il,tl))>.75f || Math.abs(dot(ir,tr))>.75f ||
           dot(tl,x)<.45f || dot(tr,x)>-.45f)return null;
        float[] y=unit(new float[]{il[0]+ir[0],il[1]+ir[1],il[2]+ir[2]});
        float xy=dot(x,y);if(Math.abs(xy)>.45f)return null;
        y=unit(new float[]{y[0]-x[0]*xy,y[1]-x[1]*xy,y[2]-x[2]*xy});
        float[] z=cross(x,y);
        float minX=Float.MAX_VALUE,maxX=-minX,minY=minX,maxY=-minX;
        for(int i:new int[]{0,1,3,4,5,7}){
            float[] d=sub(q[i],q[0]);if(Math.abs(dot(d,z))>.035f)return null;
            float a=dot(d,x),b=dot(d,y);minX=Math.min(minX,a);maxX=Math.max(maxX,a);minY=Math.min(minY,b);maxY=Math.max(maxY,b);
        }
        float width=maxX-minX,depth=maxY-minY;
        if(width<.18f||width>1.1f||depth<.055f||depth>.4f)return null;
        float cx=(minX+maxX)/2,cy=(minY+maxY)/2;
        float[] out=new float[]{q[0][0]+cx*x[0]+cy*y[0]-.005f*z[0],
            q[0][1]+cx*x[1]+cy*y[1]-.005f*z[1],q[0][2]+cx*x[2]+cy*y[2]-.005f*z[2],0,0,0,1,width,depth};
        // Rotation matrix columns: keyboard right, forward, normal.
        float trace=x[0]+y[1]+z[2],s;
        if(trace>0){s=(float)Math.sqrt(trace+1)*2;out[6]=s/4;out[3]=(y[2]-z[1])/s;out[4]=(z[0]-x[2])/s;out[5]=(x[1]-y[0])/s;}
        else if(x[0]>y[1]&&x[0]>z[2]){s=(float)Math.sqrt(1+x[0]-y[1]-z[2])*2;out[6]=(y[2]-z[1])/s;out[3]=s/4;out[4]=(y[0]+x[1])/s;out[5]=(z[0]+x[2])/s;}
        else if(y[1]>z[2]){s=(float)Math.sqrt(1+y[1]-x[0]-z[2])*2;out[6]=(z[0]-x[2])/s;out[3]=(y[0]+x[1])/s;out[4]=s/4;out[5]=(z[1]+y[2])/s;}
        else{s=(float)Math.sqrt(1+z[2]-x[0]-y[1])*2;out[6]=(x[1]-y[0])/s;out[3]=(z[0]+x[2])/s;out[4]=(z[1]+y[2])/s;out[5]=s/4;}
        return out;
    }
}
