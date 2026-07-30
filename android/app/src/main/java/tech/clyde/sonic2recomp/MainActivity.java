package tech.clyde.sonic2recomp;

import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Display;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

/**
 * SDLActivity subclass: the game itself is untouched; this only adds the
 * dev warp panel on a presentation display (the Thor's bottom screen) and
 * owns the DebugClient lifecycle.
 *
 * Lifecycle: panel + connection live between onResume and onPause. SDL
 * pauses the native thread in onPause, so the cmd server stops being
 * polled — holding the socket would only produce timeouts, and releasing
 * it frees the single client slot for host adb-forward tools while the
 * app is backgrounded.
 */
public class MainActivity extends SDLActivity implements DebugClient.Listener {

    private static final String TAG = "S2DevPanel";

    private DisplayManager displayManager;
    private DebugClient client;
    private DevPanelPresentation panel;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private boolean resumed;
    // Seed data for recreated panels (Presentation self-cancels on display
    // config changes, so the panel can be rebuilt mid-session).
    private DebugClient.State lastConnState = DebugClient.State.RELEASED;
    private DebugClient.GameSnapshot lastSnapshot;
    private byte[] lastLayout;
    private int lastLayoutZa = -1;

    private final DisplayManager.DisplayListener displayListener =
            new DisplayManager.DisplayListener() {
        @Override public void onDisplayAdded(int displayId) { showPanelIfPresent(); }
        @Override public void onDisplayRemoved(int displayId) {
            if (panel != null && panel.getDisplay().getDisplayId() == displayId) {
                Log.i(TAG, "panel display " + displayId + " removed");
                dismissPanel();
            }
        }
        @Override public void onDisplayChanged(int displayId) {
            if (panel == null) showPanelIfPresent();
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        displayManager = (DisplayManager) getSystemService(DISPLAY_SERVICE);
        client = new DebugClient(this);
    }

    @Override
    protected void onResume() {
        super.onResume();
        resumed = true;
        displayManager.registerDisplayListener(displayListener, null);
        client.start();
        showPanelIfPresent();
    }

    @Override
    protected void onPause() {
        resumed = false;
        mainHandler.removeCallbacksAndMessages(null);
        client.cancelWarp();
        client.release();
        dismissPanel();
        displayManager.unregisterDisplayListener(displayListener);
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        client.shutdown();
        super.onDestroy();
    }

    // ---- panel management ---------------------------------------------------

    private void showPanelIfPresent() {
        if (panel != null) return;
        Display d = findPresentationDisplay();
        if (d == null) return;   // single-screen device: nothing to do
        if (d.getState() == Display.STATE_OFF)
            Toast.makeText(this, "Wake the bottom screen for the warp panel",
                    Toast.LENGTH_LONG).show();
        Log.i(TAG, "showing warp panel on display " + d.getDisplayId()
                + " (" + d.getName() + ")");
        panel = new DevPanelPresentation(this, d, client);
        // Presentation cancels ITSELF whenever its display reports a config
        // change (the widescreen resize trips one) — recreate it whenever a
        // dismissal wasn't ours, or the panel silently disappears until the
        // next onResume.
        panel.setOnDismissListener(dlg -> {
            panel = null;
            if (resumed) {
                Log.i(TAG, "panel self-dismissed; re-showing");
                mainHandler.postDelayed(this::showPanelIfPresent, 300);
            }
        });
        panel.show();
        panel.setConnectionState(lastConnState);
        if (lastLayout != null) panel.setLayoutData(lastLayout, lastLayoutZa);
        if (lastSnapshot != null) panel.setSnapshot(lastSnapshot);
    }

    private void dismissPanel() {
        if (panel != null) {
            DevPanelPresentation p = panel;
            panel = null;
            p.setOnDismissListener(null);
            p.dismiss();
        }
    }

    private Display findPresentationDisplay() {
        Display[] displays =
                displayManager.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION);
        return displays.length > 0 ? displays[0] : null;
    }

    // ---- DebugClient.Listener (main thread) ----------------------------------

    @Override public void onConnectionState(DebugClient.State state) {
        lastConnState = state;
        if (panel != null) panel.setConnectionState(state);
    }

    @Override public void onSnapshot(DebugClient.GameSnapshot snapshot) {
        lastSnapshot = snapshot;
        if (snapshot.gameMode != 0x0C) { lastLayout = null; lastLayoutZa = -1; }
        if (panel != null) panel.setSnapshot(snapshot);
    }

    @Override public void onLayout(byte[] fgChunks, int zoneAct) {
        lastLayout = fgChunks;
        lastLayoutZa = zoneAct;
        if (panel != null) panel.setLayoutData(fgChunks, zoneAct);
    }

    @Override public void onWarpProgress(String message) {
        if (panel != null) panel.showProgress(message);
    }

    @Override public void onWarpDone(boolean ok, String message) {
        if (panel != null) {
            panel.setWarpBusy(false);
            panel.showProgress((ok ? "✓ " : "✗ ") + message);
        }
    }
}
