package tech.clyde.sonic2recomp;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.view.View;

/**
 * Live level minimap for the status page: a chunk-occupancy silhouette of
 * the current act (from the layout RAM at FF8000, one cell per 128x128 px
 * chunk), overlaid with the camera viewport, the player, and — on boss
 * acts — the arena's camera-lock line. Extent comes from occupancy, not
 * camera bounds: bounds read unclamped (0x3FFF) in some acts and would
 * squash the map.
 *
 * Two view modes (tap to toggle, owner wires the click): fit shows the
 * whole act letterboxed; zoom fills the view height and pans horizontally
 * to keep the player centered, clamped at the level edges.
 */
final class MinimapView extends View {

    private static final int COL_CAMERA  = 0xFFF2F5FF;
    private static final int COL_PLAYER  = 0xFF3D7BFF;   // Sonic blue
    private static final int COL_PLAYER_RIM = 0xFFF2F5FF;
    private static final int COL_LOCK    = 0xFFE04040;   // Robotnik red
    private static final int COL_HINT    = 0xFF93A4CE;

    /** Terrain tint per zone, echoing each act's palette. */
    private static int terrainColor(int zoneAct) {
        switch ((zoneAct >> 8) & 0xFF) {
            case 0x00: return 0xFF2F8A46;              // EHZ green
            case 0x0D: return 0xFF3A6FA8;              // CPZ steel blue
            case 0x0F: return 0xFF2E8A7A;              // ARZ teal
            case 0x0C: return 0xFF8A3A8A;              // CNZ purple
            case 0x07: return 0xFF9A5230;              // HTZ lava brown
            case 0x0B: return 0xFF6A55A6;              // MCZ violet
            case 0x0A: return 0xFFA8742A;              // OOZ oil orange
            case 0x04: case 0x05: return 0xFF9A6A32;   // MTZ copper
            case 0x10: return 0xFF5A82A8;              // SCZ sky
            case 0x06: return 0xFF6A7688;              // WFZ slate
            case 0x0E: return 0xFF7A8090;              // DEZ gray
            case 0x08: return 0xFF3A8A6E;              // HPZ jade
            default:   return 0xFF3D4A61;
        }
    }

    private static final int SCREEN_W = 320, SCREEN_H = 224;   // nominal 4:3 viewport
    private static final int CHUNK = 128;

    private Bitmap chunkBmp;            // 128x16, one pixel per chunk
    private int cols, rows;             // occupied extent in chunks
    private int zoneAct = -1;
    private DebugClient.GameSnapshot snap;
    private boolean zoom;               // fill height + follow the player

    /* User-supplied full-level map image (sonicgalaxy.net renders are 1:1
     * with the level pixel grid, chunk-aligned — see Zones.mapFileName).
     * Decoded downsampled on a worker thread; the original pixel dims are
     * kept for coordinate mapping. Null → silhouette fallback. */
    private Bitmap mapBmp;
    private int mapLevelW, mapLevelH;   // ORIGINAL image dims = level pixels
    private int mapZoneAct = -1;        // zone/act the load was kicked for

    private final Paint bmpPaint = new Paint();          // unfiltered: crisp cells
    private final Paint mapPaint = new Paint(Paint.FILTER_BITMAP_FLAG);
    private final Paint boxPaint = new Paint();
    private final Paint dotPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint lockPaint = new Paint();
    private final Paint hintPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint labelPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Rect src = new Rect();
    private final RectF dst = new RectF();

    MinimapView(Context ctx) {
        super(ctx);
        bmpPaint.setFilterBitmap(false);
        boxPaint.setStyle(Paint.Style.STROKE);
        boxPaint.setStrokeWidth(dp(2));
        boxPaint.setColor(COL_CAMERA);
        dotPaint.setColor(COL_PLAYER);
        lockPaint.setStrokeWidth(dp(2));
        lockPaint.setColor(COL_LOCK);
        hintPaint.setColor(COL_HINT);
        hintPaint.setTextAlign(Paint.Align.CENTER);
        hintPaint.setTypeface(Typeface.MONOSPACE);
        hintPaint.setTextSize(dp(14));
        labelPaint.setColor(COL_HINT);
        labelPaint.setTextAlign(Paint.Align.RIGHT);
        labelPaint.setTypeface(Typeface.MONOSPACE);
        labelPaint.setTextSize(dp(11));
    }

    void setLayoutData(byte[] fg, int za) {
        zoneAct = za;
        int terrain = terrainColor(za);
        Bitmap bmp = Bitmap.createBitmap(128, 16, Bitmap.Config.ARGB_8888);
        int maxC = 0, maxR = 0;
        for (int r = 0; r < 16; r++)
            for (int c = 0; c < 128; c++)
                if (fg[r * 128 + c] != 0) {
                    bmp.setPixel(c, r, terrain);
                    if (c > maxC) maxC = c;
                    if (r > maxR) maxR = r;
                }
        chunkBmp = bmp;
        cols = Math.max(maxC + 1, 8);
        rows = Math.max(maxR + 1, 4);
        maybeLoadMapImage(za);
        invalidate();
    }

