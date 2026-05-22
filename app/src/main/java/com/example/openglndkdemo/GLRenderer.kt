package com.example.openglndkdemo

import android.content.res.AssetManager
import android.opengl.GLSurfaceView
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class GLRenderer(
    private val assetManager: AssetManager,
    private val density: Float,
    private val onFpsUpdate: (Int) -> Unit,
) : GLSurfaceView.Renderer {

    private var frameCount  = 0
    private var lastFpsTime = 0L

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) = nativeInit(assetManager, density)
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
                val fps = (frameCount * 1_000_000_000L / elapsed).toInt()
                onFpsUpdate(fps)
                nativeSetFps("$fps FPS")
                frameCount  = 0
                lastFpsTime = now
            }
        }
    }

    fun touchDown  (id: Int, x: Float, y: Float) = nativeTouchDown  (id, x, y)
    fun touchMove  (id: Int, x: Float, y: Float) = nativeTouchMove  (id, x, y)
    fun touchUp    (id: Int, x: Float, y: Float) = nativeTouchUp    (id, x, y)
    fun touchCancel(id: Int, x: Float, y: Float) = nativeTouchCancel(id, x, y)
    fun key        (code: Int, unicode: Int, down: Boolean) = nativeKey(code, unicode, down)

    private external fun nativeInit    (assetMgr: AssetManager, density: Float)
    private external fun nativeResize  (width: Int, height: Int)
    private external fun nativeDraw    ()
    private external fun nativeSetFps  (fps: String)
    private external fun nativeTouchDown  (id: Int, x: Float, y: Float)
    private external fun nativeTouchMove  (id: Int, x: Float, y: Float)
    private external fun nativeTouchUp    (id: Int, x: Float, y: Float)
    private external fun nativeTouchCancel(id: Int, x: Float, y: Float)
    private external fun nativeKey        (keyCode: Int, unicode: Int, down: Boolean)

    companion object {
        init { System.loadLibrary("openglndkdemo") }
    }
}
