package com.example.openglndkdemo

import android.opengl.GLSurfaceView
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class GLRenderer(private val onFpsUpdate: (Int) -> Unit) : GLSurfaceView.Renderer {

    private var frameCount = 0
    private var lastFpsTime = 0L

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) = nativeInit()
    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) = nativeResize(width, height)

    override fun onDrawFrame(gl: GL10?) {
        nativeDraw()
        frameCount++
        val now = System.nanoTime()
        if (lastFpsTime == 0L) {
            lastFpsTime = now
        } else {
            val elapsed = now - lastFpsTime
            if (elapsed >= 1_000_000_000L) {
                onFpsUpdate((frameCount * 1_000_000_000L / elapsed).toInt())
                frameCount = 0
                lastFpsTime = now
            }
        }
    }

    private external fun nativeInit()
    private external fun nativeResize(width: Int, height: Int)
    private external fun nativeDraw()

    companion object {
        init { System.loadLibrary("openglndkdemo") }
    }
}
