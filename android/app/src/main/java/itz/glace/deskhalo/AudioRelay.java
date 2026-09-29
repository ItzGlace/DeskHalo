package itz.glace.deskhalo;
import android.media.*;
import java.net.*;
import java.io.*;

final class AudioRelay implements AutoCloseable {
    private final String base,code;
    private volatile boolean running=true,sound=false,microphone=false;
    private volatile HttpURLConnection receive,send;
    private final Thread receiver,sender;
    AudioRelay(String base,String code){this.base=base;this.code=code;receiver=new Thread(this::listen,"DeskHalo-sound");sender=new Thread(this::speak,"DeskHalo-microphone");receiver.start();sender.start();}
    void set(boolean pcSound,boolean mic){sound=pcSound;microphone=mic;if(!sound&&receive!=null)receive.disconnect();if(!mic&&send!=null)send.disconnect();}
    private HttpURLConnection open(String path)throws Exception{HttpURLConnection c=(HttpURLConnection)new URL(base+path).openConnection();c.setConnectTimeout(2000);c.setReadTimeout(3000);c.setRequestProperty("X-DeskHalo-Code",code);return c;}
    private void pause(){try{Thread.sleep(250);}catch(InterruptedException ignored){}}
    private void listen(){while(running){if(!sound){pause();continue;}AudioTrack track=null;HttpURLConnection c=null;try{
        c=open("/audio");receive=c;if(c.getResponseCode()!=200)throw new IOException("Audio HTTP "+c.getResponseCode());
        track=new AudioTrack.Builder().setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build()).setAudioFormat(new AudioFormat.Builder().setSampleRate(48000).setEncoding(AudioFormat.ENCODING_PCM_16BIT).setChannelMask(AudioFormat.CHANNEL_OUT_STEREO).build()).setBufferSizeInBytes(Math.max(19200,AudioTrack.getMinBufferSize(48000,AudioFormat.CHANNEL_OUT_STEREO,AudioFormat.ENCODING_PCM_16BIT))).setTransferMode(AudioTrack.MODE_STREAM).setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY).build();track.play();
        try(DataInputStream in=new DataInputStream(c.getInputStream())){byte[] bytes=new byte[3840];while(running&&sound){in.readFully(bytes);int at=0;while(at<bytes.length&&running&&sound){int written=track.write(bytes,at,bytes.length-at,AudioTrack.WRITE_BLOCKING);if(written<=0)throw new IOException("Audio output stopped");at+=written;}}}
    }catch(Exception e){if(running&&sound)android.util.Log.w("DeskHalo","Sound reconnect: "+e.getMessage());pause();}finally{if(track!=null){track.stop();track.release();}if(c!=null)c.disconnect();receive=null;}}}
    private void speak(){while(running){if(!microphone){pause();continue;}AudioRecord recorder=null;HttpURLConnection c=null;try{
        recorder=new AudioRecord(MediaRecorder.AudioSource.VOICE_COMMUNICATION,48000,AudioFormat.CHANNEL_IN_MONO,AudioFormat.ENCODING_PCM_16BIT,Math.max(9600,AudioRecord.getMinBufferSize(48000,AudioFormat.CHANNEL_IN_MONO,AudioFormat.ENCODING_PCM_16BIT)));if(recorder.getState()!=AudioRecord.STATE_INITIALIZED)throw new IOException("Microphone unavailable");
        c=open("/audio/microphone");send=c;c.setRequestMethod("POST");c.setDoOutput(true);c.setChunkedStreamingMode(1920);c.setRequestProperty("Content-Type","application/octet-stream");recorder.startRecording();
        try(OutputStream out=c.getOutputStream()){byte[] bytes=new byte[1920];while(running&&microphone){int n=recorder.read(bytes,0,bytes.length);if(n<0)throw new IOException("Microphone stopped");out.write(bytes,0,n);out.flush();}}
    }catch(Exception e){if(running&&microphone)android.util.Log.w("DeskHalo","Microphone reconnect: "+e.getMessage());pause();}finally{if(recorder!=null){try{recorder.stop();}catch(Exception ignored){}recorder.release();}if(c!=null)c.disconnect();send=null;}}}
    public void close(){running=false;set(false,false);receiver.interrupt();sender.interrupt();}
}
