package tech.clyde.sonic2recomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
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
import java.util.Locale;
import java.util.zip.CRC32;

/**
 * Bring-your-own-ROM gate. Existing files are accepted only when they match
 * Sonic 2 (World, Rev A), or when the picker previously recorded explicit
 * approval for that exact filename and CRC.
 */
public class RomGateActivity extends Activity {
    private static final String TAG = "S2RomGate";
    private static final int REQ_PICK = 1;

    private static final int BG = 0xFF0A1030;
    private static final int FG_DIM = 0xFF93A4CE;
    private static final int ACCENT = 0xFFFFC81E;

    private TextView status;
    private Button pickButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        File dir = getExternalFilesDir(null);
        try {
            File rom = RomGateFiles.findAcceptedRom(
                    dir, RomGateFiles.SONIC2_REVA_CRC);
            if (rom != null) {
                RomGateFiles.writeRomConfig(dir, rom);
                launchGame();
                return;
            }
        } catch (Exception e) {
            Log.w(TAG, "existing ROM check failed", e);
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

        String intro = dir == null
                ? "App storage is unavailable. Check device storage and try again."
                : "No approved Sonic 2 ROM found.\n\n"
                    + "This app does not include the game. Select your own\n"
                    + "Sonic 2 (World, Rev A) raw ROM (.bin/.md/.gen) to continue.";
        TextView body = text(intro, 15, FG_DIM, false);
        body.setGravity(Gravity.CENTER);
        body.setPadding(0, dp(16), 0, dp(24));
        root.addView(body);

        pickButton = new Button(this);
        pickButton.setText(R.string.rom_select);
        pickButton.setAllCaps(false);
        pickButton.setTextSize(TypedValue.COMPLEX_UNIT_SP, 18);
        pickButton.setTypeface(Typeface.DEFAULT_BOLD);
        pickButton.setTextColor(BG);
        pickButton.setBackgroundColor(ACCENT);
        pickButton.setPadding(dp(32), dp(12), dp(32), dp(12));
        pickButton.setEnabled(dir != null);
        pickButton.setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            startActivityForResult(intent, REQ_PICK);
        });
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.gravity = Gravity.CENTER_HORIZONTAL;
        root.addView(pickButton, lp);

        status = text("", 13, FG_DIM, false);
        status.setGravity(Gravity.CENTER);
        status.setPadding(0, dp(16), 0, 0);
        root.addView(status);
        setContentView(root);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQ_PICK || resultCode != RESULT_OK || data == null) {
            return;
        }
        Uri uri = data.getData();
        if (uri == null) return;
        setBusy(true, "copying…");
        new Thread(() -> copyRom(uri), "rom-copy").start();
    }

    private void copyRom(Uri uri) {
        File dir = getExternalFilesDir(null);
        if (dir == null) {
            postFailure("app storage is unavailable");
            return;
        }

        File dest = new File(dir, RomGateFiles.INSTALLED_ROM);
        File staged = new File(dir, RomGateFiles.INSTALLED_ROM + ".part");
        long crc;
        try (InputStream in = getContentResolver().openInputStream(uri);
             FileOutputStream out = new FileOutputStream(staged, false)) {
            if (in == null) throw new java.io.IOException("picker returned no stream");
            CRC32 calculator = new CRC32();
            byte[] buffer = new byte[65536];
            int count;
            long total = 0;
            while ((count = in.read(buffer)) != -1) {
                if (count > 0) {
                    total += count;
                    if (total > RomGateFiles.MAX_ROM_BYTES) {
                        throw new java.io.IOException("selected file is too large");
                    }
                    out.write(buffer, 0, count);
                    calculator.update(buffer, 0, count);
                }
            }
            out.getFD().sync();
            crc = calculator.getValue();
        } catch (Exception e) {
            Log.w(TAG, "copy failed", e);
            staged.delete();
            postFailure("couldn't read that file — try another");
            return;
        }

        runOnUiThread(() -> {
            if (!RomGateFiles.hasGenesisHeader(staged)) {
                staged.delete();
                setBusy(false, "not a raw Genesis ROM (.bin/.md/.gen)");
            } else if (crc == RomGateFiles.SONIC2_REVA_CRC) {
                commitRom(staged, dest, crc, false);
            } else {
                new AlertDialog.Builder(this)
                        .setTitle("Not Sonic 2 (Rev A)")
                        .setMessage(String.format(Locale.ROOT,
                                "This file's CRC32 is %08X, not the expected %08X for "
                                + "Sonic 2 (World, Rev A). The recompiled code was "
                                + "built from Rev A, so other ROMs may not run.\n\n"
                                + "Use and remember this exact file anyway?",
                                crc, RomGateFiles.SONIC2_REVA_CRC))
                        .setPositiveButton("Use anyway",
                                (dialog, which) ->
                                        commitRom(staged, dest, crc, true))
                        .setNegativeButton("Cancel", (dialog, which) -> {
                            staged.delete();
                            setBusy(false, "cancelled — pick the Rev A ROM");
                        })
                        .setOnCancelListener(dialog -> {
                            staged.delete();
                            setBusy(false, "cancelled — pick the Rev A ROM");
                        })
                        .show();
            }
        });
    }

    private void commitRom(File staged, File dest, long crc, boolean approved) {
        try {
            RomGateFiles.replaceFile(staged, dest);
            if (approved) {
                RomGateFiles.recordApproval(dest.getParentFile(), dest, crc);
            } else {
                RomGateFiles.clearApproval(dest.getParentFile());
            }
            RomGateFiles.writeRomConfig(dest.getParentFile(), dest);
            launchGame();
        } catch (Exception e) {
            Log.w(TAG, "ROM install failed", e);
            staged.delete();
            setBusy(false, "couldn't save the ROM — storage problem?");
        }
    }

    private void postFailure(String message) {
        runOnUiThread(() -> setBusy(false, message));
    }

    private void setBusy(boolean busy, String message) {
        if (pickButton != null) pickButton.setEnabled(!busy);
        if (status != null) status.setText(message);
    }

    private void launchGame() {
        startActivity(new Intent(this, MainActivity.class));
        finish();
    }

    private TextView text(String value, int sp, int color, boolean bold) {
        TextView view = new TextView(this);
        view.setText(value);
        view.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        view.setTextColor(color);
        view.setTypeface(Typeface.MONOSPACE,
                bold ? Typeface.BOLD : Typeface.NORMAL);
        return view;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }
}
