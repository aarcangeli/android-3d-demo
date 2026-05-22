#include "scene_renderer.h"
#include "ui/ui_renderer.h"
#include "ui/input.h"
#include "camera.h"
#include <GLES2/gl2.h>
#include <android/log.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <algorithm>
#include <time.h>

#define LOG_TAG "SceneRenderer"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace scene {

// ── Shader sources ────────────────────────────────────────────────────────────

static const char* CUBE_VERT = R"glsl(
attribute vec3 aPos;
attribute vec3 aNorm;
attribute vec4 aCol;
uniform mat4 uMVP;
uniform mat4 uModel;
varying vec4 vCol;
void main() {
    vec3 n = normalize(mat3(uModel) * aNorm);
    vec3 light = normalize(vec3(0.8, 1.5, 0.6));
    float d = max(0.25, dot(n, light));
    vCol = vec4(aCol.rgb * d, aCol.a);
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)glsl";

static const char* SHARED_FRAG = R"glsl(
precision mediump float;
varying vec4 vCol;
void main() { gl_FragColor = vCol; }
)glsl";

static const char* GRID_VERT = R"glsl(
attribute vec3 aPos;
uniform mat4 uMVP;
uniform vec4 uColor;
varying vec4 vCol;
void main() { vCol = uColor; gl_Position = uMVP * vec4(aPos, 1.0); }
)glsl";

// ── compileShader ─────────────────────────────────────────────────────────────

