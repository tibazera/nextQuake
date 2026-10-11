package org.ezquake.android;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

// Downloads the game data files listed in GameDataManifest from
// raw.githubusercontent.com/nQuake/distfiles into BASEDIR, so the user does
// not have to copy id1/pak0.pak etc. manually before the first launch.
public final class GameDataInstaller {

    private static final String RAW_BASE_URL = "https://raw.githubusercontent.com/nQuake/distfiles/master/";
    private static final int CONNECT_TIMEOUT_MS = 15000;
    private static final int READ_TIMEOUT_MS = 30000;
    private static final int BUFFER_SIZE = 16 * 1024;

    // Mirrors Android_HasUsableBaseData() in src/sys_posix.c -- keep the two
    // candidate lists in sync if either one changes.
    private static final String[] BASE_DATA_CANDIDATES = {
            "id1/pak0.pak",
            "id1/PAK0.PAK",
            "id1/gfx.wad",
            "id1/gfx/palette.lmp",
    };

    public interface ProgressListener {
        void onFileStarted(int fileIndex, int totalFiles, String relativePath);
        void onFileProgress(long bytesDownloadedTotal, long bytesEstimatedTotal);
        void onCompleted();
        void onFailed(String relativePath, Exception cause);
    }

    public static boolean isEssentialDataPresent(File baseDir) {
        for (String candidate : BASE_DATA_CANDIDATES) {
            if (new File(baseDir, candidate).isFile()) {
                return true;
            }
        }
        return false;
    }

    // True when every file of the manifest is already on disk (non-empty), so the
    // launcher can detect an existing install instead of offering to download it.
    public static boolean allFilesPresent(File baseDir, List<GameDataManifest.Entry> manifest) {
        for (GameDataManifest.Entry entry : manifest) {
            File file = new File(baseDir, entry.localPath);
            if (!file.isFile() || file.length() == 0) {
                return false;
            }
        }
        return true;
    }

    public static long totalSize(List<GameDataManifest.Entry> manifest) {
        long total = 0;
        for (GameDataManifest.Entry entry : manifest) {
            total += entry.approxSize;
        }
        return total;
    }

    // Blocking call, meant to run on a background thread. Downloads each
    // file to "<localPath>.part" and only renames it to its final name once
    // the transfer completes successfully -- this is what keeps a killed or
    // interrupted download from ever looking "complete" to
    // isEssentialDataPresent()/Android_HasUsableBaseData() on the next
    // launch, and is also what makes retries safe: any file already present
    // under its final name is skipped instead of re-downloaded.
    public void downloadManifest(File baseDir, List<GameDataManifest.Entry> manifest,
                                  ProgressListener listener, AtomicBoolean cancelled) {
        cleanupOrphanedPartFiles(baseDir, manifest);

        long bytesEstimatedTotal = totalSize(manifest);
        long bytesDownloadedTotal = 0;
        int totalFiles = manifest.size();

        for (int i = 0; i < totalFiles; i++) {
            if (cancelled.get()) {
                return;
            }

            GameDataManifest.Entry entry = manifest.get(i);
            File finalFile = new File(baseDir, entry.localPath);

            if (finalFile.isFile()) {
                // Already downloaded in a previous attempt -- skip, but
                // still count its bytes so the progress bar reflects the
                // real total instead of jumping ahead visually.
                bytesDownloadedTotal += entry.approxSize;
                listener.onFileProgress(bytesDownloadedTotal, bytesEstimatedTotal);
                continue;
            }

            listener.onFileStarted(i, totalFiles, entry.localPath);

            try {
                bytesDownloadedTotal += downloadOneFile(entry, finalFile, bytesDownloadedTotal,
                        bytesEstimatedTotal, listener, cancelled);
            } catch (IOException e) {
                listener.onFailed(entry.localPath, e);
                return;
            }
        }

        if (!cancelled.get()) {
            listener.onCompleted();
        }
    }

    private long downloadOneFile(GameDataManifest.Entry entry, File finalFile, long bytesDownloadedTotalSoFar,
                                  long bytesEstimatedTotal, ProgressListener listener,
                                  AtomicBoolean cancelled) throws IOException {
        File parentDir = finalFile.getParentFile();
        if (parentDir != null && !parentDir.isDirectory() && !parentDir.mkdirs()) {
            throw new IOException("Could not create directory: " + parentDir);
        }

        File partFile = new File(finalFile.getParentFile(), finalFile.getName() + ".part");
        URL url = new URL(RAW_BASE_URL + entry.remotePath);
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setConnectTimeout(CONNECT_TIMEOUT_MS);
        connection.setReadTimeout(READ_TIMEOUT_MS);
        connection.setInstanceFollowRedirects(true);

        long bytesWritten = 0;
        try {
            int status = connection.getResponseCode();
            if (status != HttpURLConnection.HTTP_OK) {
                throw new IOException("HTTP " + status + " for " + entry.remotePath);
            }

            long expectedLength = connection.getContentLengthLong();

            try (InputStream input = connection.getInputStream();
                 FileOutputStream output = new FileOutputStream(partFile)) {
                byte[] buffer = new byte[BUFFER_SIZE];
                int read;
                while ((read = input.read(buffer)) != -1) {
                    if (cancelled.get()) {
                        return bytesWritten;
                    }
                    output.write(buffer, 0, read);
                    bytesWritten += read;
                    listener.onFileProgress(bytesDownloadedTotalSoFar + bytesWritten, bytesEstimatedTotal);
                }
            }

            if (expectedLength >= 0 && bytesWritten != expectedLength) {
                throw new IOException("Size mismatch for " + entry.remotePath
                        + ": expected " + expectedLength + ", got " + bytesWritten);
            }
        } catch (IOException e) {
            //noinspection ResultOfMethodCallIgnored
            partFile.delete();
            throw e;
        } finally {
            connection.disconnect();
        }

        if (!partFile.renameTo(finalFile)) {
            throw new IOException("Could not rename " + partFile + " to " + finalFile);
        }

        return bytesWritten;
    }

    private void cleanupOrphanedPartFiles(File baseDir, List<GameDataManifest.Entry> manifest) {
        for (GameDataManifest.Entry entry : manifest) {
            File finalFile = new File(baseDir, entry.localPath);
            File partFile = new File(finalFile.getParentFile(), finalFile.getName() + ".part");
            if (partFile.isFile()) {
                //noinspection ResultOfMethodCallIgnored
                partFile.delete();
            }
        }
    }
}
