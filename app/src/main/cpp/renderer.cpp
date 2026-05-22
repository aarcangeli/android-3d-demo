#include <jni.h>
#include <GLES2/gl2.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <cmath>
#include <cstring>
#include <string>

#include "ui/ui_system.h"
#include "ui/widget.h"
#include "demo_scene.h"

#define LOG_TAG "OpenGLNDK"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ── Triangle shader ───────────────────────────────────────────────────────────

namespace {

const char* TRI_VERT = R"glsl(
attribute vec2 aPos;
attribute vec4 aColor;
uniform vec2 uScale;
varying vec4 vColor;
void main() {
    vColor      = aColor;
    gl_Position = vec4(aPos * uScale, 0.0, 1.0);
}
)glsl";

const char* TRI_FRAG = R"glsl(
precision mediump float;
varying vec4 vColor;
void main() { gl_FragColor = vColor; }
)glsl";

GLuint g_triProg  = 0;
GLint  g_aPos     = -1, g_aColor = -1, g_uScale = -1;
float  g_scaleX   = 1.f, g_scaleY = 1.f;
float  g_angle    = 0.f;
float  g_speed    = 1.f;

int    g_screenW  = 0, g_screenH = 0;

ui::UISystem  g_ui;
ui::Label*    g_fpsLabel    = nullptr;
ui::Label*    g_angleLabel  = nullptr;
ui::Label*    g_statusLabel = nullptr;

GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512]; glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        LOGE("Shader: %s", buf);
        glDeleteShader(s); return 0;
    }
    return s;
}

// Renders the spinning triangle into the given pixel rectangle.
void renderTriangle(float rx, float ry, float rw, float rh) {
    // Flush any pending UI batches before changing GL state.
    // (GLWidget::draw already ended the batch, but we guard here anyway.)

    const float rad = g_angle * (3.14159265f / 180.f);
    const float c = cosf(rad), s = sinf(rad);

    const float bx[3] = { 0.f,    0.779f, -0.779f };
    const float by[3] = { 0.9f,  -0.45f,  -0.45f  };
    const float col[3][4] = {
        {1.f, 0.25f, 0.25f, 1.f},
        {0.25f, 1.f, 0.25f, 1.f},
        {0.25f, 0.25f, 1.f, 1.f},
    };

    float v[3][6];
    for (int i = 0; i < 3; i++) {
        v[i][0] = c * bx[i] - s * by[i];
        v[i][1] = s * bx[i] + c * by[i];
        v[i][2] = col[i][0]; v[i][3] = col[i][1];
        v[i][4] = col[i][2]; v[i][5] = col[i][3];
    }

    // Scale so triangle is undistorted within this sub-rect.
    float sx = (rw <= rh) ? 1.f : rh / rw;
    float sy = (rh <= rw) ? 1.f : rw / rh;

    // Scissor to widget bounds (GL y is from bottom).
    glEnable(GL_SCISSOR_TEST);
    glScissor((GLint)rx,
              (GLint)(g_screenH - ry - rh),
              (GLsizei)rw, (GLsizei)rh);

    // Clear only this region.
    glClearColor(0.05f, 0.05f, 0.15f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport((GLint)rx, (GLint)(g_screenH - ry - rh),
               (GLsizei)rw, (GLsizei)rh);

    glUseProgram(g_triProg);
    glUniform2f(g_uScale, sx, sy);

    const int stride = 6 * sizeof(float);
    glVertexAttribPointer(g_aPos,   2, GL_FLOAT, GL_FALSE, stride, &v[0][0]);
    glEnableVertexAttribArray(g_aPos);
    glVertexAttribPointer(g_aColor, 4, GL_FLOAT, GL_FALSE, stride, &v[0][2]);
    glEnableVertexAttribArray(g_aColor);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisableVertexAttribArray(g_aPos);
    glDisableVertexAttribArray(g_aColor);

    // Restore full viewport and disable scissor for the UI pass.
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, g_screenW, g_screenH);
}

} // namespace

