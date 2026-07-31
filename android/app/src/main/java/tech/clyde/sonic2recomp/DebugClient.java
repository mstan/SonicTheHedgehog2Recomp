package tech.clyde.sonic2recomp;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;

import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.Locale;

/**
 * Owner of the ONE debug-server connection (the runner's cmd_server accepts
 * a single client at a time and polls it from the game main loop). All
 * socket work happens on a dedicated HandlerThread ("debug-io"); jobs are
 * inherently serialized by its Handler, matching the server's
 * one-line-at-a-time model. Listener callbacks are posted to the main
 * thread.
 */
final class DebugClient {

    private static final String TAG = "S2DevPanel";
    private static final int PORT = 4380;
    private static final int CONNECT_TIMEOUT_MS = 500;
    private static final int COMMAND_TIMEOUT_MS = 2000;
    private static final int RETRY_MS = 3000;
    private static final int POLL_MS = 300;
    private static final int LAYOUT_REFRESH_MS = 5000;  // HTZ quake edits layout live

    /** Connection state for the panel status line. */
    enum State { RELEASED, CONNECTING, CONNECTED, SERVER_OFF, BUSY }

    /** One poll's worth of game state (all fields live-verified addresses). */
    static final class GameSnapshot {
        int gameMode;        // FFF600
        int zoneAct;         // FFFE10 word
        int lives;           // FFFE12 byte
        int rings;           // FFFE20 word
        int timeMin, timeSec;// FFFE23/24 bytes
        long score;          // FFFE26 long, already x10 for display
        int emeralds;        // FFFEB1 byte, 0-7
        boolean superActive; // FFF65F != 0
        int playerX, playerY;// FFB008/FFB00C words
        int camX, camY;      // FFEE00/FFEE04 words
        boolean invincible;  // FFB02B (status_secondary) bit 1
        boolean speedShoes;  // FFB02B bit 2
    }

    interface Listener {
        void onConnectionState(State state);
        void onSnapshot(GameSnapshot snapshot);        // from the idle poller
        void onLayout(byte[] fgChunks, int zoneAct);   // 128x16 chunk map, FF8000
        void onWarpProgress(String message);
        void onWarpDone(boolean ok, String message);
    }

    private final Listener listener;
    private final Handler main = new Handler(Looper.getMainLooper());
    private final HandlerThread ioThread = new HandlerThread("debug-io");
    private final Handler io;

    // debug-io thread only:
    private Socket socket;
    private BufferedReader in;
    private OutputStream out;
    private int nextId = 1;
    private boolean warping = false;

    // cross-thread:
    private volatile boolean released = true;
    volatile boolean cancelRequested = false;

    DebugClient(Listener listener) {
        this.listener = listener;
        ioThread.start();
        io = new Handler(ioThread.getLooper());
    }

    // ---- lifecycle (any thread) -------------------------------------------

    /** Begin connecting; retries until connected or release()d. */
    void start() {
        released = false;
        io.post(this::connectJob);
    }

    /** Close the socket and stop retrying (Release button / onPause). */
    void release() {
        released = true;
        cancelRequested = true;
        io.post(() -> {
            closeSocket();
            setState(State.RELEASED);
        });
    }

    /** Quit the io thread (onDestroy). */
    void shutdown() {
        release();
        ioThread.quitSafely();
    }

    void cancelWarp() {
        cancelRequested = true;
    }

    // ---- panel actions (any thread) ---------------------------------------

    void requestWarp(int zoneActWord, int[] bossSpotOrNull) {
        cancelRequested = false;
        io.post(() -> {
            if (!ensureConnected()) return;
            warping = true;
            try {
                WarpEngine.run(this, zoneActWord, bossSpotOrNull);
            } finally {
                warping = false;
                schedulePoll(POLL_MS);
            }
        });
    }

    /* Lives here rather than in the panel: the panel is recreated whenever
     * its Presentation self-cancels, and would forget the toggle state. The
     * widescreen build boots armed. */
    volatile boolean wsOn = true;
    /** Selected panel tab (0 = Status, 1 = Warp) — survives panel recreation. */
    volatile int selectedTab = 0;
    /** Minimap zoom (fill height + follow the player) — survives recreation. */
    volatile boolean mapZoom = false;

