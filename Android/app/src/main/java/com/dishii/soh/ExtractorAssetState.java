package com.dishii.soh;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

/** Extractor YAML changes independently of the renderer's support archive. */
final class ExtractorAssetState {
    static final String MARKER = ".android_extractor_version";
    // The completed 9.3.1 support-2 installation already contains this exact tree.
    // Only adopt that known revision; future YAML changes must still be copied.
    private static final String LEGACY_REVISION =
            "c7737f39734fc3a7b2888715c825b7137de7a8a1037e28122c0426ae6d78b074";

    static String readMarker(File file) {
        try (FileInputStream input = new FileInputStream(file)) {
            byte[] bytes = new byte[128];
            int count = input.read(bytes);
            return count > 0 ? new String(bytes, 0, count, StandardCharsets.UTF_8).trim() : "";
        } catch (IOException e) {
            return "";
        }
    }

    static boolean isCurrent(File root, String revision) {
        return revision != null && revision.equals(readMarker(new File(root, MARKER))) && hasAssets(root);
    }

    static boolean needsCopy(File root, String revision) {
        if (revision == null || !hasAssets(root)) return true;
        File marker = new File(root, MARKER);
        if (revision.equals(readMarker(marker))) return false;
        return marker.exists() || !LEGACY_REVISION.equals(revision) ||
                !"soh-9.3.1-torch-support-2".equals(
                        readMarker(new File(root, ".android_support_files_version")));
    }

    static void markComplete(File root, String revision) throws IOException {
        if (revision == null || !revision.matches("[0-9a-f]{64}")) {
            throw new IOException("Invalid bundled extractor revision");
        }
        try (FileOutputStream output = new FileOutputStream(new File(root, MARKER))) {
            output.write(revision.getBytes(StandardCharsets.UTF_8));
        }
    }

    private static boolean hasAssets(File root) {
        String[] entries = new File(root, "assets").list();
        return entries != null && entries.length > 0;
    }
}
