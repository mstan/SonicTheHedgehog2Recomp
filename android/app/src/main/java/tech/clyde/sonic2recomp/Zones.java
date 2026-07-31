package tech.clyde.sonic2recomp;

import java.util.LinkedHashMap;
import java.util.Locale;
import java.util.Map;

/**
 * Static Sonic 2 zone/act data for the dev warp panel.
 * Mirrors tools/warp.py — that file is the reference; keep them in sync.
 */
final class Zones {

    static final class Zone {
        final String name;      // short label, e.g. "EHZ"
        final String longName;  // e.g. "Emerald Hill"
        final int id;           // internal zone id (high byte of Current_ZoneAndAct)
        final int acts;
        final boolean risky;    // HPZ: leftover data, may crash

        Zone(String name, String longName, int id, int acts, boolean risky) {
            this.name = name; this.longName = longName;
            this.id = id; this.acts = acts; this.risky = risky;
        }
    }

    static final Zone[] ALL = {
        new Zone("EHZ", "Emerald Hill",  0x00, 2, false),
        new Zone("CPZ", "Chemical Plant",0x0D, 2, false),
        new Zone("ARZ", "Aquatic Ruin",  0x0F, 2, false),
        new Zone("CNZ", "Casino Night",  0x0C, 2, false),
        new Zone("HTZ", "Hill Top",      0x07, 2, false),
        new Zone("MCZ", "Mystic Cave",   0x0B, 2, false),
        new Zone("OOZ", "Oil Ocean",     0x0A, 2, false),
        new Zone("MTZ", "Metropolis",    0x04, 3, false),
        new Zone("SCZ", "Sky Chase",     0x10, 1, false),
        new Zone("WFZ", "Wing Fortress", 0x06, 1, false),
        new Zone("DEZ", "Death Egg",     0x0E, 1, false),
        new Zone("HPZ", "Hidden Palace", 0x08, 1, true),
    };

    /** Current_ZoneAndAct word for a 1-based act. MTZ act 3 is zone 0x05 act 1. */
    static int zoneActWord(Zone z, int act1based) {
        if (z.id == 0x04 && act1based == 3) return 0x0500;
        return (z.id << 8) | (act1based - 1);
    }

    /** Reverse lookup for the status line; falls back to raw hex. */
    static String pretty(int zoneActWord) {
        int zone = (zoneActWord >> 8) & 0xFF;
        int act = (zoneActWord & 0xFF) + 1;
        if (zone == 0x05) return "MTZ act 3";
        for (Zone z : ALL)
            if (z.id == zone)
                return z.name + " act " + act;
        return String.format(Locale.ROOT, "zone %02X act %d", zone, act);
    }

    /**
     * Boss teleport spots: zoneActWord -> {playerX, playerY, walkMs, lockMinX}.
     * X/Y land the player just outside the arena; walkMs of held-Right pushes
     * the camera target past the LevEvents lock threshold; lockMinX is the
     * boss camera lock (Camera_Min_X) extracted from the ROM's LevEvents
     * `move.w #imm,(bounds).w` writes and verified live on the Thor
     * (2026-07-29). walkMs == 0 marks the interactive approaches: HTZ2's
     * earthquake ride and OOZ2's oil-surface hop are played, not scripted —
     * idle players die there, active ones don't. WFZ/DEZ/SCZ have no entry:
     * WFZ's boss is gated on scripted events (not camera position); DEZ IS
     * its boss corridor; SCZ is an auto-scroller. Boss buttons render only
     * for entries present here.
     */
    static final Map<Integer, int[]> BOSS_SPOTS = new LinkedHashMap<>();
    static {
        BOSS_SPOTS.put(0x0001, new int[]{0x2980, 0x0408, 1500, 0x28F0});  // EHZ 2
        BOSS_SPOTS.put(0x0D01, new int[]{0x2AC0, 0x04C8,  500, 0x2A20});  // CPZ 2
        BOSS_SPOTS.put(0x0F01, new int[]{0x2AA0, 0x0478, 1000, 0x2A40});  // ARZ 2
        BOSS_SPOTS.put(0x0C01, new int[]{0x2940, 0x0560, 1000, 0x2860});  // CNZ 2
        BOSS_SPOTS.put(0x0701, new int[]{0x2F20, 0x04F8,    0, 0x2EE0});  // HTZ 2 (ride the quake)
        BOSS_SPOTS.put(0x0B01, new int[]{0x2150, 0x0648, 1500, 0x20F0});  // MCZ 2
        BOSS_SPOTS.put(0x0A01, new int[]{0x28A0, 0x01C0,    0, 0x2880});  // OOZ 2 (hop the oil)
        BOSS_SPOTS.put(0x0500, new int[]{0x2B10, 0x0480, 1000, 0x2AB0});  // MTZ 3
    }

    /**
     * User-supplied full-level map image for a zone/act word, or null (HPZ
     * has no retail map). Files live in <files dir>/maps/, named exactly as
     * on sonicgalaxy.net so a plain download loop works (Metropolis is
     * "mz"; the single-act zones are scz-1/wfz-1/dez-1). See
     * README-android.md — the images are SEGA level art and are never
     * committed or shipped; without them the minimap falls back to the
     * layout-RAM silhouette.
     */
    static String mapFileName(int zoneActWord) {
        int zone = (zoneActWord >> 8) & 0xFF;
        int act = (zoneActWord & 0xFF) + 1;
        if (zone == 0x05) { zone = 0x04; act = 3; }   // MTZ act 3
        String slug;
        switch (zone) {
            case 0x00: slug = "ehz"; break;
            case 0x0D: slug = "cpz"; break;
            case 0x0F: slug = "arz"; break;
            case 0x0C: slug = "cnz"; break;
            case 0x07: slug = "htz"; break;
            case 0x0B: slug = "mcz"; break;
            case 0x0A: slug = "ooz"; break;
            case 0x04: slug = "mz";  break;
            case 0x10: slug = "scz"; break;
            case 0x06: slug = "wfz"; break;
            case 0x0E: slug = "dez"; break;
            default: return null;
        }
        return "maps/" + slug + "-" + act + ".png";
    }

    private Zones() {}
}
