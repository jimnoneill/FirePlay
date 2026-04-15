package org.fireplay

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.net.wifi.WifiManager
import android.os.Build
import android.os.IBinder
import android.util.Log
import java.net.InetAddress
import javax.jmdns.JmDNS
import javax.jmdns.ServiceInfo as JmdnsServiceInfo

class FirePlayService : Service() {

    private var multicastLock: WifiManager.MulticastLock? = null
    private var jmdns: JmDNS? = null
    private var started = false

    companion object {
        private const val TAG = "FirePlay"
        private const val CHANNEL_ID = "fireplay-svc"
        private const val NOTIF_ID = 1
        const val AIRPLAY_PORT = 7000
        const val RAOP_PORT = 7001
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        createChannel()
        startForegroundWithNotif()
        acquireMulticastLock()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (!started) {
            started = true
            Thread { startJmdns() }.start()
            MainActivity.nativeStart("FirePlay", AIRPLAY_PORT, RAOP_PORT)
        }
        return START_STICKY
    }

    override fun onDestroy() {
        started = false
        Thread { stopJmdns() }.start()
        MainActivity.nativeStop()
        multicastLock?.takeIf { it.isHeld }?.release()
        super.onDestroy()
    }

    private fun createChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val nm = getSystemService(NotificationManager::class.java)
            val ch = NotificationChannel(CHANNEL_ID, "FirePlay",
                NotificationManager.IMPORTANCE_LOW)
            nm.createNotificationChannel(ch)
        }
    }

    private fun startForegroundWithNotif() {
        val notif: Notification = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
            Notification.Builder(this, CHANNEL_ID)
                .setContentTitle("FirePlay")
                .setContentText("Ready to receive AirPlay")
                .setSmallIcon(android.R.drawable.stat_sys_download_done)
                .build()
        else
            @Suppress("DEPRECATION")
            Notification.Builder(this)
                .setContentTitle("FirePlay")
                .setContentText("Ready to receive AirPlay")
                .setSmallIcon(android.R.drawable.stat_sys_download_done)
                .build()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(NOTIF_ID, notif,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
        } else {
            startForeground(NOTIF_ID, notif)
        }
    }

    private fun acquireMulticastLock() {
        val wifi = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        multicastLock = wifi.createMulticastLock("fireplay-mdns").apply {
            setReferenceCounted(false)
            acquire()
        }
        Log.i(TAG, "service: MulticastLock acquired")
    }

    private fun startJmdns() {
        try {
            val addr = pickLanAddress()
            Log.i(TAG, "service: jmDNS binding to $addr")
            val j = JmDNS.create(addr, "FirePlay")
            jmdns = j

            val airplayTxt = MainActivity.nativeGetTxtRecordsAirplay()
            val raopTxt = MainActivity.nativeGetTxtRecordsRaop()

            j.registerService(JmdnsServiceInfo.create(
                "_airplay._tcp.local.", "FirePlay", AIRPLAY_PORT, 0, 0, airplayTxt))
            j.registerService(JmdnsServiceInfo.create(
                "_raop._tcp.local.", "00155D629ADD@FirePlay", RAOP_PORT, 0, 0, raopTxt))

            Log.i(TAG, "service: jmDNS registered both services")
        } catch (t: Throwable) {
            Log.e(TAG, "service: jmDNS start failed", t)
        }
    }

    private fun stopJmdns() {
        try {
            jmdns?.unregisterAllServices()
            jmdns?.close()
        } catch (_: Throwable) {}
        jmdns = null
    }

    private fun pickLanAddress(): InetAddress {
        val interfaces = java.net.NetworkInterface.getNetworkInterfaces()
        while (interfaces.hasMoreElements()) {
            val nic = interfaces.nextElement()
            if (nic.isLoopback || !nic.isUp) continue
            val addrs = nic.inetAddresses
            while (addrs.hasMoreElements()) {
                val a = addrs.nextElement()
                if (!a.isLoopbackAddress && a is java.net.Inet4Address
                    && !a.isLinkLocalAddress) {
                    return a
                }
            }
        }
        return InetAddress.getLocalHost()
    }
}