GLuint SceneRenderer::compileShader(GLenum type, const char* src) {
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

// ── buildShaders ──────────────────────────────────────────────────────────────

void SceneRenderer::buildShaders() {
    // Cube program
    {
        GLuint v = compileShader(GL_VERTEX_SHADER,   CUBE_VERT);
        GLuint f = compileShader(GL_FRAGMENT_SHADER, SHARED_FRAG);
        cubeProg_ = glCreateProgram();
        glAttachShader(cubeProg_, v);
        glAttachShader(cubeProg_, f);
        glLinkProgram(cubeProg_);
        glDeleteShader(v);
        glDeleteShader(f);

        GLint ok;
        glGetProgramiv(cubeProg_, GL_LINK_STATUS, &ok);
        if (!ok) {
            char buf[512];
            glGetProgramInfoLog(cubeProg_, sizeof(buf), nullptr, buf);
            LOGE("Cube program link error: %s", buf);
        }

        cubeUMVP_   = glGetUniformLocation(cubeProg_, "uMVP");
        cubeUModel_ = glGetUniformLocation(cubeProg_, "uModel");
        cubeAPos_   = glGetAttribLocation (cubeProg_, "aPos");
        cubeANorm_  = glGetAttribLocation (cubeProg_, "aNorm");
        cubeACol_   = glGetAttribLocation (cubeProg_, "aCol");
    }

    // Grid program
    {
        GLuint v = compileShader(GL_VERTEX_SHADER,   GRID_VERT);
        GLuint f = compileShader(GL_FRAGMENT_SHADER, SHARED_FRAG);
        gridProg_ = glCreateProgram();
        glAttachShader(gridProg_, v);
        glAttachShader(gridProg_, f);
        glLinkProgram(gridProg_);
        glDeleteShader(v);
        glDeleteShader(f);

        GLint ok;
        glGetProgramiv(gridProg_, GL_LINK_STATUS, &ok);
        if (!ok) {
            char buf[512];
            glGetProgramInfoLog(gridProg_, sizeof(buf), nullptr, buf);
            LOGE("Grid program link error: %s", buf);
        }

        gridUMVP_   = glGetUniformLocation(gridProg_, "uMVP");
        gridUColor_ = glGetUniformLocation(gridProg_, "uColor");
        gridAPos_   = glGetAttribLocation (gridProg_, "aPos");
    }
}

// ── buildCubeGeometry ─────────────────────────────────────────────────────────

struct V3D { float x,y,z, nx,ny,nz, r,g,b,a; };

void SceneRenderer::buildCubeGeometry() {
    // 24 vertices (4 per face), 36 indices (6 per face)
    // Vertices at ±0.8
    V3D verts[24];
    uint16_t idx[36];

    struct FaceDef {
        float nx, ny, nz;
        float r, g, b, a;
        float vx[4], vy[4], vz[4];
    };

    const FaceDef faces[6] = {
        // Front +Z
        { 0,0,1, 0.95f,0.35f,0.35f,1.f,
          {-0.8f,+0.8f,+0.8f,-0.8f}, {+0.8f,+0.8f,-0.8f,-0.8f}, {+0.8f,+0.8f,+0.8f,+0.8f} },
        // Back -Z
        { 0,0,-1, 0.35f,0.85f,0.85f,1.f,
          {+0.8f,-0.8f,-0.8f,+0.8f}, {+0.8f,+0.8f,-0.8f,-0.8f}, {-0.8f,-0.8f,-0.8f,-0.8f} },
        // Right +X
        { 1,0,0, 0.35f,0.85f,0.35f,1.f,
          {+0.8f,+0.8f,+0.8f,+0.8f}, {+0.8f,+0.8f,-0.8f,-0.8f}, {+0.8f,-0.8f,-0.8f,+0.8f} },
        // Left -X
        { -1,0,0, 0.75f,0.35f,0.95f,1.f,
          {-0.8f,-0.8f,-0.8f,-0.8f}, {+0.8f,+0.8f,-0.8f,-0.8f}, {-0.8f,+0.8f,+0.8f,-0.8f} },
        // Top +Y
        { 0,1,0, 0.35f,0.55f,0.95f,1.f,
          {-0.8f,+0.8f,+0.8f,-0.8f}, {+0.8f,+0.8f,+0.8f,+0.8f}, {-0.8f,-0.8f,+0.8f,+0.8f} },
        // Bottom -Y
        { 0,-1,0, 0.95f,0.75f,0.35f,1.f,
          {-0.8f,+0.8f,+0.8f,-0.8f}, {-0.8f,-0.8f,-0.8f,-0.8f}, {+0.8f,+0.8f,-0.8f,-0.8f} },
    };

    for (int f = 0; f < 6; ++f) {
        const FaceDef& fd = faces[f];
        for (int v = 0; v < 4; ++v) {
            int i = f * 4 + v;
            verts[i] = { fd.vx[v], fd.vy[v], fd.vz[v],
                         fd.nx, fd.ny, fd.nz,
                         fd.r,  fd.g,  fd.b,  fd.a };
        }
        int base = f * 4;
        int ib   = f * 6;
        idx[ib+0] = (uint16_t)(base+0);
        idx[ib+1] = (uint16_t)(base+2);
        idx[ib+2] = (uint16_t)(base+1);
        idx[ib+3] = (uint16_t)(base+0);
        idx[ib+4] = (uint16_t)(base+3);
        idx[ib+5] = (uint16_t)(base+2);
    }

    glGenBuffers(1, &cubeVBO_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &cubeIBO_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeIBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// ── buildGridGeometry ─────────────────────────────────────────────────────────

void SceneRenderer::buildGridGeometry() {
    std::vector<float> gv;
    gv.reserve(132);
    for (int i = -5; i <= 5; ++i) {
        gv.insert(gv.end(), {-5.f, 0.f, (float)i,   5.f, 0.f, (float)i});  // X-line
        gv.insert(gv.end(), {(float)i, 0.f, -5.f,   (float)i, 0.f, 5.f});  // Z-line
    }
    gridNVerts_ = (int)(gv.size() / 3);  // 44

    glGenBuffers(1, &gridVBO_);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(gv.size() * sizeof(float)), gv.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ── buildAxisGeometry ─────────────────────────────────────────────────────────

void SceneRenderer::buildAxisGeometry() {
    // 6 vertices: X/Y/Z positive axes, 2 units each, origin→tip
    const float v[] = {
        0.f, 0.f, 0.f,  2.f, 0.f, 0.f,   // X
        0.f, 0.f, 0.f,  0.f, 2.f, 0.f,   // Y
        0.f, 0.f, 0.f,  0.f, 0.f, 2.f,   // Z
    };
    glGenBuffers(1, &axisVBO_);
    glBindBuffer(GL_ARRAY_BUFFER, axisVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ── init / shutdown / resize ──────────────────────────────────────────────────

void SceneRenderer::init(float sw, float sh) {
    shutdown();
    screenW_ = sw;
    screenH_ = sh;
    buildShaders();
    buildCubeGeometry();
    buildGridGeometry();
    buildAxisGeometry();
    defaultCamera_ = camera;
    // Reset interaction state
    nPtrs_ = 0; ptrs_[0].id = ptrs_[1].id = -1;
    blockOrbit_ = false;
    cubeSelected_ = showHitMarker_ = false;
    tapMoved_ = false; tapStartMs_ = lastTapMs_ = 0;
}

void SceneRenderer::shutdown() {
    if (cubeProg_) { glDeleteProgram(cubeProg_); cubeProg_ = 0; }
    if (gridProg_) { glDeleteProgram(gridProg_); gridProg_ = 0; }
    if (cubeVBO_)  { glDeleteBuffers(1, &cubeVBO_); cubeVBO_ = 0; }
    if (cubeIBO_)  { glDeleteBuffers(1, &cubeIBO_); cubeIBO_ = 0; }
    if (gridVBO_)  { glDeleteBuffers(1, &gridVBO_); gridVBO_ = 0; }
    if (axisVBO_)  { glDeleteBuffers(1, &axisVBO_); axisVBO_ = 0; }
}

void SceneRenderer::resize(float sw, float sh) {
    screenW_ = sw;
    screenH_ = sh;
}

// ── resetView ─────────────────────────────────────────────────────────────────

void SceneRenderer::resetView() {
    camera       = defaultCamera_;
    autoRotation = 0.f;
}

// ── Ray casting / tap helpers ─────────────────────────────────────────────────

int64_t SceneRenderer::nowMs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

void SceneRenderer::getRayFromTouch(float tx, float ty,
                                    float orig[3], float dir[3]) const {
    float vm[16];
    camera.viewMatrix(vm);
    // Camera basis in world space (from view matrix columns)
    float rx=vm[0], ry_=vm[1], rz=vm[2];   // right
    float ux=vm[4], uy=vm[5], uz=vm[6];    // up
    float fx=-vm[8],fy=-vm[9],fz=-vm[10];  // forward

    float ndcX = (tx - vpX_) / vpW_ * 2.f - 1.f;
    float ndcY = 1.f - (ty - vpY_) / vpH_ * 2.f;  // Y flipped

    float tanH   = tanf(camera.fovY * (3.14159265f / 360.f));
    float aspect = vpW_ / (vpH_ > 0 ? vpH_ : 1.f);

    float ddx = ndcX * tanH * aspect;
    float ddy = ndcY * tanH;

    float ex, ey, ez;
    camera.eyePos(ex, ey, ez);
    orig[0] = ex; orig[1] = ey; orig[2] = ez;

    // dir = right*ddx + up*ddy + forward (then normalize)
    float wx = rx*ddx + ux*ddy + fx;
    float wy = ry_*ddx + uy*ddy + fy;
    float wz = rz*ddx + uz*ddy + fz;
    float wl = sqrtf(wx*wx + wy*wy + wz*wz);
    if (wl < 1e-7f) wl = 1e-7f;
    dir[0] = wx/wl; dir[1] = wy/wl; dir[2] = wz/wl;
}

bool SceneRenderer::rayCastCube(const float orig[3], const float dir[3],
                                 float autoRotRad, float& hitT) {
    // Transform ray to cube's object space (inverse Y rotation)
    float c = cosf(-autoRotRad), s = sinf(-autoRotRad);
    float ox = c*orig[0] + s*orig[2];
    float oy = orig[1];
    float oz = -s*orig[0] + c*orig[2];
    float dx = c*dir[0]  + s*dir[2];
    float dy = dir[1];
    float dz = -s*dir[0] + c*dir[2];

    // AABB slab test: cube is [-0.8, 0.8]^3 in object space
    float tmin = -1e30f, tmax = 1e30f;
    float os[3] = {ox,oy,oz}, ds[3] = {dx,dy,dz};
    for (int a = 0; a < 3; ++a) {
        if (fabsf(ds[a]) < 1e-7f) {
            if (os[a] < -0.8f || os[a] > 0.8f) return false;
        } else {
            float t1 = (-0.8f - os[a]) / ds[a];
            float t2 = ( 0.8f - os[a]) / ds[a];
            if (t1 > t2) { float tmp=t1; t1=t2; t2=tmp; }
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
    }
    if (tmax < 0.f) return false;
    hitT = tmin > 0.f ? tmin : tmax;
    return true;
}

void SceneRenderer::handleTap(float tx, float ty, bool isDouble) {
    if (vpW_ <= 0 || vpH_ <= 0) return;
    float orig[3], dir[3];
    getRayFromTouch(tx, ty, orig, dir);

    float autoRotRad = autoRotation * (3.14159265f / 180.f);
    float hitT;
    bool hit = rayCastCube(orig, dir, autoRotRad, hitT);

    if (isDouble) {
        if (hit) {
            // Set orbit pivot to the world-space hit point
            camera.targetX = orig[0] + dir[0] * hitT;
            camera.targetY = orig[1] + dir[1] * hitT;
            camera.targetZ = orig[2] + dir[2] * hitT;
        }
    } else {
        // Single tap: select/deselect
        if (hit) {
            cubeSelected_ = true;
            showHitMarker_ = true;
            hitMarkerPos_[0] = orig[0] + dir[0] * hitT;
            hitMarkerPos_[1] = orig[1] + dir[1] * hitT;
            hitMarkerPos_[2] = orig[2] + dir[2] * hitT;
        } else {
            cubeSelected_  = false;
            showHitMarker_ = false;
        }
    }
}

// ── render ────────────────────────────────────────────────────────────────────

void SceneRenderer::render(float rx, float ry, float rw, float rh, ui::UIRenderer& uiR) {
    // Save viewport for ray casting in onInput
    vpX_ = rx; vpY_ = ry; vpW_ = rw; vpH_ = rh;

    // -- Set up viewport clipped to the GLWidget rect --
    GLint sy = (GLint)(screenH_ - ry - rh);
    glEnable(GL_SCISSOR_TEST);
    glScissor((GLint)rx, sy, (GLsizei)rw, (GLsizei)rh);
    glClearColor(0.05f, 0.06f, 0.10f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport((GLint)rx, sy, (GLsizei)rw, (GLsizei)rh);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // -- Build matrices --
    float aspect = (rh > 0) ? rw / rh : 1.f;
    float proj[16], view[16], model[16], mvp[16], vm[16];
    camera.projMatrix(aspect, 0.1f, 100.f, proj);
    camera.viewMatrix(view);

    // Model: rotation around Y axis (auto-spin)
    const float rad = autoRotation * (3.14159265f / 180.f);
    const float c = cosf(rad), s = sinf(rad);
    mat4Identity(model);
    model[0] = c;  model[8]  = s;
    model[2] = -s; model[10] = c;

    // MVP = proj * view * model
    mat4Mul(view, model, vm);
    mat4Mul(proj, vm, mvp);
    float cubeMVP[16]; memcpy(cubeMVP, mvp, sizeof(mvp));  // save for wireframe

    // -- Draw cube --
    glUseProgram(cubeProg_);
    glUniformMatrix4fv(cubeUMVP_,   1, GL_FALSE, mvp);
    glUniformMatrix4fv(cubeUModel_, 1, GL_FALSE, model);

    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO_);
    const GLsizei stride = 10 * sizeof(float);
    glEnableVertexAttribArray(cubeAPos_);
    glVertexAttribPointer(cubeAPos_,  3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(cubeANorm_);
    glVertexAttribPointer(cubeANorm_, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(cubeACol_);
    glVertexAttribPointer(cubeACol_,  4, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeIBO_);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, nullptr);

    glDisableVertexAttribArray(cubeAPos_);
    glDisableVertexAttribArray(cubeANorm_);
    glDisableVertexAttribArray(cubeACol_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // -- Draw grid --
    // Grid MVP: no model transform (identity)
    mat4Mul(proj, view, mvp);  // = proj * view

    glUseProgram(gridProg_);
    glUniformMatrix4fv(gridUMVP_,  1, GL_FALSE, mvp);
    glUniform4f(gridUColor_, 0.28f, 0.32f, 0.42f, 1.f);  // muted blue-gray

    glBindBuffer(GL_ARRAY_BUFFER, gridVBO_);
    glEnableVertexAttribArray(gridAPos_);
    glVertexAttribPointer(gridAPos_, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glDrawArrays(GL_LINES, 0, gridNVerts_);

    // -- Draw RGB axes (positive only: X=red, Y=green, Z=blue) --
    glBindBuffer(GL_ARRAY_BUFFER, axisVBO_);
    glVertexAttribPointer(gridAPos_, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glUniform4f(gridUColor_, 1.00f, 0.22f, 0.22f, 1.f); glDrawArrays(GL_LINES, 0, 2);  // X red
    glUniform4f(gridUColor_, 0.22f, 1.00f, 0.22f, 1.f); glDrawArrays(GL_LINES, 2, 2);  // Y green
    glUniform4f(gridUColor_, 0.22f, 0.45f, 1.00f, 1.f); glDrawArrays(GL_LINES, 4, 2);  // Z blue

    glDisableVertexAttribArray(gridAPos_);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // -- Wireframe selection outline (model space, uses cube MVP) --
    if (cubeSelected_) {
        const float s = 0.8f;
        const float wv[] = {
            -s,-s,-s, s,-s,-s,   s,-s,-s, s,-s, s,   s,-s, s,-s,-s, s,  -s,-s, s,-s,-s,-s,
            -s, s,-s, s, s,-s,   s, s,-s, s, s, s,   s, s, s,-s, s, s,  -s, s, s,-s, s,-s,
            -s,-s,-s,-s, s,-s,   s,-s,-s, s, s,-s,   s,-s, s, s, s, s,  -s,-s, s,-s, s, s,
        };
        glUniformMatrix4fv(gridUMVP_, 1, GL_FALSE, cubeMVP);
        glUniform4f(gridUColor_, 0.05f, 0.85f, 1.0f, 1.f);
        glEnableVertexAttribArray(gridAPos_);
        glVertexAttribPointer(gridAPos_, 3, GL_FLOAT, GL_FALSE, 0, wv);
        glDrawArrays(GL_LINES, 0, 24);
        glDisableVertexAttribArray(gridAPos_);
    }

    // -- Hit marker (world space; mvp = proj*view at this point) --
    if (showHitMarker_) {
        glUniformMatrix4fv(gridUMVP_, 1, GL_FALSE, mvp);
        float mx = hitMarkerPos_[0], my = hitMarkerPos_[1], mz = hitMarkerPos_[2];
        const float sz = 0.07f;
        const float mv[] = {
            mx-sz, my,    mz,     mx+sz, my,    mz,
            mx,    my-sz, mz,     mx,    my+sz, mz,
            mx,    my,    mz-sz,  mx,    my,    mz+sz,
        };
        glUniform4f(gridUColor_, 1.f, 1.f, 0.1f, 1.f);
        glEnableVertexAttribArray(gridAPos_);
        glVertexAttribPointer(gridAPos_, 3, GL_FLOAT, GL_FALSE, 0, mv);
        glDrawArrays(GL_LINES, 0, 6);
        glDisableVertexAttribArray(gridAPos_);
    }

    // -- Restore state for UI --
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, (GLsizei)screenW_, (GLsizei)screenH_);

    // -- FPS overlay (using UIRenderer, drawn in screen-space) --
    if (showFps && !fpsText.empty()) {
        float fs = std::max(11.f, rh * 0.045f);
        fs = std::min(fs, 20.f);
        uiR.drawText(fpsText, rx + 8.f, ry + 6.f, fs,
                     ui::Color{1.f, 1.f, 0.3f, 0.85f});
    }
}

// ── onInput ───────────────────────────────────────────────────────────────────

bool SceneRenderer::onInput(const ui::InputEvent& e) {
    using ui::InputType;

    if (e.type == InputType::TOUCH_DOWN) {
        int slot = (ptrs_[0].id == -1) ? 0 : (ptrs_[1].id == -1 ? 1 : -1);
        if (slot < 0) return false;
        ptrs_[slot] = {e.pointerId, e.x, e.y};
        nPtrs_++;

        if (nPtrs_ == 1 && !blockOrbit_) {
            prevOrbitX_ = e.x;
            prevOrbitY_ = e.y;
            // Start tap tracking
            tapStartX_  = e.x;
            tapStartY_  = e.y;
            tapStartMs_ = nowMs();
            tapMoved_   = false;
        } else if (nPtrs_ == 2) {
            blockOrbit_ = true;
            tapMoved_   = true;  // invalidate any pending single-tap
            float dx  = ptrs_[1].x - ptrs_[0].x;
            float dy  = ptrs_[1].y - ptrs_[0].y;
            prevDist_  = sqrtf(dx*dx + dy*dy);
            prevMidX_  = (ptrs_[0].x + ptrs_[1].x) * 0.5f;
            prevMidY_  = (ptrs_[0].y + ptrs_[1].y) * 0.5f;
            twoFingerMode_ = TwoFingerMode::UNDECIDED;
        }
        return true;
    }

    if (e.type == InputType::TOUCH_MOVE) {
        for (int i = 0; i < 2; ++i)
            if (ptrs_[i].id == e.pointerId) { ptrs_[i].x = e.x; ptrs_[i].y = e.y; }

        if (nPtrs_ == 1 && !blockOrbit_) {
            float ddx = e.x - prevOrbitX_;
            float ddy = e.y - prevOrbitY_;
            camera.azimuth   += ddx * 0.25f;
            camera.elevation -= ddy * 0.25f;
            camera.clamp();
            prevOrbitX_ = e.x;
            prevOrbitY_ = e.y;
            // Track tap movement
            float md = sqrtf((e.x-tapStartX_)*(e.x-tapStartX_) + (e.y-tapStartY_)*(e.y-tapStartY_));
            if (md > 10.f) tapMoved_ = true;
        } else if (nPtrs_ == 2) {
            float dx   = ptrs_[1].x - ptrs_[0].x;
            float dy   = ptrs_[1].y - ptrs_[0].y;
            float dist = sqrtf(dx*dx + dy*dy);
            float midX = (ptrs_[0].x + ptrs_[1].x) * 0.5f;
            float midY = (ptrs_[0].y + ptrs_[1].y) * 0.5f;
            float ddx  = midX - prevMidX_;
            float ddy  = midY - prevMidY_;

            if (twoFingerMode_ == TwoFingerMode::UNDECIDED) {
                float pinch = std::abs(dist - prevDist_);
                float pan   = sqrtf(ddx*ddx + ddy*ddy);
                if      (pinch > 10.f) twoFingerMode_ = TwoFingerMode::ZOOM;
                else if (pan   > 10.f) twoFingerMode_ = TwoFingerMode::PAN;
            }

            if (twoFingerMode_ == TwoFingerMode::ZOOM) {
                if (dist > 1e-3f && prevDist_ > 1e-3f) {
                    float ratio = prevDist_ / dist;
                    // Dampen zoom to feel similar to pan
                    camera.distance *= 1.f + (ratio - 1.f) * 0.6f;
                }
                camera.clamp();
            } else if (twoFingerMode_ == TwoFingerMode::PAN) {
                // panScale: world units per pixel, calibrated to pivot distance
                float tanH     = tanf(camera.fovY * (3.14159265f / 360.f));
                float panScale = 2.f * camera.distance * tanH / (vpH_ > 0 ? vpH_ : 1.f);
                float r[3], u[3];
                camera.rightAndUp(r, u);
                camera.targetX += (-r[0]*ddx + u[0]*ddy) * panScale;
                camera.targetY += (-r[1]*ddx + u[1]*ddy) * panScale;
                camera.targetZ += (-r[2]*ddx + u[2]*ddy) * panScale;
            }

            prevDist_ = dist;
            prevMidX_ = midX;
            prevMidY_ = midY;
        }
        return true;
    }

    if (e.type == InputType::TOUCH_UP || e.type == InputType::TOUCH_CANCEL) {
        for (int i = 0; i < 2; ++i) {
            if (ptrs_[i].id != e.pointerId) continue;
            ptrs_[i].id = -1;
            nPtrs_--;
            if (nPtrs_ < 0) nPtrs_ = 0;

            if (nPtrs_ == 0) {
                // All fingers lifted
                blockOrbit_ = false;

                // Tap / double-tap detection (single-finger only, no drag)
                if (e.type == InputType::TOUCH_UP && !tapMoved_
                    && (nowMs() - tapStartMs_) < 300LL) {
                    int64_t now = nowMs();
                    float dx = e.x - lastTapX_, dy = e.y - lastTapY_;
                    bool isDouble = (lastTapMs_ > 0)
                        && (now - lastTapMs_) < 300LL
                        && sqrtf(dx*dx + dy*dy) < 40.f;
                    handleTap(e.x, e.y, isDouble);
                    if (isDouble) {
                        lastTapMs_ = 0;  // reset to prevent triple-tap chain
                    } else {
                        lastTapMs_ = now;
                        lastTapX_ = e.x;
                        lastTapY_ = e.y;
                    }
                }
            }
            break;
        }
        return true;
    }
    return false;
}

} // namespace scene
