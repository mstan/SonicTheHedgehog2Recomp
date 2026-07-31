package tech.clyde.sonic2recomp;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
import java.util.zip.CRC32;

/** Pure file/selection logic for {@link RomGateActivity}; kept Android-free so
 * the destructive replacement and multi-ROM edge cases can be unit tested. */
final class RomGateFiles {
    static final long SONIC2_REVA_CRC = 0x7B905383L;
    static final String INSTALLED_ROM = "sonic2.bin";
    static final String APPROVAL_FILE = "approved-rom.txt";
    static final String ROM_CFG_FILE = "rom-Sonic2.cfg";
    static final long MAX_ROM_BYTES = 16L * 1024L * 1024L;

    private static final String[] RAW_ROM_EXTS = {".bin", ".md", ".gen"};

    private static final Comparator<File> FILE_ORDER = new Comparator<File>() {
        @Override
        public int compare(File left, File right) {
            int folded = String.CASE_INSENSITIVE_ORDER.compare(
                    left.getName(), right.getName());
            return folded != 0
                    ? folded
                    : left.getName().compareTo(right.getName());
        }
    };

    private RomGateFiles() {}

    /**
     * Find a usable ROM deterministically. The exact Rev A dump always wins;
     * a non-matching image is accepted only when the picker recorded explicit
     * approval for this filename and CRC.
     */
    static File findAcceptedRom(File dir, long expectedCrc) throws IOException {
        if (dir == null || !dir.isDirectory()) return null;

        Approval approval = readApproval(dir);
        List<File> candidates = new ArrayList<>();
        File[] files = dir.listFiles();
        if (files == null) return null;
        for (File file : files) {
            if (isRawGenesisRom(file)) candidates.add(file);
        }
        Collections.sort(candidates, FILE_ORDER);

        File approved = null;
        for (File file : candidates) {
            long crc = crc32(file);
            if (crc == expectedCrc) return file;
            if (approval != null && approval.matches(file, crc)) approved = file;
        }
        return approved;
    }

    static boolean isRawGenesisRom(File file) {
        if (file == null || !file.isFile()) return false;
        String lower = file.getName().toLowerCase(Locale.ROOT);
        boolean extension = false;
        for (String ext : RAW_ROM_EXTS) {
            if (lower.endsWith(ext)) {
                extension = true;
                break;
            }
        }
        if (!extension) return false;

        return hasGenesisHeader(file);
    }

    static boolean hasGenesisHeader(File file) {
        if (file == null || !file.isFile()) return false;
        try (java.io.RandomAccessFile in =
                     new java.io.RandomAccessFile(file, "r")) {
            byte[] header = new byte[4];
            in.seek(0x100);
            in.readFully(header);
            return header[0] == 'S' && header[1] == 'E'
                    && header[2] == 'G' && header[3] == 'A';
        } catch (IOException e) {
            return false;
        }
    }

    static long crc32(File file) throws IOException {
        CRC32 crc = new CRC32();
        try (InputStream in = new FileInputStream(file)) {
            byte[] buffer = new byte[65536];
            int count;
            while ((count = in.read(buffer)) != -1) {
                if (count > 0) crc.update(buffer, 0, count);
            }
        }
        return crc.getValue();
    }

    /**
     * Replace dest without first destroying it. If the new rename fails, the
     * previous file is restored from a same-directory backup.
     */
    static void replaceFile(File staged, File dest) throws IOException {
        if (staged == null || !staged.isFile()) {
            throw new IOException("staged file is missing");
        }
        File backup = new File(dest.getParentFile(), dest.getName() + ".previous");
        if (backup.exists() && !backup.delete()) {
            throw new IOException("cannot remove stale backup " + backup.getName());
        }

        boolean hadDest = dest.exists();
        if (hadDest && !dest.renameTo(backup)) {
            throw new IOException("cannot preserve existing " + dest.getName());
        }
        if (!staged.renameTo(dest)) {
            if (hadDest && !backup.renameTo(dest)) {
                throw new IOException("install failed and previous ROM could not be restored");
            }
            throw new IOException("cannot install " + dest.getName());
        }
        // The install succeeded. A leftover backup is safe and intentionally
        // ignored by ROM discovery, so do not turn cleanup into a failure.
        if (backup.exists()) backup.delete();
    }

    static void recordApproval(File dir, File rom, long crc) throws IOException {
        File staged = new File(dir, APPROVAL_FILE + ".part");
        byte[] value = String.format(
                Locale.ROOT, "%s%n%08X%n", rom.getName(), crc)
                .getBytes(StandardCharsets.UTF_8);
        try (FileOutputStream out = new FileOutputStream(staged, false)) {
            out.write(value);
            out.getFD().sync();
        }
        replaceFile(staged, new File(dir, APPROVAL_FILE));
    }

    static void clearApproval(File dir) throws IOException {
        File approval = new File(dir, APPROVAL_FILE);
        if (approval.exists() && !approval.delete()) {
            throw new IOException("cannot clear old ROM approval");
        }
    }

    static void writeRomConfig(File dir, File rom) throws IOException {
        File target = new File(dir, ROM_CFG_FILE);
        File staged = new File(dir, ROM_CFG_FILE + ".part");
        byte[] value = (rom.getAbsolutePath() + "\n")
                .getBytes(StandardCharsets.UTF_8);
        try (FileOutputStream out = new FileOutputStream(staged, false)) {
            out.write(value);
            out.getFD().sync();
        }
        replaceFile(staged, target);

        File[] stale = dir.listFiles((d, name) ->
                name.startsWith("rom-") && name.endsWith(".cfg"));
        if (stale != null) {
            for (File file : stale) {
                if (!file.equals(target) && !file.delete()) {
                    throw new IOException("cannot remove stale " + file.getName());
                }
            }
        }
    }

    private static Approval readApproval(File dir) {
        File file = new File(dir, APPROVAL_FILE);
        if (!file.isFile()) return null;
        try (BufferedReader reader = new BufferedReader(new FileReader(file))) {
            String name = reader.readLine();
            String crcText = reader.readLine();
            if (name == null || crcText == null) return null;
            return new Approval(name, Long.parseLong(crcText, 16));
        } catch (Exception e) {
            return null;
        }
    }

    private static final class Approval {
        final String filename;
        final long crc;

        Approval(String filename, long crc) {
            this.filename = filename;
            this.crc = crc;
        }

        boolean matches(File file, long candidateCrc) {
            return filename.equals(file.getName()) && crc == candidateCrc;
        }
    }
}
