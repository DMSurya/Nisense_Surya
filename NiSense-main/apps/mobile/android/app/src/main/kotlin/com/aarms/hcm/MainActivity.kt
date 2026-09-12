package com.aarms.hcm

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.location.LocationManager
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.wifi.WifiInfo
import android.net.wifi.WifiManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import androidx.core.content.ContextCompat
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel
import java.util.concurrent.atomic.AtomicBoolean

class MainActivity : FlutterActivity() {
    companion object {
        private const val CHANNEL = "com.aarms.hcm/wifi_info"
    }

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, CHANNEL)
            .setMethodCallHandler { call, result ->
                when (call.method) {
                    "getWifiSsid" -> readWifiSsidAsync { payload ->
                        result.success(payload)
                    }
                    "isLocationEnabled" -> result.success(isLocationEnabled())
                    else -> result.notImplemented()
                }
            }
    }

    private fun hasFineLocation(): Boolean {
        return ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.ACCESS_FINE_LOCATION,
        ) == PackageManager.PERMISSION_GRANTED
    }

    private fun isLocationEnabled(): Boolean {
        val lm = getSystemService(Context.LOCATION_SERVICE) as LocationManager
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            lm.isLocationEnabled
        } else {
            @Suppress("DEPRECATION")
            lm.isProviderEnabled(LocationManager.GPS_PROVIDER) ||
                lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER)
        }
    }

    private fun normalizeSsid(raw: String?): String? {
        if (raw.isNullOrBlank()) return null
        var s = raw.trim()
        if (s.length >= 2 && s.startsWith("\"") && s.endsWith("\"")) {
            s = s.substring(1, s.length - 1)
        }
        if (s.isEmpty() || s.equals("<unknown ssid>", ignoreCase = true)) {
            return null
        }
        return s
    }

    private fun deprecatedConnectionSsid(): String? {
        @Suppress("DEPRECATION")
        val wifiManager =
            applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        @Suppress("DEPRECATION")
        return normalizeSsid(wifiManager.connectionInfo?.ssid)
    }

    private fun readWifiSsidAsync(done: (Map<String, Any?>) -> Unit) {
        val finished = AtomicBoolean(false)
        fun complete(ssid: String?, error: String?) {
            if (!finished.compareAndSet(false, true)) return
            done(
                mapOf(
                    "ssid" to ssid,
                    "error" to error,
                    "locationEnabled" to isLocationEnabled(),
                    "locationGranted" to hasFineLocation(),
                ),
            )
        }

        if (!hasFineLocation()) {
            complete(null, "location_permission")
            return
        }
        if (!isLocationEnabled()) {
            complete(null, "location_disabled")
            return
        }

        // Android 12+: location-sensitive SSID requires NetworkCallback +
        // FLAG_INCLUDE_LOCATION_INFO. Deprecated connectionInfo often returns
        // <unknown ssid> even when location is granted.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val cm = getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
            val request =
                NetworkRequest.Builder()
                    .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                    .build()
            val callback =
                object : ConnectivityManager.NetworkCallback(
                    FLAG_INCLUDE_LOCATION_INFO,
                ) {
                    override fun onCapabilitiesChanged(
                        network: Network,
                        networkCapabilities: NetworkCapabilities,
                    ) {
                        val info = networkCapabilities.transportInfo as? WifiInfo
                        val ssid = normalizeSsid(info?.ssid)
                        try {
                            cm.unregisterNetworkCallback(this)
                        } catch (_: IllegalArgumentException) {
                            // Already unregistered by timeout.
                        }
                        if (ssid != null) {
                            complete(ssid, null)
                        } else {
                            val fallback = deprecatedConnectionSsid()
                            complete(fallback, if (fallback == null) "unavailable" else null)
                        }
                    }

                    override fun onUnavailable() {
                        try {
                            cm.unregisterNetworkCallback(this)
                        } catch (_: IllegalArgumentException) {
                        }
                        val fallback = deprecatedConnectionSsid()
                        complete(fallback, if (fallback == null) "unavailable" else null)
                    }
                }

            try {
                cm.registerNetworkCallback(request, callback)
            } catch (e: SecurityException) {
                complete(null, "location_permission")
                return
            }

            Handler(Looper.getMainLooper()).postDelayed({
                try {
                    cm.unregisterNetworkCallback(callback)
                } catch (_: IllegalArgumentException) {
                }
                val fallback = deprecatedConnectionSsid()
                complete(fallback, if (fallback == null) "unavailable" else null)
            }, 2500L)
            return
        }

        val legacy = deprecatedConnectionSsid()
        complete(legacy, if (legacy == null) "unavailable" else null)
    }
}
