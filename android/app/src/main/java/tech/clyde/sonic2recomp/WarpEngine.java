package tech.clyde.sonic2recomp;

import java.util.Locale;

/**
 * Java port of tools/warp.py's warp loop — that file is the reference
 * implementation; keep the sequences identical.
 *
 * Runs ON the debug-io thread (inside DebugClient.requestWarp's job), so
 * blocking sleeps are fine and all client wrappers are thread-correct.
 */
final class WarpEngine {

    private static final String GM_ADDR         = "FFF600"; // Game_Mode (byte)
    private static final String ZONEACT_ADDR    = "FFFE10"; // Current_ZoneAndAct (word)
    private static final String RESTART_ADDR    = "FFFE02"; // Level_Inactive_flag (word)
    private static final String CHECKPOINT_ADDR = "FFFE30"; // Last_star_pole_hit P1+P2
    private static final String PLAYER_X_ADDR   = "FFB008";
    private static final String PLAYER_Y_ADDR   = "FFB00C";
    private static final String CAMERA_MIN_X_ADDR = "FFEEC8";

    private static final int GM_SEGA = 0x00, GM_TITLE = 0x04, GM_DEMO = 0x08, GM_LEVEL = 0x0C;

    private static final long DEADLINE_MS = 60_000;
    private static final long POLL_MS     = 200;
    private static final long TAP_GAP_MS  = 1_000;

    /**
     * Warp to zoneActWord; if bossSpot != null ({x, y}), teleport the player
     * there after the level loads (camera/LevEvents converge over a few
     * frames — the same mechanism as the game's own debug-mode placement).
     * Reports progress/completion through the client; never throws.
     */
    static void run(DebugClient c, int target, int[] bossSpot) {
        c.warpProgress("warping to " + Zones.pretty(target) + "…");
        long deadline = now() + DEADLINE_MS;
        long lastTap = 0;
        boolean warped = false;
        try {
            while (now() < deadline) {
                if (c.cancelRequested) { c.warpDone(false, "warp cancelled"); return; }

                int gm = c.readByte(GM_ADDR);
                int base = gm & 0x7F;   // warp.py masks the pending-mode high bit

                if (!warped) {
                    if (gm == GM_LEVEL) {
                        c.writeHex(ZONEACT_ADDR, String.format(
                                Locale.ROOT, "%04X", target));
                        c.writeHex(CHECKPOINT_ADDR, "0000");
                        c.writeHex(RESTART_ADDR, "0001");
                        warped = true;
                        c.warpProgress("restart flag set, waiting for reload…");
                    } else if ((gm == GM_TITLE || gm == GM_DEMO) && now() - lastTap > TAP_GAP_MS) {
                        c.warpProgress(String.format(Locale.ROOT,
                                "Game_Mode 0x%02X — tapping Start", gm));
                        c.setInput("80");
                        sleep(150);
                        c.setInput("off");
                        lastTap = now();
                    } else if (base != GM_SEGA && base != GM_TITLE
                            && base != GM_DEMO && base != GM_LEVEL) {
                        c.warpDone(false, String.format(Locale.ROOT,
                            "can't warp from GM 0x%02X (special stage/continue/2P?)", gm));
                        return;
                    }
                } else {
                    if (gm == GM_LEVEL && c.readWord(ZONEACT_ADDR) == target) {
                        if (bossSpot != null) {
                            bossApproach(c, target, bossSpot);
                        } else {
                            c.warpDone(true, Zones.pretty(target) + " loaded");
                        }
                        return;
                    }
                }
                sleep(POLL_MS);
            }
            c.warpDone(false, "timed out waiting for the warp");
        } catch (Exception e) {
            c.ioTrouble(e);
            c.warpDone(false, "connection lost mid-warp (" + e.getMessage() + ")");
        }
    }

    /**
     * Post-load boss approach: teleport to the spot, optionally walk right so
     * the camera's target passes the LevEvents thresholds, then wait for the
     * camera to pan there (~16 px/frame from the level start — tens of
     * seconds) and the boss lock (Camera_Min_X) to engage.
     * bossSpot = {x, y, walkMs, lockMinX} — verified per act.
     */
    private static void bossApproach(DebugClient c, int target, int[] spot) throws Exception {
        c.warpProgress("teleporting to boss arena…");
        sleep(1_000);   // let objects/camera settle post-load
        c.writeHex(PLAYER_X_ADDR, String.format(
                Locale.ROOT, "%04X", spot[0] & 0xFFFF));
        c.writeHex(PLAYER_Y_ADDR, String.format(
                Locale.ROOT, "%04X", spot[1] & 0xFFFF));
        sleep(1_000);
        if (spot[2] > 0) {
            c.setInput("08");   // hold Right: walk deeper into the arena
            sleep(spot[2]);
            c.setInput("off");
        }
        c.warpProgress("waiting for camera / boss lock…");
        int lock = spot[3];
        long lockDeadline = now() + 35_000;
        while (now() < lockDeadline) {
            if (c.cancelRequested) { c.warpDone(false, "warp cancelled"); return; }
            int minX = c.readWord(CAMERA_MIN_X_ADDR);
            if (Math.abs(minX - lock) <= 0x80) {
                c.warpDone(true, Zones.pretty(target) + " boss engaged");
                return;
            }
            sleep(500);
        }
        c.warpDone(true, Zones.pretty(target) + " boss (teleported; lock still pending)");
    }

    private static long now() { return System.nanoTime() / 1_000_000; }

    private static void sleep(long ms) {
        try { Thread.sleep(ms); } catch (InterruptedException ignored) {}
    }

    private WarpEngine() {}
}
