package io.github.sirbilliams.lmrecompiled.companion;

import android.content.Context;
import android.view.View;

/** The view shown on the companion display. Called on the main thread. */
public abstract class CompanionView extends View {
    protected CompanionView(Context context) { super(context); }

    public abstract void update(MemorySnapshot snapshot);
}