    void requestWsSet(boolean on) {
        wsOn = on;
        oneShot("ws_set 16:9=" + on, () ->
            cmd(new JSONObject().put("cmd", "ws_set").put("on", on ? 1 : 0)));
    }

    /* Slot files share names with the F-keys / gamepad quicksave, so panel
     * saves and controller quick-loads interoperate. */
    void requestSaveState(int slot) {
        oneShot("save slot " + slot, () -> cmd(new JSONObject()
            .put("cmd", "save_state").put("path", "native_save_" + slot + ".bin")));
    }

    void requestLoadState(int slot) {
        oneShot("load slot " + slot, () -> cmd(new JSONObject()
            .put("cmd", "load_state").put("path", "native_save_" + slot + ".bin")));
    }

    // ---- cheats (RAM pokes; every address live-verified on the Thor) --------

    /** Last in-level zone/act from the poller — the character swap restarts it. */
    private volatile int lastZoneAct = 0x0000;

    void cheatAddLife() {
        oneShot("+1 life", () -> {
            int lives = Math.min(readByte("FFFE12") + 1, 99);
            writeHex("FFFE12", String.format(Locale.ROOT, "%02X", lives));
            writeHex("FFFE1C", "01");                    // Update_HUD_lives
            note("lives = " + lives);
        });
    }

    void cheatAddRings(int n) {
        oneShot("+rings", () -> {
            int rings = Math.min(readWord("FFFE20") + n, 999);
            writeHex("FFFE20", String.format(Locale.ROOT, "%04X", rings));
            writeHex("FFFE1D", "01");                    // Update_HUD_rings
            note("rings = " + rings);
        });
    }

    void cheatEmeralds() {
        oneShot("emeralds", () -> {
            writeHex("FFFEB1", "07");
            note("all 7 emeralds");
        });
    }

    /** Seeds the real transformation: with flag+palette set to 1 the game
     *  advances them itself (flag 1 -> FF, palette fade runs). Rings are
     *  topped up to 50 so super doesn't instantly revert — it drains 1/s
     *  and reverts at 0, authentically. */
    void cheatGoSuper() {
        oneShot("super", () -> {
            if (readByte("FFF600") != 0x0C) { note("not in a level"); return; }
            if (readWord("FFFF72") == 2)    { note("Super needs Sonic"); return; }
            if (readByte("FFF65F") != 0)    { note("already super"); return; }
            if (readWord("FFFE20") < 50) {
                writeHex("FFFE20", "0032");
                writeHex("FFFE1D", "01");
            }
            writeHex("FFFEB1", "07");
            writeHex("FFF65E", "01");                    // Super_Sonic_palette: fade in
            writeHex("FFF65F", "01");                    // Super_Sonic_flag
            writeHex("FFF760", "0A0000300100");          // top speed / accel / decel
            orByte("FFB02B", 0x02);
            note("SUPER");
        });
    }

    /** Permanent while on: the timer stays 0, so the star-monitor countdown
     *  never runs and nothing clears the bit until toggled off. */
    void cheatToggleInvincible() {
        oneShot("invincible", () -> {
            int ss = readByte("FFB02B");
            boolean on = (ss & 0x02) != 0;
            writeHex("FFB02B", String.format(
                    Locale.ROOT, "%02X", on ? ss & ~0x02 : ss | 0x02));
            if (!on) writeHex("FFB032", "0000");
            note(on ? "invincibility off" : "INVINCIBLE");
        });
    }

    /** The authentic 20 s burst — the game restores the speed table itself
     *  when the timer expires. Super's faster table is left alone. */
    void cheatSpeedShoes() {
        oneShot("speed shoes", () -> {
            orByte("FFB02B", 0x04);
            writeHex("FFB034", "04B0");                  // 1200 frames = 20 s
            if (readByte("FFF65F") == 0)
                writeHex("FFF760", "0C000018");          // top C00, accel 18
            note("speed shoes (20 s)");
        });
    }

