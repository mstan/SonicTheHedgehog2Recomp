package tech.clyde.sonic2recomp;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class RomGateFilesTest {
    @Rule
    public TemporaryFolder temporary = new TemporaryFolder();

    @Test
    public void exactCrcWinsOverApprovedMismatch() throws Exception {
        File dir = temporary.getRoot();
        File mismatch = createRom(dir, "a-other.bin", (byte) 0x11);
        File exact = createRom(dir, "z-sonic2.BIN", (byte) 0x22);
        long exactCrc = RomGateFiles.crc32(exact);

        RomGateFiles.recordApproval(
                dir, mismatch, RomGateFiles.crc32(mismatch));

        assertEquals(exact, RomGateFiles.findAcceptedRom(dir, exactCrc));
    }

    @Test
    public void mismatchRequiresMatchingApproval() throws Exception {
        File dir = temporary.getRoot();
        File rom = createRom(dir, "sonic2.bin", (byte) 0x33);

        assertNull(RomGateFiles.findAcceptedRom(dir, 0x12345678L));

        RomGateFiles.recordApproval(dir, rom, RomGateFiles.crc32(rom));
        assertEquals(rom, RomGateFiles.findAcceptedRom(dir, 0x12345678L));

        overwriteByte(rom, 0x220, (byte) 0x7F);
        assertNull(RomGateFiles.findAcceptedRom(dir, 0x12345678L));
    }

    @Test
    public void ignoresSavestatesAndUnsupportedSmd() throws Exception {
        File dir = temporary.getRoot();
        File save = temporary.newFile("native_save_1.bin");
        File smd = createRom(dir, "sonic2.smd", (byte) 0x44);

        assertFalse(RomGateFiles.isRawGenesisRom(save));
        assertFalse(RomGateFiles.isRawGenesisRom(smd));
        assertNull(RomGateFiles.findAcceptedRom(
                dir, RomGateFiles.crc32(smd)));
    }

    @Test
    public void replacementPreservesOldFileUntilNewFileIsReady() throws Exception {
        File dir = temporary.getRoot();
        File dest = temporary.newFile("sonic2.bin");
        Files.write(dest.toPath(), "old".getBytes(StandardCharsets.UTF_8));
        File staged = temporary.newFile("sonic2.bin.part");
        byte[] replacement = "new-rom".getBytes(StandardCharsets.UTF_8);
        Files.write(staged.toPath(), replacement);

        RomGateFiles.replaceFile(staged, dest);

        assertArrayEquals(replacement, Files.readAllBytes(dest.toPath()));
        assertFalse(staged.exists());
        assertFalse(new File(dir, "sonic2.bin.previous").exists());
    }

    @Test
    public void configPinsChosenRomAndRemovesStaleConfigs() throws Exception {
        File dir = temporary.getRoot();
        File rom = createRom(dir, "sonic2.bin", (byte) 0x55);
        File stale = temporary.newFile("rom-Other.cfg");

        RomGateFiles.writeRomConfig(dir, rom);

        assertFalse(stale.exists());
        String config = new String(
                Files.readAllBytes(new File(dir, RomGateFiles.ROM_CFG_FILE).toPath()),
                StandardCharsets.UTF_8);
        assertEquals(rom.getAbsolutePath() + "\n", config);
    }

    private static File createRom(File dir, String name, byte fill)
            throws Exception {
        byte[] bytes = new byte[0x400];
        java.util.Arrays.fill(bytes, fill);
        bytes[0x100] = 'S';
        bytes[0x101] = 'E';
        bytes[0x102] = 'G';
        bytes[0x103] = 'A';
        File file = new File(dir, name);
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write(bytes);
        }
        return file;
    }

    private static void overwriteByte(File file, int offset, byte value)
            throws Exception {
        try (java.io.RandomAccessFile out =
                     new java.io.RandomAccessFile(file, "rw")) {
            out.seek(offset);
            out.write(value);
        }
    }
}
