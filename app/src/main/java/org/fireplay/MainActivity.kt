package org.fireplay

import android.app.Activity
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.WindowManager
import android.widget.LinearLayout

class MainActivity : Activity(), SurfaceHolder.Callback {

    private lateinit var surfaceView: SurfaceView
    private lateinit var splash: LinearLayout

    companion object {
        private const val TAG = "FirePlay"
        init { System.loadLibrary("fireplay") }

        @JvmStatic external fun nativeStart(name: String, airplayPort: Int, raopPort: Int, mac: String, pi: String): Int
        @JvmStatic external fun nativeStop()
        @JvmStatic external fun nativeSetSurface(surface: Surface?)
        @JvmStatic external fun nativeGetTxtRecordsAirplay(): Map<String, String>
        @JvmStatic external fun nativeGetTxtRecordsRaop(): Map<String, String>

        // Renderer notifies us when first video frame renders so we hide splash.
        @JvmStatic
        fun onFirstFrame() {
            instance?.runOnUiThread { instance?.showVideo() }
        }
        @JvmStatic
        fun onMediaIdle() {
            instance?.runOnUiThread { instance?.showSplash() }
        }
        @JvmStatic
        fun onConnectionStart() {
            // Bring Activity to foreground when iPhone connects, so Surface exists.
            val ctx = appContext ?: instance ?: return
            val intent = Intent(ctx, MainActivity::class.java).apply {
                flags = Intent.FLAG_ACTIVITY_NEW_TASK or
                        Intent.FLAG_ACTIVITY_REORDER_TO_FRONT
            }
            ctx.startActivity(intent)
        }
        var appContext: android.content.Context? = null
        var instance: MainActivity? = null
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        setContentView(R.layout.activity_main)
        surfaceView = findViewById(R.id.surface)
        splash = findViewById(R.id.splash)
        surfaceView.holder.addCallback(this)
        instance = this
        appContext = applicationContext
        scheduleAutoStart()

        val intent = Intent(this, FirePlayService::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) startForegroundService(intent)
        else startService(intent)
        Log.i(TAG, "MainActivity: FirePlayService requested")
    }

    override fun onDestroy() {
        super.onDestroy()
        if (instance === this) instance = null
    }

    private fun scheduleAutoStart() {
        val req = androidx.work.PeriodicWorkRequestBuilder<StartWorker>(
            15, java.util.concurrent.TimeUnit.MINUTES
        ).build()
        androidx.work.WorkManager.getInstance(this)
            .enqueueUniquePeriodicWork(
                "fireplay-keepalive",
                androidx.work.ExistingPeriodicWorkPolicy.KEEP,
                req
            )
        Log.i(TAG, "WorkManager keepalive scheduled")
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.i(TAG, "surfaceCreated")
        nativeSetSurface(holder.surface)
    }
    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.i(TAG, "surfaceChanged ${width}x${height}")
        nativeSetSurface(holder.surface)
    }
    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.i(TAG, "surfaceDestroyed")
        nativeSetSurface(null)
    }

    fun showVideo() {
        surfaceView.visibility = View.VISIBLE
        splash.visibility = View.INVISIBLE
    }
    fun showSplash() {
        surfaceView.visibility = View.INVISIBLE
        splash.visibility = View.VISIBLE
    }
}
