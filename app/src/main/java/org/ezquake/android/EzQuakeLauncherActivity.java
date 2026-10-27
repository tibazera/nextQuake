package org.ezquake.android;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.StatFs;
import android.provider.Settings;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

public class EzQuakeLauncherActivity extends Activity {
    // Must match ANDROID_EZQUAKE_BASEDIR in src/sys_posix.c.
    private static final String HD_PROMPT_MARKER = "ezquake/.hd_textures_prompted";
    private static final String BASEDIR = "/storage/emulated/0/Documents/ezQuake";
    private static final int REQUEST_LEGACY_STORAGE = 1001;
    private static final int REQUEST_MANAGE_STORAGE = 1002;

    private static final String PREFS_NAME = "game_data_prefs";
    private static final String PREF_HD_TEXTURES_PROMPTED = "hd_textures_prompted";
    private static final String PREF_HD_TEXTURES_INSTALLED = "hd_textures_installed";

    // Rough safety margin added on top of a manifest's own approxSize sum
    // when checking free disk space -- covers filesystem overhead and any
    // .part files briefly coexisting with their final renamed counterpart.
    private static final double DISK_SPACE_SAFETY_MARGIN = 1.2;

    private final ExecutorService downloadExecutor = Executors.newSingleThreadExecutor();
    private final AtomicBoolean downloadCancelled = new AtomicBoolean(false);

