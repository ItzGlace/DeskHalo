package itz.glace.deskhalo;
import java.io.*;
import java.util.Arrays;
final class AccessUnits {
        private final InputStream input;
        private byte[] buffer = new byte[65536];
        private int length;
        private boolean eof;
        AccessUnits(InputStream input) { this.input = input; }
        byte[] next() throws IOException {
            while (true) {
                boolean first = false;
                for (int i = 0; i + 4 < length; i++) {
                    int header = -1;
                    if (buffer[i] == 0 && buffer[i+1] == 0) {
                        if (buffer[i+2] == 1) header = i+3;
                        else if (buffer[i+2] == 0 && buffer[i+3] == 1) header = i+4;
                    }
                    if (header >= 0) {
                        if ((buffer[header] & 31) == 9) {
                            if (first) { byte[] result = Arrays.copyOf(buffer, i); System.arraycopy(buffer, i, buffer, 0, length-i); length -= i; return result; }
                            first = true;
                        }
                        i = header;
                    }
                }
                if (eof) { if (length == 0) return null; byte[] result=Arrays.copyOf(buffer,length); length=0; return result; }
                if (length + 8192 > buffer.length) {
                    if (buffer.length >= 4*1024*1024) throw new IOException("Invalid H.264 access unit");
                    buffer = Arrays.copyOf(buffer, buffer.length*2);
                }
                int n = input.read(buffer,length,Math.min(8192,buffer.length-length));
                if (n < 0) eof = true; else length += n;
            }
        }
    }
