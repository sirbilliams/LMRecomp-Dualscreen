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
}
