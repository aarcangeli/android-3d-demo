package com.example.openglndkdemo

import android.app.ActivityManager
import android.content.Context
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.view.KeyEvent
import android.view.MotionEvent
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {

    private var glView: GLSurfaceView? = null
    private var renderer: GLRenderer?  = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val am = getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        if (am.deviceConfigurationInfo.reqGlEsVersion < 0x20000) {
            Toast.makeText(this, "OpenGL ES 2.0 not supported", Toast.LENGTH_LONG).show()
            return
        }

        renderer = GLRenderer(assets) { /* FPS is updated natively via nativeSetFps */ }

        glView = object : GLSurfaceView(this) {
            // Touch events go directly to the native input queue.
            override fun onTouchEvent(event: MotionEvent): Boolean {
                val r = renderer ?: return false
                val action = event.actionMasked
                val idx    = event.actionIndex
                val id     = event.getPointerId(idx)
                val x      = event.getX(idx)
                val y      = event.getY(idx)

                when (action) {
                    MotionEvent.ACTION_DOWN,
                    MotionEvent.ACTION_POINTER_DOWN -> r.touchDown(id, x, y)

                    MotionEvent.ACTION_MOVE -> {
                        for (i in 0 until event.pointerCount) {
                            r.touchMove(event.getPointerId(i),
                                        event.getX(i), event.getY(i))
                        }
                    }

                    MotionEvent.ACTION_UP,
                    MotionEvent.ACTION_POINTER_UP  -> r.touchUp(id, x, y)

                    MotionEvent.ACTION_CANCEL       -> r.touchCancel(id, x, y)
                }
                return true
            }
        }.also { v ->
            v.setEGLContextClientVersion(2)
            v.setRenderer(renderer)
            v.renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
        }

        setContentView(glView)
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        renderer?.key(keyCode, event?.unicodeChar ?: 0, true)
        return super.onKeyDown(keyCode, event)
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        renderer?.key(keyCode, event?.unicodeChar ?: 0, false)
        return super.onKeyUp(keyCode, event)
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
