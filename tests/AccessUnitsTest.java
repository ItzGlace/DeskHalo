package itz.glace.deskhalo;
import java.io.*;
import java.util.*;

public class AccessUnitsTest {
    public static void main(String[] args) throws Exception {
        byte[][] expected = {
            {0,0,0,1,9,16,0,0,1,103,1,2,0,0,3,1,4,0,0,1,101,42},
            {0,0,1,9,16,0,0,0,1,65,3,7},
            {0,0,0,1,9,16,0,0,1,65,33}
        };
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        for(byte[] frame:expected)bytes.write(frame);
        for(int chunk:new int[]{1,2,3,4,5,7,8192}){
            InputStream input = new ByteArrayInputStream(bytes.toByteArray()) {
                public synchronized int read(byte[] b,int offset,int length){return super.read(b,offset,Math.min(length,chunk));}
            };
            AccessUnits parser = new AccessUnits(input);
            for(byte[] frame:expected)if(!Arrays.equals(frame,parser.next()))throw new AssertionError("Fragmented start code failed at chunk "+chunk);
            if(parser.next()!=null)throw new AssertionError("Unexpected trailing frame");
        }
        byte[] oversized=new byte[4*1024*1024+1];
        try {new AccessUnits(new ByteArrayInputStream(oversized)).next();throw new AssertionError("Missing size limit");} catch(IOException expectedFailure){}
        System.out.println("PASS: mixed start codes, fragmented reads, final frame, EOF and size bound");
    }
}
