package org.ezquake.android;

import android.os.Bundle;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;

import org.libsdl.app.SDLActivity;

public class EzQuakeActivity extends SDLActivity {
    private ImageView startupSplashView;
    private ProgressBar startupProgressView;

    @Override
    protected String[] getLibraries() {
        return new String[] { "ezquake" };
    }

    @Override
    protected String getMainFunction() {
        return "main";
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // SDLActivity's SurfaceView paints black as soon as it's attached,
        // well before the Vulkan device/swapchain are ready to present a
        // first frame -- the window's splash background (styles.xml) is
        // already obscured by then, so without this the user just sees a
        // black screen during native/renderer init. Stack the same splash
        // artwork as a normal View on top of the SurfaceView instead, and
        // remove it once hideStartupSplash() below fires.
        startupSplashView = new ImageView(this);
        startupSplashView.setImageResource(R.drawable.ezquake_splash_background);
        startupSplashView.setScaleType(ImageView.ScaleType.CENTER_CROP);
        ((ViewGroup) getContentView()).addView(startupSplashView, new RelativeLayout.LayoutParams(
                RelativeLayout.LayoutParams.MATCH_PARENT, RelativeLayout.LayoutParams.MATCH_PARENT));

        // Indeterminate, not a real percentage: native startup (filesystem,
        // configs, Vulkan device/swapchain setup) has no progress checkpoints
        // wired through yet, so a determinate bar would either sit at 0% for
        // most of the wait or jump unconvincingly near the end. This is just
        // "still working", not "frozen", until hideStartupSplash() fires.
        startupProgressView = new ProgressBar(this, null, android.R.attr.progressBarStyleLarge);
        RelativeLayout.LayoutParams progressParams = new RelativeLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        progressParams.addRule(RelativeLayout.CENTER_HORIZONTAL);
        progressParams.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
        progressParams.bottomMargin = (int) (64 * getResources().getDisplayMetrics().density);
        ((ViewGroup) getContentView()).addView(startupProgressView, progressParams);
    }

    // Called via JNI (GetMethodID "setRenderHeight" "(I)V") from vk_main.c when
    // the chosen resolution (vid_android_resolution) changes. 0 = default 1080p envelope.
    public void setRenderHeight(final int maxHeight) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (mSurface != null) {
                    mSurface.applyRenderHeightCap(maxHeight);
                }
            }
        });
    }

    // Called via JNI (GetMethodID "hideStartupSplash" "()V") from vk_main.c
    // once the Vulkan renderer has drawn its first frame, so the static
    // splash drawable set as the window background -- and the overlay view
    // added above -- don't linger behind/over it.
    public void hideStartupSplash() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                getWindow().setBackgroundDrawable(null);
                if (startupSplashView != null) {
                    ViewGroup parent = (ViewGroup) startupSplashView.getParent();
                    if (parent != null) {
                        parent.removeView(startupSplashView);
                    }
                    startupSplashView = null;
                }
                if (startupProgressView != null) {
                    ViewGroup parent = (ViewGroup) startupProgressView.getParent();
                    if (parent != null) {
                        parent.removeView(startupProgressView);
                    }
                    startupProgressView = null;
                }
            }
        });
    }
}
