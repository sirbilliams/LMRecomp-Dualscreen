package io.github.sirbilliams.lmrecompiled.companion;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.os.SystemClock;
import android.text.TextPaint;
import android.text.TextUtils;
import android.util.Log;
import android.view.MotionEvent;

import java.io.File;
import java.text.NumberFormat;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;

/**
 * The Luigi's Mansion bottom screen (ported from the Dolphin Dualscreen companion).
 *
 * Layout (portrait, as on the Thor): room name and floor at the top, the floor map in the middle
 * with Luigi's room marked, and health / Boos / treasure / keys below. On a wide display the status
 * column moves to the right of the map.
 *
 * Floor images are read from the app's data folder, companion/LuigisMansion/ (floor_1.png, f2.png,
 * f3.png, b1.png, roof.png, lab.png; visible in the Files app under "LM Recompiled Data"). Without
 * them the map is drawn as room names at their positions.
 *
 * Tap the map to look at another floor; tap the top bar (or wait) to follow Luigi again. The
 * buttons along the bottom open the F1 options menu on the main screen and pause (Start).
 */
final class LuigisMansionView extends CompanionView {
    private static final long FRAME_MS = 33;
    private static final long PULSE_MS = 1400;
    private static final long MANUAL_TIMEOUT_MS = 20_000;

    private final LuigisMansionCompanion game;
    private final LmRoomTable table;
    private final float density;
    private final NumberFormat numberFormat = NumberFormat.getIntegerInstance(Locale.US);

    private LuigisMansionCompanion.State state;
    private LmRoomTable.LmRoom lastRoom;
    private String lastFloorKey;

    // Manual floor browsing.
    private String manualFloor;
    private long manualUntil;

    private final HashMap<String, Bitmap> bitmaps = new HashMap<>();
    private final File imageFolder;

    // Hit areas, updated in onDraw.
    private final RectF headerRect = new RectF();
    private final RectF mapRect = new RectF();
    private final RectF menuButton = new RectF();
    private final RectF pauseButton = new RectF();
    private RectF pressed;
    private long pressedUntil;

    private final int bg = Color.rgb(11, 14, 20);
    private final int panel = Color.rgb(22, 27, 36);
    private final int green = Color.rgb(76, 195, 90);
    private final int textMain = Color.rgb(240, 242, 245);
    private final int textDim = Color.rgb(140, 148, 160);
    private final int heartRed = Color.rgb(229, 72, 77);
    private final int gold = Color.rgb(242, 193, 78);
    private final int booBlue = Color.rgb(201, 212, 255);

    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint bitmapPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final TextPaint titlePaint = text(true);
    private final TextPaint bodyPaint = text(false);
    private final TextPaint labelPaint = text(true);
    private final TextPaint valuePaint = text(true);

    LuigisMansionView(Context context, LuigisMansionCompanion game) {
        super(context);
        this.game = game;
        this.table = LmRoomTable.load(context);
        this.density = getResources().getDisplayMetrics().density;
        this.imageFolder = new File(new File(new File(context.getFilesDir(), "data"), "companion"), "LuigisMansion");
        stroke.setStyle(Paint.Style.STROKE);
        setBackgroundColor(bg);
        StringBuilder sb = new StringBuilder();
        for (LmRoomTable.LmMap m : table.maps.values()) {
            if (sb.length() > 0) sb.append(", ");
            sb.append(m.image).append('=').append(new File(imageFolder, m.image).isFile());
        }
        Log.i(CompanionScreenController.TAG, "Luigi's Mansion screen: " + table.rooms.size() + " rooms; map images in "
                + imageFolder.getPath() + " " + sb);
    }

    private static TextPaint text(boolean bold) {
        TextPaint p = new TextPaint(Paint.ANTI_ALIAS_FLAG);
        p.setTypeface(bold ? Typeface.DEFAULT_BOLD : Typeface.DEFAULT);
        return p;
    }

    private float dp(float v) { return v * density; }

