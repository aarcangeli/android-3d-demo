package com.example.openglndkdemo

import android.app.ActivityManager
import android.content.Context
import android.opengl.GLSurfaceView
import android.os.Bundle
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

        glView = GLSurfaceView(this).also { v ->
            v.setEGLContextClientVersion(2)
            v.setRenderer(GLRenderer())
            v.renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
            setContentView(v)
        }
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
