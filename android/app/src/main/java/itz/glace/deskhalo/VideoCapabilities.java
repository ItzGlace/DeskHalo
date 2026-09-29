package itz.glace.deskhalo;
import android.media.*;

final class VideoCapabilities {
    static String decoder(int width,int height,int fps) {
        for(MediaCodecInfo codec:new MediaCodecList(MediaCodecList.REGULAR_CODECS).getCodecInfos()) {
            if(codec.isEncoder()||!codec.isHardwareAccelerated())continue;
            try {if(codec.getCapabilitiesForType("video/avc").getVideoCapabilities().areSizeAndRateSupported(width,height,fps))return codec.getName();}catch(IllegalArgumentException ignored){}
        }
        throw new IllegalArgumentException("No hardware decoder supports this stream mode");
    }
    static int[] choose(int width,int height,int fps,int streams) {
        // Advertised per-decoder limits are not an aggregate guarantee. Bound the
        // simultaneous workload to 4K60 worth of pixels, retaining the requested FPS.
        int[][] sizes={{7680,4320},{3840,2160},{2560,1440},{1920,1080},{1280,720}};
        for(int[] size:sizes) {
            if(size[0]>width || (long)size[0]*size[1]*fps*Math.max(1,streams)>3840L*2160*60)continue;
            for(MediaCodecInfo codec:new MediaCodecList(MediaCodecList.REGULAR_CODECS).getCodecInfos()) {
                if(codec.isEncoder() || !codec.isHardwareAccelerated())continue;
                try {
                    MediaCodecInfo.VideoCapabilities caps=codec.getCapabilitiesForType("video/avc").getVideoCapabilities();
                    if(caps.areSizeAndRateSupported(size[0],size[1],fps))return new int[]{size[0],size[1],fps};
                } catch(IllegalArgumentException ignored){}
            }
        }
        if(fps>60)return choose(width,height,60,streams);
        return new int[]{1280,720,30};
    }
}
