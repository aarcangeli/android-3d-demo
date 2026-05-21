package com.example.openglndkdemo

import android.app.ActivityManager
import android.content.Context
import android.graphics.Color
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.view.Gravity
import android.widget.FrameLayout
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {

    private var glView: GLSurfaceView? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val am = getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        if (am.deviceConfigurationInfo.reqGlEsVersion < 0x20000) {
            Toast.makeText(this, "OpenGL ES 2.0 not supported", Toast.LENGTH_LONG).show()
            return
        }

        val fpsLabel = TextView(this).apply {
            setTextColor(Color.WHITE)
            setShadowLayer(4f, 1f, 1f, Color.BLACK)
            textSize = 14f
            text = "-- FPS"
            val pad = (12 * resources.displayMetrics.density).toInt()
            setPadding(pad, pad, pad, pad)
        }

        val renderer = GLRenderer { fps ->
            runOnUiThread { fpsLabel.text = "$fps FPS" }
        }

        glView = GLSurfaceView(this).also { v ->
            v.setEGLContextClientVersion(2)
            v.setRenderer(renderer)
            v.renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
        }

        val root = FrameLayout(this)
        root.addView(glView, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ))
        root.addView(fpsLabel, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.TOP or Gravity.START
        ))
        setContentView(root)
    }

    override fun onResume() {
        super.onResume()
        glView?.onResume()
    }

    override fun onPause() {
        super.onPause()
        glView?.onPause()
    }
}
