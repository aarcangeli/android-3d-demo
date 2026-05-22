#pragma once
#include "camera.h"
#include <GLES2/gl2.h>
#include <string>

namespace ui { class UIRenderer; struct InputEvent; }

namespace scene {

class SceneRenderer {
public:
    Camera camera;
    Camera defaultCamera_;   // for resetView()
    float  autoRotation = 0.f;
    float  autoRotSpeed = 0.5f;  // degrees per frame; 0 = paused
    bool   showFps      = true;
    std::string fpsText;         // set externally by nativeSetFps

    void init(float screenW, float screenH);
    void shutdown();
    void resize(float screenW, float screenH);

    // Called from GLWidget::onRender.  rx,ry,rw,rh = viewport in screen pixels, y from top.
    void render(float rx, float ry, float rw, float rh, ui::UIRenderer& uiR);

    // Returns true if event consumed.
    bool onInput(const ui::InputEvent& e);

    void resetView();

private:
    float screenW_ = 0, screenH_ = 0;

    // Cube shader
    GLuint cubeProg_ = 0;
    GLint  cubeUMVP_ = -1, cubeUModel_ = -1;
    GLint  cubeAPos_ = -1, cubeANorm_ = -1, cubeACol_  = -1;

    // Grid shader (shared with cube frag, simple flat-color vert)
    GLuint gridProg_ = 0;
    GLint  gridUMVP_ = -1, gridUColor_ = -1, gridAPos_ = -1;

    // Geometry
    GLuint cubeVBO_ = 0, cubeIBO_ = 0;
    GLuint gridVBO_ = 0;
    int    gridNVerts_ = 0;

    // Touch state (up to 2 pointers)
    struct Ptr { int id=-1; float x=0,y=0; };
    Ptr    ptrs_[2];
    int    nPtrs_    = 0;
    float  prevDist_ = 0;
    float  prevMidX_ = 0, prevMidY_ = 0;
    // Stored camera state at gesture start
    float  orbitAz0_ = 0, orbitEl0_ = 0;
    float  orbitStartX_ = 0, orbitStartY_ = 0;

    static GLuint compileShader(GLenum type, const char* src);
    void buildShaders();
    void buildCubeGeometry();
    void buildGridGeometry();
};

} // namespace scene
