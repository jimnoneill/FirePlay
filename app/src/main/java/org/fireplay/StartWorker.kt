package org.fireplay

import android.content.Context
import android.content.Intent
import android.os.Build
import android.util.Log
import androidx.work.Worker
import androidx.work.WorkerParameters

class StartWorker(ctx: Context, params: WorkerParameters) : Worker(ctx, params) {
    override fun doWork(): Result {
        Log.i("FirePlay", "StartWorker: launching Activity + Service")
        // Only start the Service (for Music/audio). Activity launches
        // automatically when iPhone sends video (onConnectionStart).
        val svc = Intent(applicationContext, FirePlayService::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            applicationContext.startForegroundService(svc)
        } else {
            applicationContext.startService(svc)
        }
        return Result.success()
    }
}