    /** 0 = Sonic & Tails, 1 = Sonic, 2 = Tails. Player art is loaded at
     *  level init, so this sets Player_option and restarts the current act
     *  through the normal warp flow. */
    void requestCharacter(int option) {
        io.post(() -> {
            if (!ensureConnected()) return;
            try {
                writeHex("FFFF72", String.format(Locale.ROOT, "%04X", option));
            } catch (Exception e) {
                ioTrouble(e);
                return;
            }
            requestWarp(lastZoneAct, null);
        });
    }

    private void orByte(String addr, int bits) throws Exception {
        writeHex(addr, String.format(
                Locale.ROOT, "%02X", readByte(addr) | bits));
    }

    private void note(String msg) {
        main.post(() -> listener.onWarpProgress(msg));
    }

    private interface ThrowingRunnable { void run() throws Exception; }

    private void oneShot(String what, ThrowingRunnable r) {
        io.post(() -> {
            if (!ensureConnected()) return;
            try {
                r.run();
            } catch (Exception e) {
                Log.w(TAG, what + " failed: " + e);
                ioTrouble(e);
            }
        });
    }

    // ---- connection management (debug-io thread) --------------------------

    private void connectJob() {
        if (released || socket != null) return;
        setState(State.CONNECTING);
        try {
            Socket s = new Socket();
            s.connect(new InetSocketAddress("127.0.0.1", PORT), CONNECT_TIMEOUT_MS);
            s.setSoTimeout(COMMAND_TIMEOUT_MS);
            s.setTcpNoDelay(true);
            socket = s;
            in = new BufferedReader(new InputStreamReader(s.getInputStream(), StandardCharsets.UTF_8));
            out = s.getOutputStream();
            // Probe: the TCP handshake succeeds even when another client
            // holds the slot (kernel backlog) — only a served command
            // proves we own it.
            readByte("FFF600");
            setState(State.CONNECTED);
            Log.i(TAG, "connected to 127.0.0.1:" + PORT);
            schedulePoll(0);
        } catch (Exception e) {
            boolean refused = socket == null;   // connect() failed vs probe timed out
            closeSocket();
            setState(refused ? State.SERVER_OFF : State.BUSY);
            if (!released) io.postDelayed(this::connectJob, RETRY_MS);
        }
    }

    private boolean ensureConnected() {
        if (socket == null) connectJob();
        return socket != null;
    }

    private void closeSocket() {
        try { if (socket != null) socket.close(); } catch (IOException ignored) {}
        socket = null; in = null; out = null;
    }

    /** debug-io thread only: drop the connection and enter the retry cycle. */
    void ioTrouble(Exception e) {
        closeSocket();
        setState(released ? State.RELEASED : State.BUSY);
        if (!released) io.postDelayed(this::connectJob, RETRY_MS);
    }

    // ---- idle status poller (debug-io thread) ------------------------------

    private int lastLayoutZa = -1;
    private long lastLayoutAt;

    private final Runnable pollJob = new Runnable() {
        @Override public void run() {
            if (released || socket == null || warping) return;
            try {
                final GameSnapshot s = new GameSnapshot();
                s.gameMode = readByte("FFF600");
                byte[] blk = readBlock("FFFE10", 0x20);   // za..score in one read
                s.zoneAct = u16(blk, 0x00);
                s.lives   = blk[0x02] & 0xFF;
                s.rings   = u16(blk, 0x10);
                s.timeMin = blk[0x13] & 0xFF;
                s.timeSec = blk[0x14] & 0xFF;
                s.score   = (((long) u16(blk, 0x16) << 16) | u16(blk, 0x18)) * 10L;
                s.emeralds    = readByte("FFFEB1");
                s.superActive = readByte("FFF65F") != 0;
                byte[] pb = readBlock("FFB008", 0x24);    // X, Y, status_secondary
                s.playerX = u16(pb, 0);
                s.playerY = u16(pb, 4);
                int statusSec = pb[0x23] & 0xFF;          // FFB02B
                s.invincible = (statusSec & 0x02) != 0;
                s.speedShoes = (statusSec & 0x04) != 0;
                if (s.gameMode == 0x0C) lastZoneAct = s.zoneAct;
                byte[] cb = readBlock("FFEE00", 8);
                s.camX = u16(cb, 0);
                s.camY = u16(cb, 4);
                maybeFetchLayout(s);
                main.post(() -> listener.onSnapshot(s));
            } catch (Exception e) {
                ioTrouble(e);
                return;
            }
            schedulePoll(POLL_MS);
        }
    };

