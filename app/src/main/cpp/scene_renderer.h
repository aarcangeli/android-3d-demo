#pragma once
#include "camera.h"
#include <GLES2/gl2.h>
#include <string>
#include <cstdint>

namespace ui { class UIRenderer; struct InputEvent; }

namespace scene {

class SceneRenderer {
public:
    Camera camera;
    Camera defaultCamera_;
    float  autoRotation = 0.f;
    float  autoRotSpeed = 0.f;

    // Selection / pivot marker state
    bool  cubeSelected_  = false;
    bool  showHitMarker_ = false;

    void init(float screenW, float screenH);
    void shutdown();
    void resize(float screenW, float screenH);
    void render(float rx, float ry, float rw, float rh, ui::UIRenderer& uiR);
    bool onInput(const ui::InputEvent& e);
    void resetView();

private:
    float screenW_ = 0, screenH_ = 0;

    // Cube shader
    GLuint cubeProg_ = 0;
    GLint  cubeUMVP_ = -1, cubeUModel_ = -1;
    GLint  cubeAPos_ = -1, cubeANorm_ = -1, cubeACol_ = -1;

    // Grid/flat-color shader
    GLuint gridProg_ = 0;
    GLint  gridUMVP_ = -1, gridUColor_ = -1, gridAPos_ = -1;

    // Geometry
    GLuint cubeVBO_ = 0, cubeIBO_ = 0;
    GLuint gridVBO_ = 0;
    int    gridNVerts_ = 0;
    GLuint axisVBO_ = 0;

    // Last viewport (set in render, used in onInput for ray casting)
    float vpX_ = 0, vpY_ = 0, vpW_ = 0, vpH_ = 0;

    // Touch state (up to 2 pointers)
    struct Ptr { int id=-1; float x=0,y=0; };
    Ptr   ptrs_[2];
    int   nPtrs_ = 0;

    // Orbit (single finger): per-frame delta
    float prevOrbitX_ = 0, prevOrbitY_ = 0;
    bool  blockOrbit_ = false;   // true while any 2-finger gesture is/was active

    // Two-finger: exclusive zoom vs pan
    float prevDist_ = 0, prevMidX_ = 0, prevMidY_ = 0;
    enum class TwoFingerMode { UNDECIDED, ZOOM, PAN };
    TwoFingerMode twoFingerMode_ = TwoFingerMode::UNDECIDED;

    // Tap detection
    float   tapStartX_ = 0, tapStartY_ = 0;
    int64_t tapStartMs_ = 0;
    bool    tapMoved_   = false;   // also set true when a 2nd finger arrives
    int64_t lastTapMs_  = 0;
    float   lastTapX_   = 0, lastTapY_ = 0;

    static int64_t nowMs();
    void getRayFromTouch(float tx, float ty, float orig[3], float dir[3]) const;
    // Returns true and sets hitT if ray hits the (rotated) cube AABB
    static bool rayCastCube(const float orig[3], const float dir[3],
                            float autoRotRad, float& hitT);
    void handleTap(float tx, float ty, bool isDouble);

    static GLuint compileShader(GLenum type, const char* src);
    void buildShaders();
    void buildCubeGeometry();
    void buildGridGeometry();
    void buildAxisGeometry();
};

} // namespace scene
