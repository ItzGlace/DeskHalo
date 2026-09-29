package itz.glace.deskhalo;

import android.media.MediaCodec;
import android.media.MediaFormat;
import android.os.Build;
import android.view.Surface;
import java.io.*;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.BooleanSupplier;

/** Annex-B access units are delimited by AUDs inserted by the host. No frame queue. */
final class AvcStream {
    static void play(InputStream input, Surface surface, int width, int height, int fps,
                     BooleanSupplier active, Runnable rendered) throws Exception {
        MediaCodec codec = MediaCodec.createByCodecName(VideoCapabilities.decoder(width,height,fps));
        Thread drain = null;
        AtomicReference<Exception> failure = new AtomicReference<>();
        try {
            MediaFormat format = MediaFormat.createVideoFormat("video/avc", width, height);
            format.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, 4 * 1024 * 1024);
            format.setInteger(MediaFormat.KEY_PRIORITY, 0);
            format.setInteger(MediaFormat.KEY_FRAME_RATE, fps);
            format.setInteger(MediaFormat.KEY_OPERATING_RATE, fps);
            format.setInteger(MediaFormat.KEY_ALLOW_FRAME_DROP, 1);
            if (Build.VERSION.SDK_INT >= 30) format.setInteger(MediaFormat.KEY_LOW_LATENCY, 1);
            codec.configure(format, surface, null, 0); codec.start();android.util.Log.i("DeskHalo","Decoder started: "+codec.getName()+" "+width+"x"+height);
            drain = new Thread(() -> {
                MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
                try {
                    while (active.getAsBoolean() && !Thread.currentThread().isInterrupted()) {
                        int index = codec.dequeueOutputBuffer(info, 10000);
                        if (index >= 0) {
                            boolean show = info.size != 0;
                            // Discard stale decoded output, never encoded reference frames.
                            int newer;
                            while ((newer = codec.dequeueOutputBuffer(info, 0)) >= 0) {
                                codec.releaseOutputBuffer(index, false); index = newer; show = info.size != 0;
                            }
                            codec.releaseOutputBuffer(index, show); if (show) rendered.run();
                        }
                    }
                } catch (Exception e) { failure.set(e); }
            }, "DeskHalo-decode");
            drain.start();
            AccessUnits units = new AccessUnits(input);
            byte[] unit;
            while (active.getAsBoolean() && (unit = units.next()) != null) {
                if (failure.get() != null) throw failure.get();
                int index;
                long deadline = System.nanoTime() + 2000000000L;
                do { index = codec.dequeueInputBuffer(10000); if (!active.getAsBoolean()) return; if (failure.get() != null) throw failure.get(); if (System.nanoTime() > deadline) throw new IOException("Decoder stalled; reconnecting"); } while (index < 0);
                ByteBuffer buffer = codec.getInputBuffer(index);
                if (buffer == null || buffer.capacity() < unit.length) throw new IOException("Encoded frame exceeds decoder buffer");
                buffer.clear(); buffer.put(unit);
                codec.queueInputBuffer(index, 0, unit.length, System.nanoTime()/1000, 0);
            }
        } finally {
            boolean interrupted = Thread.interrupted();
            if (drain != null) {
                drain.interrupt();
                long deadline = System.nanoTime() + 1500000000L;
                while (drain.isAlive() && System.nanoTime() < deadline) {
                    try { drain.join(50); } catch (InterruptedException e) { interrupted = true; }
                }
            }
            try { codec.stop(); } catch (IllegalStateException ignored) { }
            codec.release();
            if (interrupted) Thread.currentThread().interrupt();
        }
    }

}
