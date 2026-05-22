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
#include "scene_renderer.h"

#define LOG_TAG "OpenGLNDK"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

int    g_screenW  = 0, g_screenH = 0;
float  g_density  = 1.f;
int    g_apiLevel = 21;
bool   g_needBuild = false;
bool   g_isCompact = false;

ui::UISystem  g_ui;
ui::Label*    g_fpsLabel    = nullptr;
ui::Label*    g_angleLabel  = nullptr;
ui::Label*    g_statusLabel = nullptr;

scene::SceneRenderer g_scene;
ui::GLWidget*        g_glWidget = nullptr;

static void wireGLWidget() {
    if (g_glWidget) {
        g_glWidget->onRender = [](float rx, float ry, float rw, float rh, ui::UIRenderer& uiR) {
            g_scene.render(rx, ry, rw, rh, uiR);
        };
        g_glWidget->inputHandler = [](const ui::InputEvent& e) -> bool {
            return g_scene.onInput(e);
        };
    }
}

static void rebuildUI(float screenW) {
    g_ui.root().clearChildren();
    g_fpsLabel = g_angleLabel = g_statusLabel = nullptr;
    g_glWidget = nullptr;
    buildDemoScene(g_ui, g_density, screenW, g_apiLevel,
                   g_scene, g_glWidget,
                   g_fpsLabel, g_angleLabel, g_statusLabel);
    wireGLWidget();
}

} // namespace

// ── JNI ───────────────────────────────────────────────────────────────────────

extern "C" {

JNIEXPORT void JNICALL
Java_com_example_openglndkdemo_GLRenderer_nativeInit(JNIEnv* env, jobject,
                                                     jobject jAssetMgr, jfloat density, jint apiLevel) {
    g_density  = density;
    g_apiLevel = (int)apiLevel;
    g_needBuild = true;   // defer UI build to nativeResize (when screen size is known)

    AAssetManager* am = AAssetManager_fromJava(env, jAssetMgr);
    g_ui.init(am, density);
    g_scene.init((float)g_screenW, (float)g_screenH);
    glClearColor(0.08f, 0.09f, 0.12f, 1.f);
    LOGI("GL init OK");
}

JNIEXPORT void JNICALL
Java_com_example_openglndkdemo_GLRenderer_nativeResize(JNIEnv*, jobject, jint w, jint h) {
    g_screenW = w; g_screenH = h;
    glViewport(0, 0, w, h);
    g_scene.resize((float)w, (float)h);

    bool newCompact = (g_density > 0.f) ? ((float)w / g_density < 480.f) : false;
    if (g_needBuild || newCompact != g_isCompact) {
        g_isCompact = newCompact;
        g_needBuild = false;
        rebuildUI((float)w);
    }
    g_ui.resize((float)w, (float)h);  // always last: applies layout to the current tree
    LOGI("Resize %dx%d", w, h);
}

JNIEXPORT void JNICALL
Java_com_example_openglndkdemo_GLRenderer_nativeDraw(JNIEnv*, jobject) {
    g_scene.autoRotation += g_scene.autoRotSpeed;
    if (g_scene.autoRotation >= 360.f) g_scene.autoRotation -= 360.f;

    if (g_angleLabel) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Angle: %.1f", g_scene.autoRotation);
        g_angleLabel->text = buf;
    }
    glClearColor(0.08f, 0.09f, 0.12f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
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
    const char* s = env->GetStringUTFChars(fps, nullptr);
    g_scene.fpsText = s;
    if (g_fpsLabel) g_fpsLabel->text = s;
    env->ReleaseStringUTFChars(fps, s);
}

} // extern "C"
