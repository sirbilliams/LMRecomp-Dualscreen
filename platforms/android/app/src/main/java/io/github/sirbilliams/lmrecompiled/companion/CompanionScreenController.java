package io.github.sirbilliams.lmrecompiled.companion;

import android.app.Activity;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.WindowManager;

/**
 * Drives the companion (bottom) screen while the game runs (docs/ANDROID.md, "Companion screen").
 *
 * Every POLL_MS it checks which version is running. For a supported one it tells the native side
 * which memory to watch, opens a {@link CompanionPresentation} on the secondary display and feeds it
 * the latest values. On a device without a second display nothing happens.
 */
public final class CompanionScreenController implements DisplayManager.DisplayListener {
    static final String TAG = "LMCompanion";
    private static final long POLL_MS = 100;

    private final Activity activity;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final DisplayManager displayManager;

    private boolean running = false;
    private String gameId = null;
    private LuigisMansionCompanion game = null;
    private CompanionPresentation presentation = null;
    private int lastSerial = 0;
    private boolean loggedDisplays = false;

    private final Runnable tick = new Runnable() {
        @Override
        public void run() {
            if (!running) return;
            try {
                poll();
            } catch (UnsatisfiedLinkError e) {
                // libmain.so not loaded yet.
            } catch (RuntimeException e) {
                Log.e(TAG, "Companion screen update failed", e);
            }
            handler.postDelayed(this, POLL_MS);
        }
    };

    public CompanionScreenController(Activity activity) {
        this.activity = activity;
        this.displayManager = activity.getSystemService(DisplayManager.class);
    }

    public void start() {
        if (running) return;
        running = true;
        displayManager.registerDisplayListener(this, handler);
        handler.post(tick);
    }

    public void stop() {
        if (!running) return;
        running = false;
        handler.removeCallbacks(tick);
        displayManager.unregisterDisplayListener(this);
        dismiss();
    }

    private void poll() {
        String id = CompanionNative.gameId();
        if (id != null && id.isEmpty()) id = null;
        if (!java.util.Objects.equals(id, gameId)) {
            gameId = id;
            game = id != null && LuigisMansionCompanion.supports(id) ? new LuigisMansionCompanion() : null;
            Log.i(TAG, "Game: " + (id != null ? id : "none") + "; companion: " + (game != null ? "Luigi's Mansion" : "none"));
            CompanionNative.setWatches(game != null ? game.buildWatches().toIntArray() : null);
            lastSerial = 0;
            dismiss();
        }
        if (game == null) return;
        CompanionPresentation p = presentation != null ? presentation : show(game);
        if (p == null) return;
        MemorySnapshot snapshot = new MemorySnapshot(CompanionNative.getSnapshot());
        if (snapshot.serial() != lastSerial) {
            lastSerial = snapshot.serial();
            CompanionView v = p.view();
            if (v != null) v.update(snapshot);
        }
    }

    private CompanionPresentation show(LuigisMansionCompanion g) {
        Display display = findCompanionDisplay();
        if (display == null) return null;
        CompanionPresentation p = new CompanionPresentation(activity, display, g::createView);
        try {
            p.show();
            presentation = p;
            Log.i(TAG, "Showing companion screen on display " + display.getDisplayId() + " (" + display.getName() + ")");
            return p;
        } catch (WindowManager.InvalidDisplayException e) {
            Log.e(TAG, "Couldn't show on display " + display.getDisplayId(), e);
            return null;
        }
    }

    private void dismiss() {
        if (presentation != null) {
            try {
                presentation.dismiss();
            } catch (RuntimeException ignored) {
            }
        }
        presentation = null;
    }

    private Display findCompanionDisplay() {
        int own = ownDisplayId();
        Display[] all = displayManager.getDisplays();
        if (!loggedDisplays) {
            loggedDisplays = true;
            Log.i(TAG, "Game is on display " + own + ". Displays:");
            for (Display d : all) {
                DisplayMetrics m = new DisplayMetrics();
                d.getRealMetrics(m);
                Log.i(TAG, "  #" + d.getDisplayId() + " '" + d.getName() + "' " + m.widthPixels + "x" + m.heightPixels
                        + " flags=0x" + Integer.toHexString(d.getFlags()) + " state=" + d.getState());
            }
        }
        for (Display d : displayManager.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION)) {
            if (d.getDisplayId() != own) return d;
        }
        for (Display d : all) {
            if (d.getDisplayId() != own) return d;
        }
        return null;
    }

    @SuppressWarnings("deprecation")
    private int ownDisplayId() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && activity.getDisplay() != null) {
            return activity.getDisplay().getDisplayId();
        }
        return activity.getWindowManager().getDefaultDisplay().getDisplayId();
    }

    @Override
    public void onDisplayAdded(int displayId) {
        Log.i(TAG, "Display added: " + displayId);
        loggedDisplays = false;
    }

    @Override
    public void onDisplayRemoved(int displayId) {
        Log.i(TAG, "Display removed: " + displayId);
        if (presentation != null && presentation.getDisplay().getDisplayId() == displayId) dismiss();
    }

    @Override
    public void onDisplayChanged(int displayId) {}
}
