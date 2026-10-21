package io.github.sirbilliams.lmrecompiled;

import dev.encounter.borealis.BorealisActivity;

/**
 * Luigi's Mansion Recompiled on Android (docs/ANDROID.md). The shared Borealis activity does the
 * work: SDL surface lifecycle, immersive mode, launch arguments ("borealis_argv" extra, used by
 * port::android::relaunch for "--setup"), preferred frame rate.
 *
 * The AYN Thor's second screen is added here later (a Presentation on the secondary display).
 */
public final class LMActivity extends BorealisActivity {
}
