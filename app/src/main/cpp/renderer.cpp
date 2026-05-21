#include <jni.h>
#include <GLES2/gl2.h>
#include <android/log.h>
#include <cmath>

#define LOG_TAG "OpenGLNDK"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

const char* VERT_SRC = R"glsl(
attribute vec2 aPos;
attribute vec4 aColor;
varying vec4 vColor;
void main() {
    vColor = aColor;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)glsl";

const char* FRAG_SRC = R"glsl(
precision mediump float;
varying vec4 vColor;
void main() {
    gl_FragColor = vColor;
}
)glsl";

GLuint g_program = 0;
GLint  g_aPos    = -1;
GLint  g_aColor  = -1;
float  g_angle   = 0.0f;

GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        LOGE("Shader compile error: %s", buf);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

} // namespace

extern "C" {

JNIEXPORT void JNICALL
Java_com_example_openglndkdemo_GLRenderer_nativeInit(JNIEnv*, jobject) {
    GLuint vert = compileShader(GL_VERTEX_SHADER,   VERT_SRC);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, FRAG_SRC);

    g_program = glCreateProgram();
    glAttachShader(g_program, vert);
    glAttachShader(g_program, frag);
    glLinkProgram(g_program);
    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint ok;
    glGetProgramiv(g_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetProgramInfoLog(g_program, sizeof(buf), nullptr, buf);
        LOGE("Program link error: %s", buf);
        return;
    }

    g_aPos   = glGetAttribLocation(g_program, "aPos");
    g_aColor = glGetAttribLocation(g_program, "aColor");
    glClearColor(0.05f, 0.05f, 0.15f, 1.0f);
    LOGI("GL init OK  program=%u aPos=%d aColor=%d", g_program, g_aPos, g_aColor);
}

JNIEXPORT void JNICALL
Java_com_example_openglndkdemo_GLRenderer_nativeResize(JNIEnv*, jobject, jint w, jint h) {
    glViewport(0, 0, w, h);
    LOGI("Viewport %dx%d", w, h);
}

JNIEXPORT void JNICALL
Java_com_example_openglndkdemo_GLRenderer_nativeDraw(JNIEnv*, jobject) {
    g_angle += 1.0f;
    if (g_angle >= 360.0f) g_angle -= 360.0f;

    const float rad = g_angle * (3.14159265f / 180.0f);
    const float c = cosf(rad);
    const float s = sinf(rad);

    // Equilateral triangle inscribed in a circle of radius ~0.9
    const float bx[3] = {  0.0f,   0.779f, -0.779f };
    const float by[3] = {  0.9f,  -0.45f,  -0.45f  };
    const float col[3][4] = {
        { 1.0f, 0.25f, 0.25f, 1.0f },   // red
        { 0.25f, 1.0f, 0.25f, 1.0f },   // green
        { 0.25f, 0.25f, 1.0f, 1.0f },   // blue
    };

    // Interleaved vertex buffer: x, y, r, g, b, a
    float v[3][6];
    for (int i = 0; i < 3; i++) {
        v[i][0] = c * bx[i] - s * by[i];
        v[i][1] = s * bx[i] + c * by[i];
        v[i][2] = col[i][0];
        v[i][3] = col[i][1];
        v[i][4] = col[i][2];
        v[i][5] = col[i][3];
    }

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(g_program);

    const int stride = 6 * sizeof(float);
    glVertexAttribPointer(g_aPos,   2, GL_FLOAT, GL_FALSE, stride, &v[0][0]);
    glEnableVertexAttribArray(g_aPos);
    glVertexAttribPointer(g_aColor, 4, GL_FLOAT, GL_FALSE, stride, &v[0][2]);
    glEnableVertexAttribArray(g_aColor);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisableVertexAttribArray(g_aPos);
    glDisableVertexAttribArray(g_aColor);
}

} // extern "C"
