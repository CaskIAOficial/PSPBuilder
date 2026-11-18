package com.pspbuilder.app

import android.app.IntentService
import android.content.Intent

class TermuxResultService : IntentService("PSPBuilderTermuxResult") {
    override fun onHandleIntent(intent: Intent?) {
        if (intent == null) return
        val runId = intent.getStringExtra("run_id") ?: return
        val result = intent.getBundleExtra("result") ?: return
        getSharedPreferences("termux_results", MODE_PRIVATE).edit()
            .putString("${runId}_stdout", result.getString("stdout", ""))
            .putString("${runId}_stderr", result.getString("stderr", ""))
            .putInt("${runId}_exit_code", result.getInt("exitCode", -999))
            .putInt("${runId}_err", result.getInt("err", 0))
            .putString("${runId}_errmsg", result.getString("errmsg", ""))
            .putBoolean("${runId}_received", true)
            .apply()
    }
}
