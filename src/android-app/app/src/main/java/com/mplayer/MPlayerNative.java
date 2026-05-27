package com.mplayer;

public class MPlayerNative {
    private long nativeHandle = 0;

    public MPlayerNative() {
        nativeHandle = nativeCreate();
    }

    public boolean open(String url) {
        return nativeOpen(nativeHandle, url);
    }

    public void close() {
        nativeClose(nativeHandle);
    }

    public void play() { nativePlay(nativeHandle); }
    public void pause() { nativePause(nativeHandle); }
    public void stop() { nativeStop(nativeHandle); }
    public void seek(double seconds) { nativeSeek(nativeHandle, seconds); }
    public void setVolume(float volume) { nativeSetVolume(nativeHandle, volume); }
    public void setSpeed(float speed) { nativeSetSpeed(nativeHandle, speed); }
    public double getDuration() { return nativeGetDuration(nativeHandle); }
    public double getCurrentPosition() { return nativeGetCurrentPosition(nativeHandle); }
    public void setSurface(Object surface) { nativeSetSurface(nativeHandle, surface); }

    public void release() {
        if (nativeHandle != 0) {
            nativeDestroy(nativeHandle);
            nativeHandle = 0;
        }
    }

    private native long nativeCreate();
    private native void nativeDestroy(long handle);
    private native boolean nativeOpen(long handle, String url);
    private native void nativeClose(long handle);
    private native void nativePlay(long handle);
    private native void nativePause(long handle);
    private native void nativeStop(long handle);
    private native void nativeSeek(long handle, double seconds);
    private native void nativeSetVolume(long handle, float volume);
    private native void nativeSetSpeed(long handle, float speed);
    private native double nativeGetDuration(long handle);
    private native double nativeGetCurrentPosition(long handle);
    private native void nativeSetSurface(long handle, Object surface);

    static {
        System.loadLibrary("MPlayerAndroid");
    }
}
