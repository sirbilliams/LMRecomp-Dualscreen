package io.github.sirbilliams.lmrecompiled.companion;

import android.content.Context;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Luigi's Mansion (USA, GLME01) companion screen: a mansion map that follows Luigi's room, plus
 * health, Boos captured, treasure and keys. Ported from the Dolphin Dualscreen companion.
 *
 * Memory addresses come from the Luigi's Mansion Archipelago client
 * (https://github.com/BootsinSoots/Archipelago, worlds/luigismansion, MIT license). They are the
 * USA release's; the European release (GLMP01) lays its data out differently.
 */
public final class LuigisMansionCompanion {
    public static boolean supports(String gameId) { return "GLME01".equals(gameId); }

    // 2 when the game is in a playable state (not title screen, loading or a cutscene transition).
    private static final long PLAY_STATE_ADDR = 0x803A3AE4L;
    // 1 = E. Gadd's lab, 2 = mansion, 3 = training room, 6 = portrait gallery, 9/11/13 = bosses.
    private static final long MAP_ID_ADDR = 0x804D80A4L;
    // Pointer to Luigi's data; the current room ID is at +0x35C and the wallet starts at +0x324.
    private static final long LUIGI_PTR = 0x803D8B7CL;
    private static final int ROOM_ID_OFFSET = 0x35C;
    // Pointer whose target holds Luigi's health (16 bits) at +0xB8.
    private static final long HEALTH_PTR = 0x803D8B40L;
    private static final int HEALTH_OFFSET = 0xB8;
    // 8 bytes of Boo capture flags; the number of set bits is the number of Boos caught.
    private static final long BOO_FLAGS_ADDR = 0x803D5E04L;
    // Door key flags live in the 12 bytes from here (read as three 32-bit words).
    private static final long KEY_FLAGS_ADDR = 0x803D5E14L;

    static final class DoorKey {
        final String name;
        final long address;
        final int bit;

        DoorKey(String name, long address, int bit) {
            this.name = name;
            this.address = address;
            this.bit = bit;
        }
    }

    static final List<DoorKey> DOOR_KEYS = Arrays.asList(
            new DoorKey("Heart Key", 0x803D5E14L, 3),
            new DoorKey("Club Key", 0x803D5E19L, 2),
            new DoorKey("Diamond Key", 0x803D5E1BL, 3),
            new DoorKey("Spade Key", 0x803D5E1DL, 0),
            new DoorKey("Parlor Key", 0x803D5E18L, 2),
            new DoorKey("Anteroom Key", 0x803D5E18L, 6),
            new DoorKey("Wardrobe Key", 0x803D5E19L, 3),
            new DoorKey("Family Hallway Key", 0x803D5E18L, 1),
            new DoorKey("Master Bedroom Key", 0x803D5E17L, 7),
            new DoorKey("Nursery Key", 0x803D5E17L, 3),
            new DoorKey("Twins Bedroom Key", 0x803D5E17L, 4),
            new DoorKey("Ballroom Key", 0x803D5E15L, 7),
            new DoorKey("Storage Room Key", 0x803D5E16L, 0),
            new DoorKey("Fortune Teller Key", 0x803D5E14L, 4),
            new DoorKey("Laundry Room Key", 0x803D5E14L, 7),
            new DoorKey("Lower 2F Stairwell Key", 0x803D5E1DL, 2),
            new DoorKey("Conservatory Key", 0x803D5E16L, 5),
            new DoorKey("Dining Room Key", 0x803D5E15L, 6),
            new DoorKey("North Rec Room Key", 0x803D5E17L, 1),
            new DoorKey("Billiards Room Key", 0x803D5E16L, 1),
            new DoorKey("Safari Room Key", 0x803D5E1BL, 0),
            new DoorKey("Balcony Key", 0x803D5E1BL, 6),
            new DoorKey("Cellar Key", 0x803D5E1CL, 4),
            new DoorKey("Clockwork Key", 0x803D5E1AL, 5),
            new DoorKey("Armory Key", 0x803D5E1AL, 3),
            new DoorKey("Sitting Room Key", 0x803D5E17L, 5),
            new DoorKey("Pipe Room Key", 0x803D5E1CL, 5),
            new DoorKey("Cold Storage Key", 0x803D5E1CL, 1),
            new DoorKey("Artist's Studio Key", 0x803D5E1BL, 7),
            new DoorKey("Wardrobe Balcony Key", 0x803D5E19L, 1),
            new DoorKey("Study Key", 0x803D5E18L, 0),
            new DoorKey("Basement Stairwell Key", 0x803D5E15L, 1),
            new DoorKey("1F Bathroom Key", 0x803D5E16L, 7),
            new DoorKey("1F Washroom Key", 0x803D5E16L, 4),
            new DoorKey("Kitchen Key", 0x803D5E15L, 3),
            new DoorKey("Boneyard Key", 0x803D5E15L, 2),
            new DoorKey("Projection Room Key", 0x803D5E16L, 2),
            new DoorKey("Mirror Room Key", 0x803D5E14L, 5),
            new DoorKey("Butler's Room Key", 0x803D5E14L, 1),
            new DoorKey("Tea Room Key", 0x803D5E19L, 7),
            new DoorKey("South Rec Room Key", 0x803D5E17L, 0),
            new DoorKey("Upper 2F Stairwell Key", 0x803D5E1DL, 3),
            new DoorKey("2F Bathroom Key", 0x803D5E1AL, 0),
            new DoorKey("2F Washroom Key", 0x803D5E19L, 5),
            new DoorKey("Nana's Room Key", 0x803D5E1AL, 1),
            new DoorKey("Astral Hall Key", 0x803D5E19L, 4),
            new DoorKey("Observatory Key", 0x803D5E19L, 0),
            new DoorKey("Guest Room Key", 0x803D5E17L, 6),
            new DoorKey("East Attic Hallway Key", 0x803D5E1AL, 7),
            new DoorKey("Telephone Room Key", 0x803D5E1AL, 4),
            new DoorKey("Ceramics Studio Key", 0x803D5E1AL, 2),
            new DoorKey("Breaker Room Key", 0x803D5E1CL, 7),
            new DoorKey("Basement Hallway Key", 0x803D5E1CL, 3),
            new DoorKey("Altar Hallway Key", 0x803D5E1CL, 6)
    );

    /** In-game value of each kind of treasure and where its count is kept (offset from LUIGI_PTR). */
    private static final int[] TREASURE_OFFSETS = {0x324, 0x328, 0x32C, 0x330, 0x334, 0x338, 0x33C, 0x344, 0x348, 0x34C, 0x350};
    private static final long[] TREASURE_VALUES = {5_000, 20_000, 100_000, 500_000, 800_000, 1_000_000, 2_000_000,
            20_000_000, 50_000, 100_000, 1_000_000};

    // Watch indices, filled in by buildWatches().
    private int iPlayState, iMapId, iRoomId, iHealth, iBoos, iKeys, iTreasure;

    public WatchList buildWatches() {
        WatchList w = new WatchList();
        iPlayState = w.u32(PLAY_STATE_ADDR);
        iMapId = w.u32(MAP_ID_ADDR);
        iRoomId = w.deref(LUIGI_PTR, ROOM_ID_OFFSET, 4);
        iHealth = w.deref(HEALTH_PTR, HEALTH_OFFSET, 2);
        iBoos = w.u32(BOO_FLAGS_ADDR);
        w.u32(BOO_FLAGS_ADDR + 4);
        iKeys = w.u32(KEY_FLAGS_ADDR);
        w.u32(KEY_FLAGS_ADDR + 4);
        w.u32(KEY_FLAGS_ADDR + 8);
        iTreasure = w.deref(LUIGI_PTR, TREASURE_OFFSETS[0], 4);
        for (int i = 1; i < TREASURE_OFFSETS.length; i++) w.deref(LUIGI_PTR, TREASURE_OFFSETS[i], 4);
        return w;
    }

    public CompanionView createView(Context context) { return new LuigisMansionView(context, this); }

    /** Decoded game state. Fields are null when the game didn't have a valid value. */
    public static final class State {
        public boolean inGame;
        public Integer mapId, roomId, health, boos;
        public Long treasure;
        public List<String> keys;
    }

    public State parse(MemorySnapshot s) {
        State st = new State();
        Long map = s.value(iMapId);
        st.mapId = map != null ? (int) (long) map : null;
        Long play = s.value(iPlayState);
        st.inGame = play != null && play == 2L;
        Long room = s.value(iRoomId);
        st.roomId = room != null ? (int) (long) room : null;
        Long health = s.value(iHealth);
        // Health reads aren't meaningful in the lab.
        st.health = (st.mapId != null && st.mapId == 1) || health == null ? null : (int) (long) health;

        Long b0 = s.value(iBoos), b1 = s.value(iBoos + 1);
        st.boos = b0 != null && b1 != null ? Long.bitCount(b0) + Long.bitCount(b1) : null;

        Long[] words = {s.value(iKeys), s.value(iKeys + 1), s.value(iKeys + 2)};
        if (words[0] != null && words[1] != null && words[2] != null) {
            List<String> keys = new ArrayList<>();
            for (DoorKey k : DOOR_KEYS) {
                int byteIndex = (int) (k.address - KEY_FLAGS_ADDR);
                long word = words[byteIndex / 4];
                long b = (word >> ((3 - byteIndex % 4) * 8)) & 0xFF;
                if (((b >> k.bit) & 1) == 1) keys.add(k.name);
            }
            st.keys = keys;
        }

        long total = 0;
        boolean ok = true;
        for (int i = 0; i < TREASURE_VALUES.length; i++) {
            Long count = s.value(iTreasure + i);
            if (count == null) {
                ok = false;
                break;
            }
            total += count * TREASURE_VALUES[i];
        }
        st.treasure = ok ? total : null;
        return st;
    }
}