    private TextView downloadStatusText;
    private ProgressBar downloadProgressBar;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestStorageAccessThenLaunch();
    }

    @Override
    protected void onDestroy() {
        downloadCancelled.set(true);
        downloadExecutor.shutdownNow();
        super.onDestroy();
    }

    private void requestStorageAccessThenLaunch() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (Environment.isExternalStorageManager()) {
                launchGame();
                return;
            }
            Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
            intent.setData(Uri.parse("package:" + getPackageName()));
            try {
                startActivityForResult(intent, REQUEST_MANAGE_STORAGE);
            } catch (ActivityNotFoundException e) {
                try {
                    startActivityForResult(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION),
                            REQUEST_MANAGE_STORAGE);
                } catch (ActivityNotFoundException e2) {
                    launchGame();
                }
            }
        } else if (checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                == PackageManager.PERMISSION_GRANTED) {
            launchGame();
        } else {
            requestPermissions(new String[] {
                    Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE
            }, REQUEST_LEGACY_STORAGE);
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_LEGACY_STORAGE) {
            launchGame();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_MANAGE_STORAGE) {
            launchGame();
        }
    }

    private void launchGame() {
        File baseDir = new File(BASEDIR);
        baseDir.mkdirs();

        if (GameDataInstaller.isEssentialDataPresent(baseDir)) {
            List<GameDataManifest.Entry> missing = GameDataInstaller.missingRequiredFiles(baseDir);
            if (missing.isEmpty()) {
                checkHdTexturesPromptThenStartGame(baseDir);
            } else {
                startDownload(baseDir, missing);
            }
            return;
        }

        startEssentialDownload(baseDir);
    }

    private void startEssentialDownload(File baseDir) {
        startDownload(baseDir, GameDataManifest.essential());
    }

    private void startDownload(File baseDir, List<GameDataManifest.Entry> manifest) {
        setContentView(R.layout.activity_launcher_progress);
        downloadStatusText = findViewById(R.id.download_status_text);
        downloadProgressBar = findViewById(R.id.download_progress_bar);

        runManifestDownload(baseDir, manifest, new Runnable() {
            @Override
            public void run() {
                checkHdTexturesPromptThenStartGame(baseDir);
            }
        });
    }

    private void runManifestDownload(File baseDir, List<GameDataManifest.Entry> manifest, Runnable onSuccess) {
        long requiredBytes = (long) (GameDataInstaller.totalSize(manifest) * DISK_SPACE_SAFETY_MARGIN);
        if (getAvailableBytes(baseDir) < requiredBytes) {
            showRetryDialog("Not enough free storage space to download game data.", baseDir, manifest, onSuccess);
            return;
        }

        downloadCancelled.set(false);
        downloadExecutor.execute(new Runnable() {
            @Override
            public void run() {
                new GameDataInstaller().downloadManifest(baseDir, manifest, new GameDataInstaller.ProgressListener() {
                    private long lastUiUpdateBytes = -1;

                    @Override
                    public void onFileStarted(int fileIndex, int totalFiles, String relativePath) {
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                if (downloadStatusText != null) {
                                    downloadStatusText.setText(
                                            "Downloading: " + relativePath + " (" + (fileIndex + 1) + "/" + totalFiles + ")");
                                }
                            }
                        });
                    }

                    @Override
                    public void onFileProgress(final long bytesDownloadedTotal, final long bytesEstimatedTotal) {
                        // Throttle UI updates so thousands of small texture
                        // files don't flood the UI thread with posts.
                        if (lastUiUpdateBytes >= 0 && bytesDownloadedTotal - lastUiUpdateBytes < 64 * 1024) {
                            return;
                        }
                        lastUiUpdateBytes = bytesDownloadedTotal;
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                if (downloadProgressBar != null && bytesEstimatedTotal > 0) {
                                    int progress = (int) Math.min(10000,
                                            (bytesDownloadedTotal * 10000L) / bytesEstimatedTotal);
                                    downloadProgressBar.setProgress(progress);
                                }
                            }
                        });
                    }

                    @Override
                    public void onCompleted() {
                        runOnUiThread(onSuccess);
                    }

                    @Override
                    public void onFailed(String relativePath, Exception cause) {
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                showRetryDialog("Failed to download " + relativePath + ": " + cause.getMessage(),
                                        baseDir, manifest, onSuccess);
                            }
                        });
                    }
                }, downloadCancelled);
            }
        });
    }

    private long getAvailableBytes(File baseDir) {
        StatFs stat = new StatFs(baseDir.getAbsolutePath());
        return stat.getAvailableBytes();
    }

    private void showRetryDialog(String message, File baseDir, List<GameDataManifest.Entry> manifest, Runnable onSuccess) {
        if (isFinishing()) {
            return;
        }
        new AlertDialog.Builder(this)
                .setTitle("Download failed")
                .setMessage(message)
                .setCancelable(false)
                .setPositiveButton("Try again", new android.content.DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(android.content.DialogInterface dialog, int which) {
                        runManifestDownload(baseDir, manifest, onSuccess);
                    }
                })
                .setNegativeButton("Exit", new android.content.DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(android.content.DialogInterface dialog, int which) {
                        finish();
                    }
                })
                .show();
    }

    private void checkHdTexturesPromptThenStartGame(File baseDir) {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        boolean alreadyPrompted = prefs.getBoolean(PREF_HD_TEXTURES_PROMPTED, false);
        boolean alreadyInstalled = prefs.getBoolean(PREF_HD_TEXTURES_INSTALLED, false);

        if (alreadyPrompted || alreadyInstalled) {
            startGameActivity();
            return;
        }

        // Detect instead of asking: the pack is already in the game folder, or the
        // question was answered before. The marker file lives next to the game data,
        // so it survives uninstall/reinstall (the preferences above do not).
        if (GameDataInstaller.allFilesPresent(baseDir, GameDataManifest.hdTextures())) {
            prefs.edit().putBoolean(PREF_HD_TEXTURES_INSTALLED, true).apply();
            startGameActivity();
            return;
        }
        if (new File(baseDir, HD_PROMPT_MARKER).isFile()) {
            prefs.edit().putBoolean(PREF_HD_TEXTURES_PROMPTED, true).apply();
            startGameActivity();
            return;
        }

        if (isFinishing()) {
            startGameActivity();
            return;
        }

        new AlertDialog.Builder(this)
                .setTitle("Optional HD textures")
                .setMessage("Download the high-resolution texture pack (~385MB)? "
                        + "You can play without it -- this only affects visual quality.")
                .setCancelable(false)
                .setPositiveButton("Download now", new android.content.DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(android.content.DialogInterface dialog, int which) {
                        prefs.edit().putBoolean(PREF_HD_TEXTURES_PROMPTED, true).apply();
                        markHdPrompted(baseDir);
                        setContentView(R.layout.activity_launcher_progress);
                        downloadStatusText = findViewById(R.id.download_status_text);
                        downloadProgressBar = findViewById(R.id.download_progress_bar);
                        runManifestDownload(baseDir, GameDataManifest.hdTextures(), new Runnable() {
                            @Override
                            public void run() {
                                prefs.edit().putBoolean(PREF_HD_TEXTURES_INSTALLED, true).apply();
                                startGameActivity();
                            }
                        });
                    }
                })
                .setNegativeButton("Not now", new android.content.DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(android.content.DialogInterface dialog, int which) {
                        prefs.edit().putBoolean(PREF_HD_TEXTURES_PROMPTED, true).apply();
                        markHdPrompted(baseDir);
                        startGameActivity();
                    }
                })
                .show();
    }

    private void markHdPrompted(File baseDir) {
        try {
            File marker = new File(baseDir, HD_PROMPT_MARKER);
            File dir = marker.getParentFile();
            if (dir != null) {
                dir.mkdirs();
            }
            marker.createNewFile();
        } catch (Exception ignored) {
            // only costs a repeated question after a reinstall
        }
    }

    // On-screen control artwork (Kenney "Mobile Controls", CC0) ships inside the
    // APK; the engine loads it as loose files from <basedir>/ezquake/gfx/touch/.
    private void extractTouchAssets() {
        try {
            File outDir = new File(BASEDIR, "ezquake/gfx/touch");
            outDir.mkdirs();
            String[] names = getAssets().list("touch");
            if (names == null) {
                return;
            }
            for (String name : names) {
                File outFile = new File(outDir, name);
                java.io.InputStream in = getAssets().open("touch/" + name);
                try {
                    if (outFile.isFile() && outFile.length() == in.available()) {
                        continue;
                    }
                    java.io.OutputStream out = new java.io.FileOutputStream(outFile);
                    try {
                        byte[] buffer = new byte[16384];
                        int read;
                        while ((read = in.read(buffer)) > 0) {
                            out.write(buffer, 0, read);
                        }
                    } finally {
                        out.close();
                    }
                } finally {
                    in.close();
                }
            }
        } catch (Exception ignored) {
            // artwork is optional: buttons fall back to plain circles with text
        }
    }

    // First run only: install the bundled default config.cfg when the game folder has none.
    // An existing config.cfg (the player's own) is never touched.
    private void installDefaultConfigIfMissing() {
        try {
            File config = new File(BASEDIR, "ezquake/configs/config.cfg");
            if (config.exists()) {
                return;
            }
            File dir = config.getParentFile();
            if (dir != null) {
                dir.mkdirs();
            }
            java.io.InputStream in = getAssets().open("defaults/config.cfg");
            try {
                java.io.OutputStream out = new java.io.FileOutputStream(config);
                try {
                    byte[] buffer = new byte[16384];
                    int read;
                    while ((read = in.read(buffer)) > 0) {
                        out.write(buffer, 0, read);
                    }
                } finally {
                    out.close();
                }
            } finally {
                in.close();
            }
        } catch (Exception ignored) {
            // optional: without it the engine just starts with its built-in defaults
        }
    }

    private void startGameActivity() {
        installDefaultConfigIfMissing();
        extractTouchAssets();
        startActivity(new Intent(this, EzQuakeActivity.class));
        finish();
    }
}
