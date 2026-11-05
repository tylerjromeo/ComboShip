package com.dishii.soh;

import android.content.Context;
import android.content.res.AssetManager;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class AssetCopyUtil {

    public static void copyAssetsToExternal(Context context, String assetsFolderPath, String externalFolderPath) throws IOException {
        AssetManager assetManager = context.getAssets();
        copyAssets(assetManager, assetsFolderPath, externalFolderPath,
                assetManager.list(assetsFolderPath), new byte[64 * 1024]);
    }

    private static void copyAssets(AssetManager assetManager, String assetsFolderPath,
                                   String externalFolderPath, String[] assetFiles, byte[] buffer) throws IOException {
        if (assetFiles == null) throw new IOException("Unable to list bundled assets: " + assetsFolderPath);

        for (String assetFile : assetFiles) {
            String assetPath = assetsFolderPath + File.separator + assetFile;
            String externalPath = externalFolderPath + File.separator + assetFile;

            String[] children = assetManager.list(assetPath);
            if (children != null && children.length > 0) {
                // It's a directory
                // Check if the directory exists in the external storage
                File externalDir = new File(externalPath);
                if (!externalDir.exists() && !externalDir.mkdirs()) {
                    throw new IOException("Failed to create asset directory: " + externalDir.getAbsolutePath());
                }

                // Recursively copy contents of the directory
                copyAssets(assetManager, assetPath, externalPath, children, buffer);
            } else {
                // It's a file
                File externalFile = new File(externalPath);
                if (!externalFile.exists()) {
                    // Check if the file exists in the external storage
                    InputStream in = null;
                    OutputStream out = null;

                    try {
                        in = assetManager.open(assetPath);
                        File parentDir = externalFile.getParentFile();
                        if (parentDir != null && !parentDir.exists() && !parentDir.mkdirs()) {
                            throw new IOException("Failed to create asset parent directory: " + parentDir.getAbsolutePath());
                        }
                        out = new FileOutputStream(externalFile);

                        int read;
                        while ((read = in.read(buffer)) != -1) {
                            out.write(buffer, 0, read);
                        }

                    } finally {
                        if (in != null) {
                            in.close();
                        }
                        if (out != null) {
                            out.close();
                        }
                    }
                }
            }
        }
    }

    public static void copyDirectory(File sourceDir, File targetDir) throws IOException {
        if (!targetDir.exists() && !targetDir.mkdirs()) {
            throw new IOException("Failed to create target directory: " + targetDir.getAbsolutePath());
        }
        File[] files = sourceDir.listFiles();
        if (files == null) {
            throw new IOException("Failed to list source directory: " + sourceDir.getAbsolutePath());
        }
        for (File file : files) {
            File dest = new File(targetDir, file.getName());
            if (file.isDirectory()) {
                copyDirectory(file, dest);
            } else {
                copyFile(file, dest);
            }
        }
    }

    public static void copyDirectoryNoOverwrite(File sourceDir, File targetDir) throws IOException {
        if (!targetDir.exists() && !targetDir.mkdirs()) {
            throw new IOException("Failed to create target directory: " + targetDir.getAbsolutePath());
        }
        File[] files = sourceDir.listFiles();
        if (files == null) {
            throw new IOException("Failed to list source directory: " + sourceDir.getAbsolutePath());
        }
        for (File file : files) {
            File dest = new File(targetDir, file.getName());
            if (file.isDirectory()) {
                copyDirectoryNoOverwrite(file, dest);
            } else {
                copyFileNoOverwrite(file, dest);
            }
        }
    }

    public static void copyDirectoryContentsNoOverwrite(File sourceDir, File targetDir) throws IOException {
        if (!targetDir.exists() && !targetDir.mkdirs()) {
            throw new IOException("Failed to create target directory: " + targetDir.getAbsolutePath());
        }

        File[] files = sourceDir.listFiles();
        if (files == null) {
            throw new IOException("Failed to list source directory: " + sourceDir.getAbsolutePath());
        }

        for (File file : files) {
            File dest = new File(targetDir, file.getName());
            if (file.isDirectory()) {
                copyDirectoryNoOverwrite(file, dest);
            } else {
                copyFileNoOverwrite(file, dest);
            }
        }
    }

    public static void copyFile(File source, File dest) throws IOException {
        File parentDir = dest.getParentFile();
        if (parentDir != null && !parentDir.exists() && !parentDir.mkdirs()) {
            throw new IOException("Failed to create target parent directory: " + parentDir.getAbsolutePath());
        }

        try (InputStream in = new FileInputStream(source);
             OutputStream out = new FileOutputStream(dest)) {
            byte[] buf = new byte[64*1024];
            int len;
            while ((len = in.read(buf)) > 0) {
                out.write(buf, 0, len);
            }
        }
    }

    public static void copyFileNoOverwrite(File source, File dest) throws IOException {
        if (dest.exists()) {
            return;
        }

        copyFile(source, dest);
    }
}
