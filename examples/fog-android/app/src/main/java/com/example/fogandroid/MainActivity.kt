package com.example.fogandroid

import android.app.Activity
import android.os.Bundle
import android.widget.ScrollView
import android.widget.TextView

/**
 * alib6 → Android 移植验证 App：
 * 界面展示 alib6 各模块(core/log/ecs/data...)的综合测试结果，
 * 同内容也打到 logcat(tag=AlibProbe)。
 */
class MainActivity : Activity() {

    private external fun alibProbe(): String

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        System.loadLibrary("fogandroid")

        val textView = TextView(this).apply {
            text = alibProbe()
            textSize = 12f
            setPadding(36, 72, 36, 36)
        }
        setContentView(ScrollView(this).apply { addView(textView) })
    }
}
