package org.ezquake.android;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;
import android.util.Log;

// Keeps the game process running while it is in the background, so a
// minimized client stays connected to its QuakeWorld server. Without a
// foreground service Android freezes cached apps after a few seconds and the
// server drops the connection. Started when the game leaves the screen,
// stopped as soon as it comes back.
public class GameConnectionService extends Service {
    private static final String TAG = "nextQuake";
    private static final String CHANNEL_ID = "game_connection";
    private static final int NOTIFICATION_ID = 1;
    // Upper bound for the CPU wake lock if the player forgets the game in the
    // background with the screen off; the service itself keeps running.
    private static final long WAKE_LOCK_TIMEOUT_MS = 30L * 60L * 1000L;

    private PowerManager.WakeLock wakeLock;

    public static void start(Context context) {
        try {
            context.startForegroundService(new Intent(context, GameConnectionService.class));
        } catch (Exception e) {
            // Background-start restrictions or a missing permission must never
            // take the game down; the client then just behaves as before.
            Log.w(TAG, "Unable to start background connection service", e);
        }
    }

    public static void stop(Context context) {
        context.stopService(new Intent(context, GameConnectionService.class));
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Notification notification = buildNotification();
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
            } else {
                startForeground(NOTIFICATION_ID, notification);
            }
        } catch (Exception e) {
            Log.w(TAG, "Unable to enter foreground for background connection", e);
            stopSelf();
            return START_NOT_STICKY;
        }

        if (wakeLock == null) {
            PowerManager power = (PowerManager) getSystemService(Context.POWER_SERVICE);
            wakeLock = power.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "nextQuake:connection");
            wakeLock.setReferenceCounted(false);
        }
        wakeLock.acquire(WAKE_LOCK_TIMEOUT_MS);
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        if (wakeLock != null && wakeLock.isHeld()) {
            wakeLock.release();
        }
        wakeLock = null;
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private Notification buildNotification() {
        NotificationManager manager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (manager.getNotificationChannel(CHANNEL_ID) == null) {
            NotificationChannel channel = new NotificationChannel(CHANNEL_ID,
                    "Server connection", NotificationManager.IMPORTANCE_LOW);
            channel.setDescription("Keeps nextQuake connected while minimized");
            channel.setShowBadge(false);
            manager.createNotificationChannel(channel);
        }

        Intent resume = new Intent(this, EzQuakeActivity.class);
        resume.addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
        PendingIntent content = PendingIntent.getActivity(this, 0, resume,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);

        return new Notification.Builder(this, CHANNEL_ID)
                .setSmallIcon(R.mipmap.nextquake_icon)
                .setContentTitle("nextQuake")
                .setContentText("Running in the background, server connection kept")
                .setContentIntent(content)
                .setOngoing(true)
                .setCategory(Notification.CATEGORY_SERVICE)
                .build();
    }
}
