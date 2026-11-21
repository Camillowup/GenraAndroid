// SPDX-License-Identifier: GPL-3.0-only
package ch.camillowup.genrabox;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.MediaStore;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStream;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/** GENRA BOX: the engine plays in native code, the panel is a page in a WebView (assets/ui). */
public class MainActivity extends Activity {
    private WebView web;

    @SuppressLint("SetJavaScriptEnabled")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        Native.init(getFilesDir().getAbsolutePath());

        web = new WebView(this);
        web.setBackgroundColor(Color.rgb(0x17, 0x18, 0x1d));
        web.setOverScrollMode(View.OVER_SCROLL_NEVER);
        web.setVerticalScrollBarEnabled(false);
        web.setHorizontalScrollBarEnabled(false);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setAllowFileAccess(false);
        s.setTextZoom(100);
        s.setSupportZoom(false);
        s.setBuiltInZoomControls(false);
        s.setMediaPlaybackRequiresUserGesture(true);
        web.setWebViewClient(new WebViewClient());
        web.setWebChromeClient(new WebChromeClient());
        web.addJavascriptInterface(new Bridge(), "GB");
        web.loadUrl("file:///android_asset/ui/index.html");
        setContentView(web);
        hideBars();
    }

    private void hideBars() {
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController c = getWindow().getInsetsController();
            if (c != null) {
                c.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                c.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus)
            hideBars();
    }

    @Override
    protected void onStart() {
        super.onStart();
        Native.start();
    }

    @Override
    protected void onPause() {
        super.onPause();
        Native.save();
    }

    @Override
    protected void onStop() {
        Native.stop();
        Native.save();
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        if (web != null)
            web.destroy();
        super.onDestroy();
    }

    /** A MIDI file into Downloads/GENRA BOX; the text the panel shows. */
    private String saveMidi(byte[] data, String base) {
        if (data == null || data.length == 0)
            return "Nothing to export";
        String stamp = new SimpleDateFormat("yyyyMMdd-HHmmss", Locale.ROOT).format(new Date());
        String name = base.replaceAll("[^A-Za-z0-9 _.-]", "_") + "-" + stamp + ".mid";
        try {
            if (Build.VERSION.SDK_INT >= 29) {
                ContentValues v = new ContentValues();
                v.put(MediaStore.Downloads.DISPLAY_NAME, name);
                v.put(MediaStore.Downloads.MIME_TYPE, "audio/midi");
                v.put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS + "/GENRA BOX");
                ContentResolver r = getContentResolver();
                Uri uri = r.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, v);
                if (uri == null)
                    return "Saving failed";
                try (OutputStream o = r.openOutputStream(uri)) {
                    if (o == null)
                        return "Saving failed";
                    o.write(data);
                }
                return "Saved: Download/GENRA BOX/" + name;
            }
            File dir = new File(getExternalFilesDir(null), "MIDI");
            if (!dir.exists() && !dir.mkdirs())
                return "Saving failed";
            File f = new File(dir, name);
            try (FileOutputStream o = new FileOutputStream(f)) {
                o.write(data);
            }
            return "Saved: " + f.getAbsolutePath();
        } catch (Exception e) {
            return "Saving failed: " + e.getMessage();
        }
    }

    /** What the page calls (window.GB). */
    public final class Bridge {
        @JavascriptInterface public String meta() { return Native.meta(); }
        @JavascriptInterface public String state(int known) { return Native.state(known); }
        @JavascriptInterface public String fast() { return Native.fast(); }
        @JavascriptInterface public void cmd(int op, int a, int b, float f) { Native.cmd(op, a, b, f); }
        @JavascriptInterface public void savePreset(int track, String name) { Native.savePreset(track, name); }
        @JavascriptInterface public void save() { Native.save(); }
        @JavascriptInterface public String audioInfo() { return Native.audioInfo(); }
        @JavascriptInterface public String version() { return BuildConfigInfo.VERSION; }
        @JavascriptInterface public String exportMidi(int what, int idx) {
            byte[] d = Native.exportMidi(what, idx);
            return saveMidi(d, Native.exportName());
        }
    }
}
