package org.ezquake.android;

import android.content.Context;

import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;

// Java side of the crash log: uncaught exceptions are appended to the same
// <basedir>/ezquake/crash.txt that src/android_crash.c writes native crashes
// to, then passed on to the default handler. Nothing leaves the device.
final class CrashLog {
    private static final String CRASH_FILE = "/storage/emulated/0/Documents/ezQuake/ezquake/crash.txt";
    private static boolean installed;
    private static String versionName = "?";

    private CrashLog() {
    }

    static synchronized void installJavaHandler(Context context) {
        if (installed) {
            return;
        }
        installed = true;
        try {
            versionName = context.getPackageManager().getPackageInfo(context.getPackageName(), 0).versionName;
        } catch (Exception ignored) {
            // keep "?"
        }
        final Thread.UncaughtExceptionHandler previous = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread thread, Throwable error) {
                write(thread, error);
                if (previous != null) {
                    previous.uncaughtException(thread, error);
                }
            }
        });
    }

    private static void write(Thread thread, Throwable error) {
        try {
            File file = new File(CRASH_FILE);
            File dir = file.getParentFile();
            if (dir != null) {
                dir.mkdirs();
            }
            try (PrintWriter out = new PrintWriter(new FileWriter(file, true))) {
                out.println("==== nextQuake " + versionName + " Java crash, unix time "
                        + (System.currentTimeMillis() / 1000) + " ====");
                out.println("thread " + thread.getName());
                error.printStackTrace(out);
                out.println();
            }
        } catch (Throwable ignored) {
            // the crash log is best effort; never mask the original crash
        }
    }
}
