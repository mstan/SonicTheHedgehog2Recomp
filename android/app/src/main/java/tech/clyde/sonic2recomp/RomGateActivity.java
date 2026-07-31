package tech.clyde.sonic2recomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.zip.CRC32;

/**
 * Launcher gate: the runner needs a Genesis ROM in the app files dir (we
 * never ship one). If a ROM is already there this activity forwards to the
 * game instantly; otherwise it shows a picker (Storage Access Framework,
 * so no storage permissions) and copies the chosen file in. The engine
 * then finds it via its files-dir scan; any stale rom-*.cfg is removed so
 * a re-pick takes effect.
 *
 * The recompiled code is generated from Sonic 2 (W) Rev A — other files
 * won't run correctly, so the copy is CRC32-checked and anything else gets
 * an "are you sure" dialog.
 */
public class RomGateActivity extends Activity {

    private static final String TAG = "S2RomGate";
    private static final int REQ_PICK = 1;
    private static final long SONIC2_REVA_CRC = 0x7B905383L;
    private static final String[] ROM_EXTS = {".bin", ".md", ".gen", ".smd"};

    private static final int BG     = 0xFF0A1030;
    private static final int FG     = 0xFFF2F5FF;
    private static final int FG_DIM = 0xFF93A4CE;
    private static final int ACCENT = 0xFFFFC81E;

    private TextView status;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (findRom() != null) {
            launchGame();
            return;
        }
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BG);
        root.setGravity(Gravity.CENTER);
        int pad = dp(32);
        root.setPadding(pad, pad, pad, pad);

        TextView title = text("SONIC THE HEDGEHOG 2", 26, ACCENT, true);
        title.setGravity(Gravity.CENTER);
        root.addView(title);

        TextView body = text(
                "No Genesis ROM found.\n\n"
                + "This app does not include the game. Select your own\n"
                + "Sonic 2 (World, Rev A) ROM (.bin) to continue.",
                15, FG_DIM, false);
        body.setGravity(Gravity.CENTER);
        body.setPadding(0, dp(16), 0, dp(24));
        root.addView(body);

        Button pick = new Button(this);
        pick.setText("Select ROM…");
        pick.setAllCaps(false);
        pick.setTextSize(TypedValue.COMPLEX_UNIT_SP, 18);
        pick.setTypeface(Typeface.DEFAULT_BOLD);
        pick.setTextColor(BG);
        pick.setBackgroundColor(ACCENT);
        pick.setPadding(dp(32), dp(12), dp(32), dp(12));
        pick.setOnClickListener(v -> {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("*/*");
            startActivityForResult(i, REQ_PICK);
        });
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.gravity = Gravity.CENTER_HORIZONTAL;
        root.addView(pick, lp);

        status = text("", 13, FG_DIM, false);
        status.setGravity(Gravity.CENTER);
        status.setPadding(0, dp(16), 0, 0);
        root.addView(status);

        setContentView(root);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQ_PICK || resultCode != RESULT_OK || data == null) return;
        final Uri uri = data.getData();
        if (uri == null) return;
        status.setText("copying…");
        new Thread(() -> copyRom(uri)).start();
    }

    // ---- worker thread ------------------------------------------------------

    private void copyRom(Uri uri) {
        File dest = new File(getExternalFilesDir(null), "sonic2.bin");
        File tmp = new File(getExternalFilesDir(null), "sonic2.bin.part");
        long crc;
        try (InputStream in = getContentResolver().openInputStream(uri);
             FileOutputStream out = new FileOutputStream(tmp)) {
            CRC32 c = new CRC32();
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0) {
                out.write(buf, 0, n);
                c.update(buf, 0, n);
            }
            crc = c.getValue();
        } catch (Exception e) {
            Log.w(TAG, "copy failed", e);
            tmp.delete();
            runOnUiThread(() -> status.setText("couldn't read that file — try another"));
            return;
        }
        runOnUiThread(() -> {
            if (crc == SONIC2_REVA_CRC) {
                commitRom(tmp, dest);
            } else {
                new AlertDialog.Builder(this)
                    .setTitle("Not Sonic 2 (Rev A)")
                    .setMessage(String.format(
                        "This file's CRC32 is %08X, not the expected %08X for "
                        + "Sonic 2 (World, Rev A). The recompiled code was built "
                        + "from Rev A, so other ROMs will not run correctly.\n\n"
                        + "Use it anyway?", crc, SONIC2_REVA_CRC))
                    .setPositiveButton("Use anyway", (d, w) -> commitRom(tmp, dest))
                    .setNegativeButton("Cancel", (d, w) -> {
                        tmp.delete();
                        status.setText("cancelled — pick the Rev A .bin");
                    })
                    .show();
            }
        });
    }

    private void commitRom(File tmp, File dest) {
        if (!tmp.renameTo(dest)) {
            tmp.delete();
            status.setText("couldn't save the ROM — storage problem?");
            return;
        }
        // Stale cfg would keep pointing at an old file; let the engine rescan.
        File dir = getExternalFilesDir(null);
        File[] cfgs = dir == null ? null : dir.listFiles(
                (d, name) -> name.startsWith("rom-") && name.endsWith(".cfg"));
        if (cfgs != null) for (File f : cfgs) f.delete();
        launchGame();
    }

    // ---- helpers ------------------------------------------------------------

    private File findRom() {
        File dir = getExternalFilesDir(null);
        File[] files = dir == null ? null : dir.listFiles();
        if (files == null) return null;
        for (File f : files) {
            String n = f.getName().toLowerCase();
            for (String ext : ROM_EXTS)
                if (n.endsWith(ext) && looksLikeRom(f, ext)) return f;
        }
        return null;
    }

    /** Savestates share the .bin extension (native_save_N.bin) — require the
     *  Genesis "SEGA" header at 0x100. .smd is interleaved, so extension only. */
    private boolean looksLikeRom(File f, String ext) {
        if (ext.equals(".smd")) return true;
        try (java.io.RandomAccessFile r = new java.io.RandomAccessFile(f, "r")) {
            byte[] hdr = new byte[4];
            r.seek(0x100);
            r.readFully(hdr);
            return hdr[0] == 'S' && hdr[1] == 'E' && hdr[2] == 'G' && hdr[3] == 'A';
        } catch (Exception e) {
            return false;
        }
    }

    private void launchGame() {
        startActivity(new Intent(this, MainActivity.class));
        finish();
    }

    private TextView text(String s, int sp, int color, boolean bold) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setTextColor(color);
        t.setTypeface(Typeface.MONOSPACE, bold ? Typeface.BOLD : Typeface.NORMAL);
        return t;
    }

    private int dp(int v) {
        return Math.round(v * getResources().getDisplayMetrics().density);
    }
}
