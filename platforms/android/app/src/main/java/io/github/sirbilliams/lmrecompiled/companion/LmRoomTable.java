package io.github.sirbilliams.lmrecompiled.companion;

import android.content.Context;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Rooms and floors of the mansion, from assets/companion/luigis_mansion_rooms.json. Room and map IDs
 * from the Luigi's Mansion Archipelago client (MIT); marker positions from
 * DevinLB/Luigis_Mansion_Poptracker (MIT), as pixel positions on that pack's floor images.
 */
final class LmRoomTable {
    /** A floor map: an image file name plus its pixel size (the room coordinate space). */
    static final class LmMap {
        final String key, label, image;
        final int width, height;

        LmMap(String key, String label, String image, int width, int height) {
            this.key = key;
            this.label = label;
            this.image = image;
            this.width = width;
            this.height = height;
        }
    }

    /** A room. x/y are pixel positions on map `map`; rooms without a marker only name a floor. */
    static final class LmRoom {
        final int id, mapId;
        final String name, map, floorMap;
        final Integer x, y;

        LmRoom(int id, int mapId, String name, String map, Integer x, Integer y, String floorMap) {
            this.id = id;
            this.mapId = mapId;
            this.name = name;
            this.map = map;
            this.x = x;
            this.y = y;
            this.floorMap = floorMap;
        }

        /** The floor this room is shown on, if known. */
        String floorKey() { return map != null ? map : floorMap; }

        boolean hasMarker() { return map != null && x != null && y != null; }
    }

    final LinkedHashMap<String, LmMap> maps = new LinkedHashMap<>();
    final List<LmRoom> rooms = new ArrayList<>();
    private final Map<Integer, String> areas = new HashMap<>();
    private final Map<Integer, String> areaMaps = new HashMap<>();
    private final Map<Long, LmRoom> byKey = new HashMap<>();

    private static long key(int mapId, int roomId) { return ((long) mapId << 32) | (roomId & 0xFFFFFFFFL); }

    LmRoom room(int mapId, int roomId) { return byKey.get(key(mapId, roomId)); }

    String areaName(int mapId) { return areas.get(mapId); }

    /** Floor to show for an area when there's no room marker (e.g. the lab). */
    String areaFloor(int mapId) { return areaMaps.get(mapId); }

    /** Rooms with a marker on mapKey, for drawing the schematic. Duplicates by name removed. */
    List<LmRoom> markersOn(String mapKey) {
        List<LmRoom> out = new ArrayList<>();
        Set<String> seen = new HashSet<>();
        for (LmRoom r : rooms) {
            if (mapKey.equals(r.map) && r.hasMarker() && seen.add(r.name)) out.add(r);
        }
        return out;
    }

    static LmRoomTable load(Context context) {
        LmRoomTable t = new LmRoomTable();
        try (InputStream in = context.getAssets().open("companion/luigis_mansion_rooms.json")) {
            ByteArrayOutputStream buf = new ByteArrayOutputStream();
            byte[] chunk = new byte[16384];
            int n;
            while ((n = in.read(chunk)) > 0) buf.write(chunk, 0, n);
            JSONObject json = new JSONObject(new String(buf.toByteArray(), StandardCharsets.UTF_8));

            JSONObject mapsJson = json.getJSONObject("maps");
            for (Iterator<String> it = mapsJson.keys(); it.hasNext(); ) {
                String key = it.next();
                JSONObject m = mapsJson.getJSONObject(key);
                t.maps.put(key, new LmMap(key, m.getString("label"), m.getString("image"), m.getInt("width"), m.getInt("height")));
            }
            readIntKeyed(json.optJSONObject("areas"), t.areas);
            readIntKeyed(json.optJSONObject("areaMaps"), t.areaMaps);

            JSONArray roomsJson = json.getJSONArray("rooms");
            for (int i = 0; i < roomsJson.length(); i++) {
                JSONObject r = roomsJson.getJSONObject(i);
                LmRoom room = new LmRoom(
                        r.getInt("id"), r.getInt("mapId"), r.getString("name"),
                        r.has("map") ? r.getString("map") : null,
                        r.has("x") ? r.getInt("x") : null,
                        r.has("y") ? r.getInt("y") : null,
                        r.has("floorMap") ? r.getString("floorMap") : null);
                t.rooms.add(room);
                t.byKey.put(key(room.mapId, room.id), room);
            }
        } catch (IOException | JSONException e) {
            android.util.Log.e(CompanionScreenController.TAG, "Couldn't load the room table", e);
        }
        return t;
    }

    private static void readIntKeyed(JSONObject o, Map<Integer, String> into) throws JSONException {
        if (o == null) return;
        for (Iterator<String> it = o.keys(); it.hasNext(); ) {
            String k = it.next();
            into.put(Integer.parseInt(k), o.getString(k));
        }
    }
}
