package tech.clyde.sonic2recomp;

import android.app.Presentation;
import android.content.Context;
import android.graphics.Typeface;
import android.os.Bundle;
import android.util.TypedValue;
import android.view.Display;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.GridLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * The dev panel shown on the Thor's bottom screen (Screen-2, 1240x1080).
 * Two pages: a live status HUD (minimap + rings/lives/emeralds) and the
 * warp/settings page (zone grid, boss warps, ws toggle, savestates).
 *
 * The window is FLAG_NOT_FOCUSABLE: it still receives touch on its own
 * display, but can never become the top-focused window — otherwise
 * tapping the panel moves display focus to the bottom screen and Android
 * deprioritizes the game's window (visible as frame/audio stutter until
 * the top screen is tapped again).
 * All views are built programmatically — this is dev tooling, no XML.
 */
final class DevPanelPresentation extends Presentation {

    // Genesis-era palette: deep night blue, Sonic blue, ring gold,
    // Robotnik red.
    private static final int BG        = 0xFF0A1030;
    private static final int CARD_BG   = 0xFF15234F;
    private static final int BTN_BG    = 0xFF2450B4;
    private static final int BTN_RISKY = 0xFF7A5A16;
    private static final int BTN_BOSS  = 0xFF8C2424;
    private static final int FG        = 0xFFF2F5FF;
    private static final int FG_DIM    = 0xFF93A4CE;
    private static final int ACCENT    = 0xFFFFC81E;

    private final DebugClient client;

    private TextView statusLine;      // connection + game state
    private TextView progressLine;    // warp progress / results
    private Button releaseBtn;
    private Button cancelBtn;
    private final List<Button> actionButtons = new ArrayList<>();

    private final Button[] tabButtons = new Button[2];
    private final View[] pages = new View[2];

    private MinimapView minimap;
    private TextView ringsVal, livesVal, emerVal, scoreVal, timeVal, devLine;

    private DebugClient.State connState = DebugClient.State.RELEASED;
    private boolean warping = false;

    DevPanelPresentation(Context outerContext, Display display, DebugClient client) {
        super(outerContext, display);
        this.client = client;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
                | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE);

