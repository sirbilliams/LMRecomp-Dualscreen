package io.github.sirbilliams.lmrecompiled.companion;

import java.util.ArrayList;

/** Builds the watch spec for {@link CompanionNative#setWatches}. Each add returns the watch's index. */
public final class WatchList {
    private final ArrayList<Integer> spec = new ArrayList<>();
    private int size = 0;

    public int size() { return size; }

    public int u8(long address) { return add(address, 0, 1, false); }
    public int u16(long address) { return add(address, 0, 2, false); }
    public int u32(long address) { return add(address, 0, 4, false); }

    /** Reads a pointer at {@code pointer}, then {@code bytes} bytes at pointer + offset. */
    public int deref(long pointer, int offset, int bytes) { return add(pointer, offset, bytes, true); }

    private int add(long address, int offset, int bytes, boolean deref) {
        spec.add((int) address);
        spec.add(offset);
        spec.add(bytes);
        spec.add(deref ? 1 : 0);
        return size++;
    }

    public int[] toIntArray() {
        int[] out = new int[spec.size()];
        for (int i = 0; i < out.length; i++) out[i] = spec.get(i);
        return out;
    }
}