// ── JNI ───────────────────────────────────────────────────────────────────────

extern "C" {

JNIEXPORT void JNICALL
Java_com_example_openglndkdemo_GLRenderer_nativeInit(JNIEnv* env, jobject,
                                                     jobject jAssetMgr, jfloat density) {
    GLuint v = compileShader(GL_VERTEX_SHADER,   TRI_VERT);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, TRI_FRAG);
    g_triProg = glCreateProgram();
    glAttachShader(g_triProg, v);
    glAttachShader(g_triProg, f);
    glLinkProgram(g_triProg);
    glDeleteShader(v); glDeleteShader(f);

    g_aPos   = glGetAttribLocation (g_triProg, "aPos");
    g_aColor = glGetAttribLocation (g_triProg, "aColor");
    g_uScale = glGetUniformLocation(g_triProg, "uScale");

    AAssetManager* am = AAssetManager_fromJava(env, jAssetMgr);
    g_ui.init(am, density);

    buildDemoScene(g_ui, renderTriangle, density,
                   g_fpsLabel, g_angleLabel, g_statusLabel);

    glClearColor(0.08f, 0.09f, 0.12f, 1.f);
    LOGI("GL init OK");
}

JNIEXPORT void JNICALL
Java_com_example_openglndkdemo_GLRenderer_nativeResize(JNIEnv*, jobject, jint w, jint h) {
    g_screenW = w; g_screenH = h;
    glViewport(0, 0, w, h);
    if (w >= h) { g_scaleX = (float)h/w; g_scaleY = 1.f; }
    else        { g_scaleX = 1.f;        g_scaleY = (float)w/h; }
    g_ui.resize((float)w, (float)h);
    LOGI("Resize %dx%d", w, h);
}

JNIEXPORT void JNICALL
Java_com_example_openglndkdemo_GLRenderer_nativeDraw(JNIEnv*, jobject) {
    g_angle += g_speed;
    if (g_angle >= 360.f) g_angle -= 360.f;

    if (g_angleLabel) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Angle: %.1f", g_angle);
        g_angleLabel->text = buf;
    }

    // Clear full screen with UI background.
    glClearColor(0.08f, 0.09f, 0.12f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw UI (GLWidget callback is invoked mid-draw to render the triangle).
    g_ui.draw();
}

JNIEXPORT void JNICALL
Java_com_example_openglndkdemo_GLRenderer_nativeTouchDown(JNIEnv*, jobject, jint id, jfloat x, jfloat y) {
    g_ui.onTouchDown(id, x, y);
}
JNIEXPORT void JNICALL
Java_com_example_openglndkdemo_GLRenderer_nativeTouchMove(JNIEnv*, jobject, jint id, jfloat x, jfloat y) {
    g_ui.onTouchMove(id, x, y);
}
JNIEXPORT void JNICALL
Java_com_example_openglndkdemo_GLRenderer_nativeTouchUp(JNIEnv*, jobject, jint id, jfloat x, jfloat y) {
    g_ui.onTouchUp(id, x, y);
}
JNIEXPORT void JNICALL
Java_com_example_openglndkdemo_GLRenderer_nativeTouchCancel(JNIEnv*, jobject, jint id, jfloat x, jfloat y) {
    g_ui.onTouchCancel(id, x, y);
}
JNIEXPORT void JNICALL
Java_com_example_openglndkdemo_GLRenderer_nativeKey(JNIEnv*, jobject, jint keyCode, jint unicode, jboolean down) {
    g_ui.onKey(keyCode, unicode, down);
}
JNIEXPORT void JNICALL
Java_com_example_openglndkdemo_GLRenderer_nativeSetFps(JNIEnv* env, jobject, jstring fps) {
    if (!g_fpsLabel) return;
    const char* s = env->GetStringUTFChars(fps, nullptr);
    g_fpsLabel->text = s;
    env->ReleaseStringUTFChars(fps, s);
}

} // extern "C"
