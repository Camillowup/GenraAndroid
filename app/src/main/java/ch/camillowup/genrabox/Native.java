// SPDX-License-Identifier: GPL-3.0-only
package ch.camillowup.genrabox;

/** The native engine (app/src/main/cpp/box/jni.cpp). */
final class Native {
    static {
        System.loadLibrary("genrabox");
    }

    private Native() {}

    static native void init(String filesDir);
    static native boolean start();
    static native void stop();
    static native String meta();
    static native String state(int knownVersion);
    static native String fast();
    static native void cmd(int op, int a, int b, float f);
    static native void savePreset(int track, String name);
    static native void save();
    static native byte[] exportMidi(int what, int idx);
    static native String exportName();
    static native String audioInfo();
}
