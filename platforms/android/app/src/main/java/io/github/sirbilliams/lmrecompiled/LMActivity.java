package io.github.sirbilliams.lmrecompiled;

import io.github.sirbilliams.lmrecompiled.companion.CompanionScreenController;

import dev.encounter.borealis.BorealisActivity;

/**
 * Luigi's Mansion Recompiled on Android (docs/ANDROID.md). The shared Borealis activity does the
 * engine side: SDL surface lifecycle, immersive mode, launch arguments ("borealis_argv" extra, used
 * by port::android::relaunch for "--setup"), preferred frame rate.
 *
 * On a device with a second display (the AYN Thor's bottom screen) the companion screen runs there
 * while the activity is in the foreground.
 */
public final class LMActivity extends BorealisActivity {
    private CompanionScreenController companion;

    @Override
    protected void onResume() {
        super.onResume();
        if (companion == null) companion = new CompanionScreenController(this);
        companion.start();
    }

    @Override
    protected void onPause() {
        if (companion != null) companion.stop();
        super.onPause();
    }
}