        LinearLayout root = new LinearLayout(getContext());
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BG);
        int pad = dp(12);
        root.setPadding(pad, pad, pad, pad);

        root.addView(buildStatusRow());
        progressLine = text("", 13, FG_DIM, true);
        progressLine.setPadding(dp(4), 0, dp(4), dp(6));
        root.addView(progressLine);

        FrameLayout content = new FrameLayout(getContext());
        pages[0] = buildStatusPage();
        pages[1] = buildWarpPage();
        content.addView(pages[0]);
        content.addView(pages[1]);
        root.addView(content, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f));

        setContentView(root);
        selectTab(client.selectedTab);
        applyState();
    }

    // ---- state entry points (main thread, from MainActivity's listener) ----

    void setConnectionState(DebugClient.State st) {
        connState = st;
        if (st != DebugClient.State.CONNECTED) warping = false;
        applyState();
    }

    void setSnapshot(DebugClient.GameSnapshot s) {
        boolean inLevel = s.gameMode == 0x0C;
        if (statusLine != null && connState == DebugClient.State.CONNECTED)
            statusLine.setText(String.format("connected · GM=%02X · %s",
                    s.gameMode, inLevel ? Zones.pretty(s.zoneAct) : "—"));
        if (ringsVal == null) return;
        if (inLevel) {
            ringsVal.setText(String.valueOf(s.rings));
            livesVal.setText(String.valueOf(s.lives));
            emerVal.setText(s.emeralds + "/7");
            scoreVal.setText(String.valueOf(s.score));
            timeVal.setText(String.format(Locale.US, "%d:%02d", s.timeMin, s.timeSec));
            devLine.setText(String.format("pos %04X,%04X · cam %04X,%04X%s",
                    s.playerX, s.playerY, s.camX, s.camY,
                    s.superActive ? " · SUPER" : ""));
        } else {
            ringsVal.setText("—"); livesVal.setText("—"); emerVal.setText("—");
            scoreVal.setText("—"); timeVal.setText("—");
            devLine.setText("");
        }
        minimap.setSnapshot(s);
    }

    void setLayoutData(byte[] fgChunks, int zoneAct) {
        if (minimap != null) minimap.setLayoutData(fgChunks, zoneAct);
    }

    void setWarpBusy(boolean busy) {
        warping = busy;
        applyState();
    }

    void showProgress(String msg) {
        if (progressLine != null) progressLine.setText(msg);
    }

    private void applyState() {
        boolean ready = connState == DebugClient.State.CONNECTED && !warping;
        for (Button b : actionButtons) {
            b.setEnabled(ready);
            b.setAlpha(ready ? 1f : 0.35f);
        }
        if (cancelBtn != null) cancelBtn.setVisibility(warping ? View.VISIBLE : View.GONE);
        if (releaseBtn != null)
            releaseBtn.setText(connState == DebugClient.State.RELEASED ? "Connect" : "Release");
        if (statusLine != null && connState != DebugClient.State.CONNECTED) {
            switch (connState) {
                case SERVER_OFF: statusLine.setText("debug server off — put debug.ini in the app files dir"); break;
                case BUSY:       statusLine.setText("server busy — another client holds the connection"); break;
                case CONNECTING: statusLine.setText("connecting…"); break;
                case RELEASED:   statusLine.setText("released — tap Connect to reattach"); break;
            }
        }
    }

    // ---- tabs (icon buttons inline with the status row) ---------------------

    private void selectTab(int idx) {
        client.selectedTab = idx;
        for (int i = 0; i < 2; i++) {
            boolean sel = i == idx;
            pages[i].setVisibility(sel ? View.VISIBLE : View.GONE);
            tabButtons[i].setBackgroundColor(sel ? ACCENT : CARD_BG);
        }
    }

    // ---- status page ----------------------------------------------------------

    private View buildStatusPage() {
        LinearLayout page = new LinearLayout(getContext());
        page.setOrientation(LinearLayout.VERTICAL);

        minimap = new MinimapView(getContext());
        minimap.setBackgroundColor(CARD_BG);
        minimap.setZoom(client.mapZoom);
        minimap.setOnClickListener(v -> {
            client.mapZoom = !client.mapZoom;
            minimap.setZoom(client.mapZoom);
        });
        LinearLayout.LayoutParams mapLp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f);
        mapLp.setMargins(0, dp(8), 0, dp(8));
        page.addView(minimap, mapLp);

        LinearLayout stats = new LinearLayout(getContext());
        stats.setOrientation(LinearLayout.HORIZONTAL);
        ringsVal = addStatCell(stats, "RINGS");
        livesVal = addStatCell(stats, "LIVES");
        emerVal  = addStatCell(stats, "EMERALDS");
        scoreVal = addStatCell(stats, "SCORE");
        timeVal  = addStatCell(stats, "TIME");
        page.addView(stats);

        devLine = text("", 13, FG_DIM, true);
        devLine.setPadding(dp(6), dp(8), dp(6), 0);
        page.addView(devLine);
        return page;
    }

    private TextView addStatCell(LinearLayout row, String label) {
        LinearLayout cell = new LinearLayout(getContext());
        cell.setOrientation(LinearLayout.VERTICAL);
        cell.setBackgroundColor(CARD_BG);
        cell.setGravity(Gravity.CENTER_HORIZONTAL);
        int p = dp(8);
        cell.setPadding(p, p, p, p);
        TextView lab = text(label, 12, FG_DIM, false);
        lab.setGravity(Gravity.CENTER_HORIZONTAL);
        cell.addView(lab);
        TextView val = text("—", 30, FG, true);
        val.setGravity(Gravity.CENTER_HORIZONTAL);
        cell.addView(val);
        row.addView(cell, barLp(1f));
        return val;
    }

    // ---- warp page ------------------------------------------------------------

    private View buildWarpPage() {
        LinearLayout page = new LinearLayout(getContext());
        page.setOrientation(LinearLayout.VERTICAL);
        ScrollView scroll = new ScrollView(getContext());
        scroll.addView(buildZoneGrid());
        page.addView(scroll, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f));
        page.addView(buildBottomBars());   // pinned below the scrolling grid
        TextView credit = text(
                "app icon by LexiLoo826 (deviantart.com) · CC BY-NC-ND 3.0"
                + " · level maps: sonicgalaxy.net",
                11, FG_DIM, false);
        credit.setGravity(Gravity.CENTER_HORIZONTAL);
        credit.setPadding(0, dp(8), 0, 0);
        page.addView(credit);
        return page;
    }

    private View buildStatusRow() {
        LinearLayout row = new LinearLayout(getContext());
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        statusLine = text("starting…", 15, FG, true);
        row.addView(statusLine, new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        cancelBtn = button("Cancel warp", BTN_BOSS, v -> client.cancelWarp());
        cancelBtn.setVisibility(View.GONE);
        row.addView(cancelBtn);
        String[] icons = {"🗺", "⚙"};   // Status (map) / Warp+settings (gear)
        for (int i = 0; i < 2; i++) {
            final int idx = i;
            Button b = button(icons[i], CARD_BG, v -> selectTab(idx));
            b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 22);
            tabButtons[i] = b;
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                    dp(76), LinearLayout.LayoutParams.WRAP_CONTENT);
            lp.setMargins(dp(6), 0, 0, 0);
            row.addView(b, lp);
        }
        return row;
    }

    private View buildZoneGrid() {
        GridLayout grid = new GridLayout(getContext());
        grid.setColumnCount(3);
        for (Zones.Zone z : Zones.ALL) {
            LinearLayout card = new LinearLayout(getContext());
            card.setOrientation(LinearLayout.VERTICAL);
            card.setBackgroundColor(CARD_BG);
            int p = dp(8);
            card.setPadding(p, p, p, p);

            card.addView(text(z.longName, 16, FG, true));
            if (z.risky)
                card.addView(text("may crash", 11, 0xFFD9A441, false));

            LinearLayout acts = new LinearLayout(getContext());
            acts.setOrientation(LinearLayout.HORIZONTAL);
            for (int a = 1; a <= z.acts; a++) {
                final int word = Zones.zoneActWord(z, a);
                Button b = button(String.valueOf(a),
                        z.risky ? BTN_RISKY : BTN_BG,
                        v -> startWarp(word, null));
                actionButtons.add(b);
                acts.addView(b, actLp());
                int[] boss = Zones.BOSS_SPOTS.get(word);
                if (boss != null) {
                    Button bb = button("★", BTN_BOSS, v -> startWarp(word, boss));
                    bb.setTextColor(ACCENT);   // gold star on Robotnik red
                    actionButtons.add(bb);
                    acts.addView(bb, actLp());
                }
            }
            card.addView(acts);

            GridLayout.LayoutParams lp = new GridLayout.LayoutParams();
            lp.width = 0;
            lp.columnSpec = GridLayout.spec(GridLayout.UNDEFINED, 1f);
            lp.setMargins(dp(3), dp(3), dp(3), dp(3));
            grid.addView(card, lp);
        }
        return grid;
    }

    private View buildBottomBars() {
        LinearLayout bars = new LinearLayout(getContext());
        bars.setOrientation(LinearLayout.VERTICAL);
        bars.setPadding(0, dp(8), 0, 0);

        LinearLayout row1 = new LinearLayout(getContext());
        Button ws = button("16:9 ⇄ 4:3", BTN_BG, v -> {
            boolean on = !client.wsOn;
            client.requestWsSet(on);
            showProgress("widescreen " + (on ? "ON" : "OFF"));
        });
        actionButtons.add(ws);
        row1.addView(ws, barLp(2f));
        for (int s = 1; s <= 4; s++) {
            final int slot = s;
            Button b = button("Save " + s, BTN_BG, v -> {
                client.requestSaveState(slot);
                showProgress("saved slot " + slot);
            });
            actionButtons.add(b);
            row1.addView(b, barLp(1f));
        }
        bars.addView(row1);

        LinearLayout row2 = new LinearLayout(getContext());
        releaseBtn = button("Release", BTN_BG, v -> {
            if (connState == DebugClient.State.RELEASED) client.start();
            else client.release();
        });
        row2.addView(releaseBtn, barLp(2f));
        for (int s = 1; s <= 4; s++) {
            final int slot = s;
            Button b = button("Load " + s, BTN_BG, v -> {
                client.requestLoadState(slot);
                showProgress("loaded slot " + slot);
            });
            actionButtons.add(b);
            row2.addView(b, barLp(1f));
        }
        bars.addView(row2);
        return bars;
    }

    private void startWarp(int word, int[] boss) {
        setWarpBusy(true);
        showProgress("");
        client.requestWarp(word, boss);
    }

    // ---- tiny view helpers --------------------------------------------------

    private TextView text(String s, int sp, int color, boolean bold) {
        TextView t = new TextView(getContext());
        t.setText(s);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setTextColor(color);
        t.setTypeface(Typeface.MONOSPACE, bold ? Typeface.BOLD : Typeface.NORMAL);
        return t;
    }

    private Button button(String label, int bg, View.OnClickListener l) {
        Button b = new Button(getContext());
        b.setText(label);
        b.setAllCaps(false);
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        b.setTextColor(FG);
        b.setTypeface(Typeface.DEFAULT_BOLD);
        b.setBackgroundColor(bg);
        b.setMinHeight(dp(52));
        b.setMinimumHeight(dp(52));
        // Buttons narrower than the platform default 88dp minWidth clip
        // their label (MTZ's 4-button row) — let weights set the width.
        b.setMinWidth(0);
        b.setMinimumWidth(0);
        b.setPadding(dp(2), 0, dp(2), 0);
        if (l != null) b.setOnClickListener(l);
        return b;
    }

    private LinearLayout.LayoutParams actLp() {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f);
        lp.setMargins(dp(2), dp(4), dp(2), 0);
        return lp;
    }

    private LinearLayout.LayoutParams barLp(float weight) {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                0, LinearLayout.LayoutParams.WRAP_CONTENT, weight);
        lp.setMargins(dp(2), dp(4), dp(2), 0);
        return lp;
    }

    private int dp(int v) {
        return Math.round(v * getContext().getResources().getDisplayMetrics().density);
    }
}
