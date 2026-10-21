package io.github.sirbilliams.lmrecompiled.companion;

import android.app.Presentation;
import android.content.Context;
import android.os.Bundle;
import android.view.Display;
import android.view.WindowManager;

/**
 * Shows the companion view on the secondary display (the AYN Thor's bottom screen).
 *
 * The window is never focusable, so controller buttons and keys keep going to the game on the main
 * screen (and the game doesn't pause for focus loss). Touch still works on the companion display.
 */
public final class CompanionPresentation extends Presentation {
    public interface ViewFactory {
        CompanionView create(Context context);
    }

    private final ViewFactory factory;
    private CompanionView view;

    public CompanionPresentation(Context outerContext, Display display, ViewFactory factory) {
        super(outerContext, display);
        this.factory = factory;
    }

    public CompanionView view() { return view; }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (getWindow() != null) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                    | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }
        view = factory.create(getContext());
        setContentView(view);
    }
}