    @Override
    public void update(MemorySnapshot snapshot) {
        LuigisMansionCompanion.State s = game.parse(snapshot);
        state = s;
        if (s.inGame && s.mapId != null) {
            LmRoomTable.LmRoom room = s.roomId != null ? table.room(s.mapId, s.roomId) : null;
            if (room != null) lastRoom = room;
            String floor = room != null ? room.floorKey() : table.areaFloor(s.mapId);
            if (floor != null) lastFloorKey = floor;
            if (room == null && s.mapId != 2) lastRoom = null;
        }
        invalidate();
    }

    // --- Touch ------------------------------------------------------------------------------

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (event.getActionMasked() != MotionEvent.ACTION_UP) return true;
        if (menuButton.contains(event.getX(), event.getY())) {
            CompanionNative.toggleMenu();
            flash(menuButton);
        } else if (pauseButton.contains(event.getX(), event.getY())) {
            CompanionNative.pressButtons(CompanionNative.PAD_BUTTON_START, 4);
            flash(pauseButton);
        } else if (headerRect.contains(event.getX(), event.getY())) {
            manualFloor = null;
        } else if (mapRect.contains(event.getX(), event.getY()) && !table.maps.isEmpty()) {
            List<String> keys = new ArrayList<>(table.maps.keySet());
            String current = shownFloorKey();
            if (current == null) current = keys.get(0);
            manualFloor = keys.get((keys.indexOf(current) + 1) % keys.size());
            manualUntil = SystemClock.uptimeMillis() + MANUAL_TIMEOUT_MS;
        }
        invalidate();
        return true;
    }

    private void flash(RectF button) {
        pressed = button;
        pressedUntil = SystemClock.uptimeMillis() + 150;
    }

    private String shownFloorKey() {
        if (manualFloor != null && SystemClock.uptimeMillis() > manualUntil) manualFloor = null;
        return manualFloor != null ? manualFloor : lastFloorKey;
    }

    // --- Drawing ----------------------------------------------------------------------------

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float w = getWidth(), h = getHeight();
        float pad = dp(10f);

        // Touch buttons along the bottom edge, on every screen.
        float barH = Math.min(dp(52f), h * 0.12f);
        drawButtons(canvas, new RectF(pad, h - barH - pad * 0.5f, w - pad, h - pad * 0.5f));
        h -= barH + pad * 0.5f;

        LuigisMansionCompanion.State s = state;
        if (s == null || (s.mapId == null && lastRoom == null)) {
            drawWaiting(canvas, w, h);
            postInvalidateDelayed(FRAME_MS * 6);
            return;
        }

        boolean wide = w > h * 1.15f;
        float headerH = Math.min(dp(64f), h * 0.16f);
        headerRect.set(0f, 0f, w, headerH);
        drawHeader(canvas, s, new RectF(pad, pad * 0.6f, w - pad, headerH));

        if (wide) {
            float statusW = w * 0.32f;
            mapRect.set(pad, headerH + pad * 0.5f, w - statusW - pad, h - pad);
            drawMap(canvas, mapRect);
            drawStatus(canvas, s, new RectF(w - statusW, headerH + pad * 0.5f, w - pad, h - pad), 1);
        } else {
            float statusH = Math.min(h * 0.34f, dp(230f));
            mapRect.set(pad, headerH + pad * 0.5f, w - pad, h - statusH - pad);
            drawMap(canvas, mapRect);
            drawStatus(canvas, s, new RectF(pad, h - statusH, w - pad, h - pad), 2);
        }

        // Keep the marker pulsing (and the manual-browse countdown ticking).
        postInvalidateDelayed(FRAME_MS);
    }

    private void drawButtons(Canvas canvas, RectF r) {
        float gap = dp(8f);
        float bw = (r.width() - gap) / 2;
        menuButton.set(r.left, r.top, r.left + bw, r.bottom);
        pauseButton.set(r.left + bw + gap, r.top, r.right, r.bottom);
        if (pressed != null && SystemClock.uptimeMillis() > pressedUntil) pressed = null;
        drawButton(canvas, menuButton, "Menu", pressed == menuButton);
        drawButton(canvas, pauseButton, "Pause", pressed == pauseButton);
    }

    private void drawButton(Canvas canvas, RectF r, String label, boolean down) {
        fill.setColor(down ? Color.argb(90, Color.red(green), Color.green(green), Color.blue(green)) : panel);
        canvas.drawRoundRect(r, dp(10f), dp(10f), fill);
        labelPaint.setColor(down ? green : textMain);
        labelPaint.setTextSize(Math.min(dp(16f), r.height() * 0.38f));
        labelPaint.setTextAlign(Paint.Align.CENTER);
        canvas.drawText(label, r.centerX(), r.centerY() + labelPaint.getTextSize() * 0.35f, labelPaint);
        labelPaint.setTextAlign(Paint.Align.LEFT);
    }

    private void drawWaiting(Canvas canvas, float w, float h) {
        titlePaint.setColor(textMain);
        titlePaint.setTextSize(Math.min(w, h) * 0.07f);
        titlePaint.setTextAlign(Paint.Align.CENTER);
        canvas.drawText("Luigi's Mansion", w / 2, h / 2 - titlePaint.getTextSize() * 0.2f, titlePaint);
        bodyPaint.setColor(textDim);
        bodyPaint.setTextSize(titlePaint.getTextSize() * 0.5f);
        bodyPaint.setTextAlign(Paint.Align.CENTER);
        canvas.drawText("Waiting for the mansion…", w / 2, h / 2 + bodyPaint.getTextSize() * 1.6f, bodyPaint);
        titlePaint.setTextAlign(Paint.Align.LEFT);
        bodyPaint.setTextAlign(Paint.Align.LEFT);
    }

    private void drawHeader(Canvas canvas, LuigisMansionCompanion.State s, RectF r) {
        LmRoomTable.LmRoom room = lastRoom;
        String area = s.mapId != null ? table.areaName(s.mapId) : null;
        String title;
        if (room != null && s.mapId != null && (s.mapId == 2 || s.mapId == room.mapId)) title = room.name;
        else if (area != null) title = area;
        else title = "Luigi's Mansion";
        LmRoomTable.LmMap floorMap = lastFloorKey != null ? table.maps.get(lastFloorKey) : null;
        StringBuilder sub = new StringBuilder();
        if (floorMap != null) sub.append(floorMap.label);
        if (area != null && !area.equals(title)) {
            if (sub.length() > 0) sub.append(" · ");
            sub.append(area);
        }

        // Status chip on the right.
        String shown = shownFloorKey();
        boolean manual = manualFloor != null && shown != null;
        String chip;
        if (manual) {
            LmRoomTable.LmMap m = table.maps.get(shown);
            chip = "Viewing " + (m != null ? m.label : shown) + " · tap to follow";
        } else if (!s.inGame) {
            chip = "Paused / loading";
        } else {
            chip = "Following Luigi";
        }
        labelPaint.setTextSize(r.height() * 0.24f);
        float chipW = labelPaint.measureText(chip) + dp(16f);
        float chipH = labelPaint.getTextSize() * 1.8f;
        RectF chipRect = new RectF(r.right - chipW, r.top + (r.height() - chipH) / 2, r.right, r.top + (r.height() + chipH) / 2);
        boolean dim = manual || !s.inGame;
        fill.setColor(dim ? panel : Color.argb(50, Color.red(green), Color.green(green), Color.blue(green)));
        canvas.drawRoundRect(chipRect, chipH / 2, chipH / 2, fill);
        labelPaint.setColor(dim ? textDim : green);
        canvas.drawText(chip, chipRect.left + dp(8f), chipRect.centerY() + labelPaint.getTextSize() * 0.35f, labelPaint);

        float textW = r.width() - chipW - dp(10f);
        titlePaint.setColor(textMain);
        titlePaint.setTextSize(r.height() * 0.46f);
        String t = TextUtils.ellipsize(title, titlePaint, textW, TextUtils.TruncateAt.END).toString();
        canvas.drawText(t, r.left, r.top + titlePaint.getTextSize(), titlePaint);
        if (sub.length() > 0) {
            bodyPaint.setColor(textDim);
            bodyPaint.setTextSize(r.height() * 0.26f);
            canvas.drawText(sub.toString(), r.left, r.top + titlePaint.getTextSize() + bodyPaint.getTextSize() * 1.35f, bodyPaint);
        }
    }

    private void drawMap(Canvas canvas, RectF r) {
        fill.setColor(panel);
        float corner = dp(10f);
        canvas.drawRoundRect(r, corner, corner, fill);

        String key = shownFloorKey();
        LmRoomTable.LmMap map = key != null ? table.maps.get(key) : null;
        if (map == null) {
            bodyPaint.setColor(textDim);
            bodyPaint.setTextSize(dp(14f));
            bodyPaint.setTextAlign(Paint.Align.CENTER);
            canvas.drawText("No map for this area", r.centerX(), r.centerY(), bodyPaint);
            bodyPaint.setTextAlign(Paint.Align.LEFT);
            return;
        }

        // Fit the map's coordinate space into r, keeping its aspect ratio.
        float inset = dp(6f);
        float scale = Math.min((r.width() - inset * 2) / map.width, (r.height() - inset * 2) / map.height);
        float dw = map.width * scale, dh = map.height * scale;
        float ox = r.left + (r.width() - dw) / 2, oy = r.top + (r.height() - dh) / 2;

        Bitmap bitmap = bitmapFor(map);
        if (bitmap != null) canvas.drawBitmap(bitmap, null, new RectF(ox, oy, ox + dw, oy + dh), bitmapPaint);
        else drawSchematic(canvas, map, ox, oy, scale);

        // Floor label in the corner.
        labelPaint.setColor(textDim);
        labelPaint.setTextSize(dp(11f));
        canvas.drawText(map.label, r.left + dp(8f), r.bottom - dp(8f), labelPaint);

        // Luigi's room.
        LmRoomTable.LmRoom room = lastRoom;
        if (room != null && room.hasMarker() && room.map.equals(key)) {
            float cx = ox + room.x * scale, cy = oy + room.y * scale;
            float base = Math.max(dp(7f), Math.min(dw, dh) * 0.025f);
            float phase = (SystemClock.uptimeMillis() % PULSE_MS) / (float) PULSE_MS;
            stroke.setColor(green);
            stroke.setAlpha((int) ((1f - phase) * 200));
            stroke.setStrokeWidth(dp(2.5f));
            canvas.drawCircle(cx, cy, base * (1.2f + phase * 1.6f), stroke);
            fill.setColor(Color.WHITE);
            canvas.drawCircle(cx, cy, base * 1.15f, fill);
            fill.setColor(green);
            canvas.drawCircle(cx, cy, base * (0.85f + 0.08f * (float) Math.sin(phase * 6.283f)), fill);
        }
    }

    /** Draws the floor as room names at their marker positions (used when no image is present). */
    private void drawSchematic(Canvas canvas, LmRoomTable.LmMap map, float ox, float oy, float scale) {
        bodyPaint.setTextSize(Math.max(dp(8f), Math.min(dp(11f), map.width * scale / 34f)));
        bodyPaint.setTextAlign(Paint.Align.CENTER);
        String currentName = lastRoom != null ? lastRoom.name : null;
        for (LmRoomTable.LmRoom room : table.markersOn(map.key)) {
            float cx = ox + room.x * scale, cy = oy + room.y * scale;
            boolean current = room.name.equals(currentName);
            fill.setColor(current ? green : Color.rgb(70, 80, 95));
            canvas.drawCircle(cx, cy, dp(3f), fill);
            bodyPaint.setColor(current ? textMain : textDim);
            canvas.drawText(shortName(room.name), cx, cy - dp(6f), bodyPaint);
        }
        bodyPaint.setTextAlign(Paint.Align.LEFT);
    }

    private static String shortName(String name) {
        if (name.endsWith(" Room")) name = name.substring(0, name.length() - 5);
        if (name.endsWith("'s")) name = name.substring(0, name.length() - 2);
        return name.replace("Hallway", "Hall");
    }

    /** Loads each floor image once; a missing image is remembered as null and not retried. */
    private Bitmap bitmapFor(LmRoomTable.LmMap map) {
        if (bitmaps.containsKey(map.key)) return bitmaps.get(map.key);
        File f = new File(imageFolder, map.image);
        Bitmap bitmap = null;
        if (f.isFile()) {
            try {
                bitmap = BitmapFactory.decodeFile(f.getPath());
            } catch (RuntimeException e) {
                Log.e(CompanionScreenController.TAG, "Couldn't load " + f.getPath(), e);
            }
        }
        bitmaps.put(map.key, bitmap);
        return bitmap;
    }

    private static final class Tile {
        final String label, value;
        final int color;

        Tile(String label, String value, int color) {
            this.label = label;
            this.value = value;
            this.color = color;
        }
    }

    private void drawStatus(Canvas canvas, LuigisMansionCompanion.State s, RectF r, int columns) {
        Tile[] tiles = {
                new Tile("HEALTH", s.health != null ? s.health.toString() : null, heartRed),
                new Tile("BOOS", s.boos != null ? s.boos + " / 50" : null, booBlue),
                new Tile("TREASURE", s.treasure != null ? numberFormat.format(s.treasure) + " G" : null, gold),
                new Tile("KEYS", s.keys != null ? String.valueOf(s.keys.size()) : null, green),
        };
        float gap = dp(8f);
        int rows = (tiles.length + columns - 1) / columns;
        int keysLines = s.keys == null || s.keys.isEmpty() ? 0 : 2;
        float keysH = keysLines > 0 ? dp(15f) * keysLines + gap : 0f;
        float tileW = (r.width() - gap * (columns - 1)) / columns;
        float tileH = Math.min((r.height() - keysH - gap * (rows - 1)) / rows, dp(78f));

        for (int i = 0; i < tiles.length; i++) {
            int col = i % columns, row = i / columns;
            float left = r.left + col * (tileW + gap), top = r.top + row * (tileH + gap);
            drawTile(canvas, tiles[i], new RectF(left, top, left + tileW, top + tileH));
        }

        if (keysLines > 0) {
            bodyPaint.setColor(textDim);
            bodyPaint.setTextSize(dp(11.5f));
            StringBuilder names = new StringBuilder("Keys: ");
            for (int i = 0; i < s.keys.size(); i++) {
                String k = s.keys.get(i);
                if (k.endsWith(" Key")) k = k.substring(0, k.length() - 4);
                if (i > 0) names.append(", ");
                names.append(k);
            }
            float top = r.top + rows * tileH + (rows - 1) * gap + gap + bodyPaint.getTextSize();
            String text = TextUtils.ellipsize(names, bodyPaint, r.width() * keysLines - dp(30f), TextUtils.TruncateAt.END).toString();
            // Simple two-line wrap.
            int cut = bodyPaint.breakText(text, true, r.width(), null);
            canvas.drawText(text, 0, cut, r.left, top, bodyPaint);
            if (cut < text.length()) {
                canvas.drawText(text, cut, text.length(), r.left, top + bodyPaint.getTextSize() * 1.3f, bodyPaint);
            }
        }
    }

    private void drawTile(Canvas canvas, Tile t, RectF r) {
        fill.setColor(panel);
        canvas.drawRoundRect(r, dp(10f), dp(10f), fill);
        fill.setColor(t.color);
        canvas.drawRoundRect(new RectF(r.left, r.top + dp(10f), r.left + dp(4f), r.bottom - dp(10f)), dp(2f), dp(2f), fill);

        labelPaint.setColor(textDim);
        labelPaint.setTextSize(Math.min(dp(11f), r.height() * 0.2f));
        canvas.drawText(t.label, r.left + dp(14f), r.top + r.height() * 0.36f, labelPaint);

        valuePaint.setColor(t.value == null ? textDim : textMain);
        valuePaint.setTextSize(Math.min(dp(26f), r.height() * 0.38f));
        String v = TextUtils.ellipsize(t.value != null ? t.value : "—", valuePaint, r.width() - dp(20f),
                TextUtils.TruncateAt.END).toString();
        canvas.drawText(v, r.left + dp(14f), r.top + r.height() * 0.78f, valuePaint);
    }
}
