package org.fireplay

import android.app.Activity
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.WindowManager
import android.widget.FrameLayout

class MainActivity : Activity(), SurfaceHolder.Callback {

    companion object {
        private const val TAG = "FirePlay"
        init { System.loadLibrary("fireplay") }

        @JvmStatic external fun nativeStart(name: String, airplayPort: Int, raopPort: Int): Int
        @JvmStatic external fun nativeStop()
        @JvmStatic external fun nativeSetSurface(surface: Surface?)
        @JvmStatic external fun nativeGetTxtRecordsAirplay(): Map<String, String>
        @JvmStatic external fun nativeGetTxtRecordsRaop(): Map<String, String>
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        val surfaceView = SurfaceView(this).apply { holder.addCallback(this@MainActivity) }
        setContentView(FrameLayout(this).apply {
            addView(surfaceView, FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT))
        })

        val intent = Intent(this, FirePlayService::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(intent)
        } else {
            startService(intent)
        }
        Log.i(TAG, "MainActivity: FirePlayService requested")
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
}