    void setZoom(boolean on) {
        zoom = on;
        invalidate();
    }

    void setSnapshot(DebugClient.GameSnapshot s) {
        snap = s;
        if (s == null || s.gameMode != 0x0C) {
            chunkBmp = null;
            zoneAct = -1;
            mapBmp = null;
            mapZoneAct = -1;
        }
        invalidate();
    }

    /** Decode <files>/maps/<slug>.png off the main thread; keep the original
     *  dims (== level pixels) and a panel-width-ish downsample for drawing. */
    private void maybeLoadMapImage(final int za) {
        if (za == mapZoneAct) return;   // loaded (or known-absent) already
        mapZoneAct = za;
        mapBmp = null;
        final String name = Zones.mapFileName(za);
        if (name == null) return;
        final java.io.File file =
                new java.io.File(getContext().getExternalFilesDir(null), name);
        if (!file.isFile()) return;
        new Thread(() -> {
            android.graphics.BitmapFactory.Options o =
                    new android.graphics.BitmapFactory.Options();
            o.inJustDecodeBounds = true;
            android.graphics.BitmapFactory.decodeFile(file.getPath(), o);
            if (o.outWidth <= 0) return;
            final int w = o.outWidth, h = o.outHeight;
            android.graphics.BitmapFactory.Options o2 =
                    new android.graphics.BitmapFactory.Options();
            o2.inSampleSize = 1;
            while (w / o2.inSampleSize > 2600) o2.inSampleSize *= 2;
            final Bitmap bmp =
                    android.graphics.BitmapFactory.decodeFile(file.getPath(), o2);
            if (bmp == null) return;
            post(() -> {
                if (mapZoneAct != za) return;   // level changed mid-decode
                mapBmp = bmp;
                mapLevelW = w;
                mapLevelH = h;
                invalidate();
            });
        }, "map-decode").start();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        int w = getWidth(), h = getHeight();
        int lw, lh;                     // level extent in level pixels
        if (mapBmp != null) {
            lw = mapLevelW;
            lh = mapLevelH;
        } else if (chunkBmp != null && cols > 0) {
            lw = cols * CHUNK;
            lh = rows * CHUNK;
        } else {
            canvas.drawText("no level", w / 2f, h / 2f, hintPaint);
            return;
        }
        float pxPerLvl = zoom ? (float) h / lh
                              : Math.min((float) w / lw, (float) h / lh);
        float mw = lw * pxPerLvl, mh = lh * pxPerLvl;
        float left;
        if (mw <= w) {
            left = (w - mw) / 2f;
        } else {
            // Zoomed: keep the player centered, clamped to the level edges.
            float focus = (snap != null && snap.gameMode == 0x0C)
                    ? snap.playerX : lw / 2f;
            left = w / 2f - focus * pxPerLvl;
            left = Math.max(w - mw, Math.min(0f, left));
        }
        dst.set(left, (h - mh) / 2f, left + mw, (h + mh) / 2f);
        if (mapBmp != null) {
            canvas.drawBitmap(mapBmp, null, dst, mapPaint);
        } else {
            src.set(0, 0, cols, rows);
            canvas.drawBitmap(chunkBmp, src, dst, bmpPaint);
        }

        int[] boss = Zones.BOSS_SPOTS.get(zoneAct);
        if (boss != null) {
            float lx = dst.left + boss[3] * pxPerLvl;
            canvas.drawLine(lx, dst.top, lx, dst.bottom, lockPaint);
        }
        if (snap != null && snap.gameMode == 0x0C) {
            float cx = dst.left + snap.camX * pxPerLvl;
            float cy = dst.top + snap.camY * pxPerLvl;
            canvas.drawRect(cx, cy, cx + SCREEN_W * pxPerLvl, cy + SCREEN_H * pxPerLvl, boxPaint);
            float px = dst.left + snap.playerX * pxPerLvl;
            float py = dst.top + snap.playerY * pxPerLvl;
            dotPaint.setColor(COL_PLAYER_RIM);   // white rim so the dot reads on any tint
            canvas.drawCircle(px, py, dp(5), dotPaint);
            dotPaint.setColor(COL_PLAYER);
            canvas.drawCircle(px, py, dp(4), dotPaint);
        }
        canvas.drawText(zoom ? "tap: fit" : "tap: zoom",
                w - dp(6), dp(14), labelPaint);
    }

    private float dp(int v) {
        return v * getContext().getResources().getDisplayMetrics().density;
    }
}
