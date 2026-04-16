package org.fireplay

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.net.ConnectivityManager
import android.os.Build
import android.util.Log

class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val action = intent.action ?: return
        if (action == Intent.ACTION_BOOT_COMPLETED ||
            action == ConnectivityManager.CONNECTIVITY_ACTION) {

            val cm = context.getSystemService(Context.CONNECTIVITY_SERVICE) as? ConnectivityManager
            val net = cm?.activeNetworkInfo
            if (net == null || !net.isConnected) return

            val running = try {
                val am = context.getSystemService(Context.ACTIVITY_SERVICE) as android.app.ActivityManager
                am.getRunningServices(100).any { it.service.className == FirePlayService::class.java.name }
            } catch (_: Exception) { false }

            if (!running) {
                Log.i("FirePlay", "Network up ($action) — starting FirePlayService")
                val svc = Intent(context, FirePlayService::class.java)
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    context.startForegroundService(svc)
                } else {
                    context.startService(svc)
                }
            }
        }
    }
}
