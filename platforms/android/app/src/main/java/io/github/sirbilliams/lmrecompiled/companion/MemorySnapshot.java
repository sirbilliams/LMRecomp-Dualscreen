package io.github.sirbilliams.lmrecompiled.companion;

/** One read of the watched values. Values are unsigned 32-bit, returned as long. */
public final class MemorySnapshot {
    private final int[] raw;

    public MemorySnapshot(int[] raw) { this.raw = raw != null ? raw : new int[0]; }

    /** Changes every game frame; 0 means no game is running. */
    public int serial() { return raw.length == 0 ? 0 : raw[0]; }

    public boolean isValid(int index) {
        int i = 2 + index * 2;
        return i < raw.length && raw[i] != 0;
    }

    /** The watched value, or null if the read failed or the pointer wasn't valid. */
    public Long value(int index) {
        if (!isValid(index)) return null;
        return raw[1 + index * 2] & 0xFFFFFFFFL;
    }
}
