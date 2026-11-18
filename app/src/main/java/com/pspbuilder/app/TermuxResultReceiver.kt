package com.pspbuilder.app

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

class TermuxResultReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val runId = intent.getStringExtra("run_id") ?: return
        val result = intent.getBundleExtra("result")
        val editor = context.getSharedPreferences("termux_results", Context.MODE_PRIVATE).edit()
        if (result == null) {
            editor.putString("${runId}_errmsg", "Termux devolvió una respuesta sin datos.")
                .putInt("${runId}_err", 1)
        } else {
            editor.putString("${runId}_stdout", result.getString("stdout", ""))
                .putString("${runId}_stderr", result.getString("stderr", ""))
                .putInt("${runId}_exit_code", if (result.containsKey("exitCode")) result.getInt("exitCode") else -999)
                .putInt("${runId}_err", result.getInt("err", -1))
                .putString("${runId}_errmsg", result.getString("errmsg", ""))
        }
        editor.putBoolean("${runId}_received", true).commit()
    }
}
