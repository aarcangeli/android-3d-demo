#pragma once
#include "camera.h"
#include <GLES2/gl2.h>
#include <string>
#include <cstdint>
#include <vector>

namespace ui { class UIRenderer; struct InputEvent; }

namespace scene {

enum class ObjType { CUBE, SPHERE, CYLINDER };

struct SceneObject {
    ObjType type    = ObjType::CUBE;
    float   px=0, py=0, pz=0;   // world position
    float   scale   = 1.f;
    bool    selected = false;
};

class SceneRenderer {
public:
    Camera camera;
    Camera defaultCamera_;
    float  autoRotation = 0.f;
    float  autoRotSpeed = 0.f;
    bool   showFps      = true;
    std::string fpsText;

    std::vector<SceneObject> objects;   // scene objects; index -1 = none selected
    int  selectedObj_  = -1;
    bool showHitMarker_ = false;

    // Pivot animation (double-tap)
    bool    pivotAnim_   = false;
    float   pivotSrcX_=0, pivotSrcY_=0, pivotSrcZ_=0;
    float   pivotDstX_=0, pivotDstY_=0, pivotDstZ_=0;
    int64_t pivotAnimT0_ = 0;
    static constexpr float kPivotAnimMs = 350.f;

    void init(float screenW, float screenH);
    void shutdown();
    void resize(float screenW, float screenH);
    void render(float rx, float ry, float rw, float rh, ui::UIRenderer& uiR);
    bool onInput(const ui::InputEvent& e);
    void resetView();

private:
    float screenW_ = 0, screenH_ = 0;

    // Lit object shader (cube + sphere + cylinder)
    GLuint objProg_    = 0;
    GLint  objUMVP_    = -1, objUModel_ = -1;
    GLint  objAPos_    = -1, objANorm_  = -1, objACol_ = -1;

    // Grid/flat-color shader
    GLuint gridProg_ = 0;
    GLint  gridUMVP_ = -1, gridUColor_ = -1, gridAPos_ = -1;

    // Geometry — indexed (VBO + IBO) for each object type
    struct Mesh { GLuint vbo=0, ibo=0; int nIdx=0; };
    Mesh cubeMesh_, sphereMesh_, cylinderMesh_;

    GLuint gridVBO_ = 0;
    int    gridNVerts_ = 0;
    GLuint axisVBO_ = 0;

    // Last viewport (set in render, used in onInput for ray casting)
    float vpX_ = 0, vpY_ = 0, vpW_ = 0, vpH_ = 0;

    // Touch state (up to 2 pointers)
    struct Ptr { int id=-1; float x=0,y=0; };
    Ptr   ptrs_[2];
    int   nPtrs_ = 0;

    // Orbit (single finger)
    float prevOrbitX_ = 0, prevOrbitY_ = 0;
    bool  blockOrbit_ = false;
    // Double-tap + drag-vertical zoom
    bool  dtZoom_    = false;
    float dtZoomY_   = 0;

    // Two-finger: exclusive zoom vs pan
    float prevDist_ = 0, prevMidX_ = 0, prevMidY_ = 0;
    enum class TwoFingerMode { UNDECIDED, ZOOM, PAN };
    TwoFingerMode twoFingerMode_ = TwoFingerMode::UNDECIDED;

    // Tap detection
    float   tapStartX_ = 0, tapStartY_ = 0;
    int64_t tapStartMs_ = 0;
    bool    tapMoved_   = false;
    int64_t lastTapMs_  = 0;
    float   lastTapX_   = 0, lastTapY_ = 0;

    static int64_t nowMs();
    void getRayFromTouch(float tx, float ty, float orig[3], float dir[3]) const;
    // Returns true + hitT for a single object; ray in world space, autoRotRad global spin
    static bool rayCastObject(const SceneObject& obj, const float orig[3], const float dir[3],
                              float autoRotRad, float& hitT);
    void handleTap(float tx, float ty, bool isDouble);

    static GLuint compileShader(GLenum type, const char* src);
    void buildShaders();
    static Mesh buildMeshFromVerts(const std::vector<float>& verts,
                                   const std::vector<uint16_t>& idx);
    void buildCubeMesh();
    void buildSphereMesh();
    void buildCylinderMesh();
    void buildGridGeometry();
    void buildAxisGeometry();
    void drawMesh(const Mesh& m) const;
};

} // namespace scene