    /** One 4KB read per level (plus a slow refresh): FF8000 is the layout —
     *  16 row-pairs of 128 bytes, even half FG, odd half BG. */
    private void maybeFetchLayout(GameSnapshot s) throws Exception {
        if (s.gameMode != 0x0C) {
            lastLayoutZa = -1;
            return;
        }
        long now = SystemClock.uptimeMillis();
        if (s.zoneAct == lastLayoutZa && now - lastLayoutAt < LAYOUT_REFRESH_MS) return;
        byte[] raw = readBlock("FF8000", 4096);
        final byte[] fg = new byte[128 * 16];
        for (int r = 0; r < 16; r++)
            System.arraycopy(raw, r * 0x100, fg, r * 128, 128);
        lastLayoutZa = s.zoneAct;
        lastLayoutAt = now;
        final int za = s.zoneAct;
        main.post(() -> listener.onLayout(fg, za));
    }

    private void schedulePoll(long delay) {
        io.removeCallbacks(pollJob);
        if (!released) io.postDelayed(pollJob, delay);
    }

    // ---- protocol wrappers (debug-io thread; used by WarpEngine) -----------

    private JSONObject cmd(JSONObject msg) throws Exception {
        if (out == null) throw new IOException("not connected");
        msg.put("id", nextId++);
        out.write((msg.toString() + "\n").getBytes(StandardCharsets.UTF_8));
        out.flush();
        String line = in.readLine();
        if (line == null) throw new IOException("connection closed");
        JSONObject r = new JSONObject(line);
        if (r.has("error")) throw new IOException(msg.optString("cmd") + ": " + r.getString("error"));
        return r;
    }

    int readByte(String addr) throws Exception {
        JSONObject r = cmd(new JSONObject().put("cmd", "read_memory").put("addr", addr).put("size", 1));
        return Integer.parseInt(r.getString("hex"), 16);
    }

    int readWord(String addr) throws Exception {
        JSONObject r = cmd(new JSONObject().put("cmd", "read_memory").put("addr", addr).put("size", 2));
        return Integer.parseInt(r.getString("hex"), 16);
    }

    byte[] readBlock(String addr, int size) throws Exception {
        JSONObject r = cmd(new JSONObject().put("cmd", "read_memory").put("addr", addr).put("size", size));
        String hex = r.getString("hex");
        byte[] out = new byte[size];
        for (int i = 0; i < size; i++)
            out[i] = (byte) ((Character.digit(hex.charAt(i * 2), 16) << 4)
                            | Character.digit(hex.charAt(i * 2 + 1), 16));
        return out;
    }

    private static int u16(byte[] b, int off) {
        return ((b[off] & 0xFF) << 8) | (b[off + 1] & 0xFF);   // 68k big-endian
    }

    void writeHex(String addr, String hex) throws Exception {
        cmd(new JSONObject().put("cmd", "write_memory").put("addr", addr).put("hex", hex));
    }

    void setInput(String keys) throws Exception {
        cmd(new JSONObject().put("cmd", "set_input").put("keys", keys));
    }

    // ---- listener plumbing --------------------------------------------------

    private void setState(State st) {
        main.post(() -> listener.onConnectionState(st));
    }

    void warpProgress(String msg) {
        Log.i(TAG, "warp: " + msg);
        main.post(() -> listener.onWarpProgress(msg));
    }

    void warpDone(boolean ok, String msg) {
        Log.i(TAG, "warp done ok=" + ok + ": " + msg);
        main.post(() -> listener.onWarpDone(ok, msg));
        if (!ok && socket == null && !released)
            io.postDelayed(this::connectJob, RETRY_MS);
    }
}
