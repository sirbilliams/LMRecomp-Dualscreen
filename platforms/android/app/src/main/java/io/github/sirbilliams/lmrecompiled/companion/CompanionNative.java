package io.github.sirbilliams.lmrecompiled.companion;

/**
 * Game state for the companion screen, read from guest memory by libmain.so
 * (src/port/android_companion.cpp). Calls never pause the game.
 */
public final class CompanionNative {
    private CompanionNative() {}

    /** "GLME01" / "GLMP01" once the game is running, else "". */
    public static native String gameId();

    /** spec holds groups of four ints: address, offset, size (1/2/4), deref (0/1). null clears. */
    public static native void setWatches(int[] spec);

    /** Returns [serial, value0, valid0, value1, valid1, ...]; serial is 0 while no game runs. */
    public static native int[] getSnapshot();

    /** Holds GameCube buttons (PAD_BUTTON_* mask) for the next `frames` controller reads. */
    public static native void pressButtons(int mask, int frames);

    /** Opens or closes the F1 options menu on the main screen. */
    public static native void toggleMenu();

    public static final int PAD_BUTTON_START = 0x1000;
}
