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

static const char* OBJ_VERT = R"glsl(
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
    // Lit object program (cube / sphere / cylinder)
    {
        GLuint v = compileShader(GL_VERTEX_SHADER,   OBJ_VERT);
        GLuint f = compileShader(GL_FRAGMENT_SHADER, SHARED_FRAG);
        objProg_ = glCreateProgram();
        glAttachShader(objProg_, v);
        glAttachShader(objProg_, f);
        glLinkProgram(objProg_);
        glDeleteShader(v); glDeleteShader(f);
        GLint ok; glGetProgramiv(objProg_, GL_LINK_STATUS, &ok);
        if (!ok) { char b[512]; glGetProgramInfoLog(objProg_,sizeof(b),nullptr,b); LOGE("ObjProg: %s",b); }
        objUMVP_  = glGetUniformLocation(objProg_, "uMVP");
        objUModel_= glGetUniformLocation(objProg_, "uModel");
        objAPos_  = glGetAttribLocation (objProg_, "aPos");
        objANorm_ = glGetAttribLocation (objProg_, "aNorm");
        objACol_  = glGetAttribLocation (objProg_, "aCol");
    }
    // Grid program
    {
        GLuint v = compileShader(GL_VERTEX_SHADER,   GRID_VERT);
        GLuint f = compileShader(GL_FRAGMENT_SHADER, SHARED_FRAG);
        gridProg_ = glCreateProgram();
        glAttachShader(gridProg_, v); glAttachShader(gridProg_, f);
        glLinkProgram(gridProg_);
        glDeleteShader(v); glDeleteShader(f);
        GLint ok; glGetProgramiv(gridProg_, GL_LINK_STATUS, &ok);
        if (!ok) { char b[512]; glGetProgramInfoLog(gridProg_,sizeof(b),nullptr,b); LOGE("GridProg: %s",b); }
        gridUMVP_  = glGetUniformLocation(gridProg_, "uMVP");
        gridUColor_= glGetUniformLocation(gridProg_, "uColor");
        gridAPos_  = glGetAttribLocation (gridProg_, "aPos");
    }
}

// ── Mesh helpers ──────────────────────────────────────────────────────────────

// Vertex layout: pos(3) + norm(3) + color(4) = 10 floats, stride = 40 bytes
static void pushV(std::vector<float>& v,
                  float px,float py,float pz,
                  float nx,float ny,float nz,
                  float r,float g,float b,float a=1.f) {
    v.insert(v.end(), {px,py,pz, nx,ny,nz, r,g,b,a});
}

SceneRenderer::Mesh SceneRenderer::buildMeshFromVerts(const std::vector<float>& verts,
                                                       const std::vector<uint16_t>& idx) {
    Mesh m;
    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size()*sizeof(float)), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m.nIdx = (int)idx.size();
    glGenBuffers(1, &m.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(idx.size()*sizeof(uint16_t)), idx.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    return m;
}

// ── buildCubeMesh ─────────────────────────────────────────────────────────────

void SceneRenderer::buildCubeMesh() {
    struct FaceDef { float nx,ny,nz,r,g,b; float vx[4],vy[4],vz[4]; };
    const float S = 0.8f;
    const FaceDef faces[6] = {
        { 0,0,1, 0.95f,0.35f,0.35f, {-S,+S,+S,-S},{+S,+S,-S,-S},{+S,+S,+S,+S} },
        { 0,0,-1,0.35f,0.85f,0.85f, {+S,-S,-S,+S},{+S,+S,-S,-S},{-S,-S,-S,-S} },
        { 1,0,0, 0.35f,0.85f,0.35f, {+S,+S,+S,+S},{+S,+S,-S,-S},{+S,-S,-S,+S} },
        {-1,0,0, 0.75f,0.35f,0.95f, {-S,-S,-S,-S},{+S,+S,-S,-S},{-S,+S,+S,-S} },
        { 0,1,0, 0.35f,0.55f,0.95f, {-S,+S,+S,-S},{+S,+S,+S,+S},{-S,-S,+S,+S} },
        { 0,-1,0,0.95f,0.75f,0.35f, {-S,+S,+S,-S},{-S,-S,-S,-S},{+S,+S,-S,-S} },
    };
    std::vector<float> v; std::vector<uint16_t> idx;
    for (int f = 0; f < 6; ++f) {
        const auto& fd = faces[f];
        uint16_t base = (uint16_t)(v.size()/10);
        for (int i = 0; i < 4; ++i)
            pushV(v, fd.vx[i],fd.vy[i],fd.vz[i], fd.nx,fd.ny,fd.nz, fd.r,fd.g,fd.b);
        idx.insert(idx.end(), {(uint16_t)(base),(uint16_t)(base+2),(uint16_t)(base+1),
                                (uint16_t)(base),(uint16_t)(base+3),(uint16_t)(base+2)});
    }
    cubeMesh_ = buildMeshFromVerts(v, idx);
}

// ── buildSphereMesh ───────────────────────────────────────────────────────────

void SceneRenderer::buildSphereMesh() {
    const int stacks = 14, slices = 20;
    const float R = 0.8f;
    std::vector<float> v; std::vector<uint16_t> idx;
    for (int st = 0; st <= stacks; ++st) {
        float phi  = (float)M_PI * st / stacks;  // 0 .. PI
        float y    = R * cosf(phi);
        float sinP = sinf(phi);
        for (int sl = 0; sl <= slices; ++sl) {
            float theta = 2.f*(float)M_PI * sl / slices;
            float nx = sinP*cosf(theta), ny = cosf(phi), nz = sinP*sinf(theta);
            // warm gradient: top=orange, bottom=purple
            float t = (float)st / stacks;
            float r = 1.0f - t*0.3f, g = 0.55f + t*0.1f, b = 0.15f + t*0.7f;
            pushV(v, R*nx, y, R*nz, nx, ny, nz, r, g, b);
        }
    }
    for (int st = 0; st < stacks; ++st)
        for (int sl = 0; sl < slices; ++sl) {
            uint16_t a=(uint16_t)(st*(slices+1)+sl), b=(uint16_t)(a+1),
                     c=(uint16_t)(a+(slices+1)),      d=(uint16_t)(c+1);
            idx.insert(idx.end(), {a,b,c, b,d,c});
        }
    sphereMesh_ = buildMeshFromVerts(v, idx);
}

// ── buildCylinderMesh ─────────────────────────────────────────────────────────

void SceneRenderer::buildCylinderMesh() {
    const int segs = 24;
    const float R = 0.6f, H = 0.8f;   // radius, half-height
    std::vector<float> v; std::vector<uint16_t> idx;

    // Side
    for (int i = 0; i <= segs; ++i) {
        float a = 2.f*(float)M_PI*i/segs;
        float nx = cosf(a), nz = sinf(a);
        // teal top, green bottom
        pushV(v, R*nx, +H, R*nz,  nx,0,nz, 0.25f,0.85f,0.70f);
        pushV(v, R*nx, -H, R*nz,  nx,0,nz, 0.25f,0.65f,0.35f);
    }
    for (int i = 0; i < segs; ++i) {
        uint16_t a=(uint16_t)(2*i), b=(uint16_t)(2*i+1),
                 c=(uint16_t)(2*(i+1)), d=(uint16_t)(2*(i+1)+1);
        idx.insert(idx.end(), {a,c,b, c,d,b});
    }

    // Top cap
    uint16_t topCenter = (uint16_t)(v.size()/10);
    pushV(v, 0,+H,0, 0,1,0, 0.25f,0.90f,0.75f);
    uint16_t topRingStart = (uint16_t)(v.size()/10);
    for (int i = 0; i <= segs; ++i) {
        float a = 2.f*(float)M_PI*i/segs;
        pushV(v, R*cosf(a),+H,R*sinf(a), 0,1,0, 0.25f,0.90f,0.75f);
    }
    for (int i = 0; i < segs; ++i)
        idx.insert(idx.end(), {topCenter,(uint16_t)(topRingStart+i),(uint16_t)(topRingStart+i+1)});

    // Bottom cap
    uint16_t botCenter = (uint16_t)(v.size()/10);
    pushV(v, 0,-H,0, 0,-1,0, 0.25f,0.60f,0.30f);
    uint16_t botRingStart = (uint16_t)(v.size()/10);
    for (int i = 0; i <= segs; ++i) {
        float a = 2.f*(float)M_PI*i/segs;
        pushV(v, R*cosf(a),-H,R*sinf(a), 0,-1,0, 0.25f,0.60f,0.30f);
    }
    for (int i = 0; i < segs; ++i)
        idx.insert(idx.end(), {botCenter,(uint16_t)(botRingStart+i+1),(uint16_t)(botRingStart+i)});

    cylinderMesh_ = buildMeshFromVerts(v, idx);
}

// ── buildGridGeometry ─────────────────────────────────────────────────────────

void SceneRenderer::buildGridGeometry() {
    std::vector<float> gv;
    gv.reserve(132);
    for (int i = -5; i <= 5; ++i) {
        gv.insert(gv.end(), {-5.f,0.f,(float)i,  5.f,0.f,(float)i});
        gv.insert(gv.end(), {(float)i,0.f,-5.f,  (float)i,0.f,5.f});
    }
    gridNVerts_ = (int)(gv.size()/3);
    glGenBuffers(1, &gridVBO_);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(gv.size()*sizeof(float)), gv.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ── buildAxisGeometry ─────────────────────────────────────────────────────────

void SceneRenderer::buildAxisGeometry() {
    const float v[] = {
        0,0,0, 2,0,0,   0,0,0, 0,2,0,   0,0,0, 0,0,2,
    };
    glGenBuffers(1, &axisVBO_);
    glBindBuffer(GL_ARRAY_BUFFER, axisVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ── init / shutdown / resize ──────────────────────────────────────────────────

void SceneRenderer::init(float sw, float sh) {
    shutdown();
    screenW_ = sw; screenH_ = sh;
    buildShaders();
    buildCubeMesh();
    buildSphereMesh();
    buildCylinderMesh();
    buildGridGeometry();
    buildAxisGeometry();

    // Default scene objects
    objects.clear();
    objects.push_back({ObjType::CUBE,     -2.5f, 0.f, 0.f, 1.f, false});
    objects.push_back({ObjType::SPHERE,    0.0f, 0.f, 0.f, 1.f, false});
    objects.push_back({ObjType::CYLINDER,  2.5f, 0.f, 0.f, 1.f, false});

    defaultCamera_ = camera;
    nPtrs_ = 0; ptrs_[0].id = ptrs_[1].id = -1;
    blockOrbit_ = false;
    selectedObj_ = -1; showHitMarker_ = false;
    tapMoved_ = false; tapStartMs_ = lastTapMs_ = 0;
}

void SceneRenderer::shutdown() {
    if (objProg_)  { glDeleteProgram(objProg_);  objProg_  = 0; }
    if (gridProg_) { glDeleteProgram(gridProg_); gridProg_ = 0; }
    auto delMesh = [](Mesh& m){ if(m.vbo){glDeleteBuffers(1,&m.vbo);m.vbo=0;}
                                if(m.ibo){glDeleteBuffers(1,&m.ibo);m.ibo=0;} };
    delMesh(cubeMesh_); delMesh(sphereMesh_); delMesh(cylinderMesh_);
    if (gridVBO_) { glDeleteBuffers(1,&gridVBO_); gridVBO_=0; }
    if (axisVBO_) { glDeleteBuffers(1,&axisVBO_); axisVBO_=0; }
}

void SceneRenderer::resize(float sw, float sh) { screenW_=sw; screenH_=sh; }

// ── resetView ─────────────────────────────────────────────────────────────────

void SceneRenderer::resetView() {
    camera        = defaultCamera_;
    autoRotation  = 0.f;
    showHitMarker_= false;
    selectedObj_  = -1;
    for (auto& o : objects) o.selected = false;
    pivotAnim_ = false;
}

// ── Ray casting ───────────────────────────────────────────────────────────────

int64_t SceneRenderer::nowMs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec*1000LL + ts.tv_nsec/1000000LL;
}

void SceneRenderer::getRayFromTouch(float tx, float ty,
                                    float orig[3], float dir[3]) const {
    float vm[16];
    camera.viewMatrix(vm);
    float rx=vm[0],  ry_=vm[4], rz=vm[8];
    float ux=vm[1],  uy=vm[5],  uz=vm[9];
    float fx=-vm[2], fy=-vm[6], fz=-vm[10];

    float ndcX = (tx - vpX_) / vpW_ * 2.f - 1.f;
    float ndcY = 1.f - (ty - vpY_) / vpH_ * 2.f;

    float tanH   = tanf(camera.fovY * (3.14159265f / 360.f));
    float aspect = vpW_ / (vpH_ > 0 ? vpH_ : 1.f);
    float ddx = ndcX * tanH * aspect;
    float ddy = ndcY * tanH;

    float ex, ey, ez;
    camera.eyePos(ex, ey, ez);
    orig[0]=ex; orig[1]=ey; orig[2]=ez;

    float wx=rx*ddx+ux*ddy+fx, wy=ry_*ddx+uy*ddy+fy, wz=rz*ddx+uz*ddy+fz;
    float wl=sqrtf(wx*wx+wy*wy+wz*wz); if(wl<1e-7f)wl=1e-7f;
    dir[0]=wx/wl; dir[1]=wy/wl; dir[2]=wz/wl;
}

// Transform ray to object local space (object spins around its own center).
// o_local and d_local are not normalised — hitT stays in world units.
static void rayToLocal(const SceneObject& obj, float autoRotRad,
                       const float orig[3], const float dir[3],
                       float o[3], float d[3]) {
    // Translate
    float tx = orig[0]-obj.px, ty = orig[1]-obj.py, tz = orig[2]-obj.pz;
    float dx = dir[0], dy = dir[1], dz = dir[2];
    // Inverse Y-rotation
    float c = cosf(-autoRotRad), s = sinf(-autoRotRad);
    float ox2 = c*tx + s*tz; float oz2 = -s*tx + c*tz;
    float dx2 = c*dx + s*dz; float dz2 = -s*dx + c*dz;
    // Inverse scale
    float inv = 1.f / (obj.scale > 1e-7f ? obj.scale : 1e-7f);
    o[0]=ox2*inv; o[1]=ty*inv; o[2]=oz2*inv;
    d[0]=dx2*inv; d[1]=dy*inv; d[2]=dz2*inv;
}

bool SceneRenderer::rayCastObject(const SceneObject& obj,
                                   const float orig[3], const float dir[3],
                                   float autoRotRad, float& hitT) {
    float o[3], d[3];
    rayToLocal(obj, autoRotRad, orig, dir, o, d);

    if (obj.type == ObjType::CUBE) {
        // AABB slab test [-0.8, 0.8]^3
        float tmin=-1e30f, tmax=1e30f;
        for (int a=0;a<3;++a){
            if (fabsf(d[a])<1e-7f){ if(o[a]<-0.8f||o[a]>0.8f)return false; }
            else {
                float t1=(-0.8f-o[a])/d[a], t2=(0.8f-o[a])/d[a];
                if(t1>t2){float tmp=t1;t1=t2;t2=tmp;}
                tmin=std::max(tmin,t1); tmax=std::min(tmax,t2);
                if(tmin>tmax)return false;
            }
        }
        if(tmax<0.f)return false;
        hitT=tmin>0.f?tmin:tmax; return true;
    }

    if (obj.type == ObjType::SPHERE) {
        // Ray-sphere, radius 0.8
        const float R=0.8f;
        float a2=d[0]*d[0]+d[1]*d[1]+d[2]*d[2];
        float b2=2.f*(o[0]*d[0]+o[1]*d[1]+o[2]*d[2]);
        float cc=o[0]*o[0]+o[1]*o[1]+o[2]*o[2]-R*R;
        float disc=b2*b2-4.f*a2*cc;
        if(disc<0.f)return false;
        float sq=sqrtf(disc);
        float t0=(-b2-sq)/(2.f*a2), t1=(-b2+sq)/(2.f*a2);
        if(t1<0.f)return false;
        hitT=t0>0.f?t0:t1; return true;
    }

    if (obj.type == ObjType::CYLINDER) {
        // Cylinder along Y, radius 0.6, half-height 0.8
        const float R=0.6f, H=0.8f;
        float bestT=1e30f; bool hit=false;

        // Side: (ox+t*dx)^2 + (oz+t*dz)^2 = R^2
        float a2=d[0]*d[0]+d[2]*d[2];
        if(fabsf(a2)>1e-7f){
            float b2=2.f*(o[0]*d[0]+o[2]*d[2]);
            float cc=o[0]*o[0]+o[2]*o[2]-R*R;
            float disc=b2*b2-4.f*a2*cc;
            if(disc>=0.f){
                float sq=sqrtf(disc);
                for(float t:{(-b2-sq)/(2.f*a2),(-b2+sq)/(2.f*a2)}){
                    float y=o[1]+t*d[1];
                    if(t>0.f&&fabsf(y)<=H&&t<bestT){bestT=t;hit=true;}
                }
            }
        }
        // Caps
        if(fabsf(d[1])>1e-7f){
            for(float capY:{-H,+H}){
                float t=(capY-o[1])/d[1];
                if(t>0.f){
                    float cx=o[0]+t*d[0], cz=o[2]+t*d[2];
                    if(cx*cx+cz*cz<=R*R&&t<bestT){bestT=t;hit=true;}
                }
            }
        }
        if(!hit)return false;
        hitT=bestT; return true;
    }
    return false;
}

// ── handleTap ─────────────────────────────────────────────────────────────────

void SceneRenderer::handleTap(float tx, float ty, bool isDouble) {
    if (vpW_<=0||vpH_<=0) return;
    float orig[3], dir[3];
    getRayFromTouch(tx, ty, orig, dir);

    float autoRotRad = autoRotation * (3.14159265f / 180.f);

    // Find closest hit among all objects
    int   bestObj=-1;
    float bestT=1e30f;
    for (int i=0;i<(int)objects.size();++i){
        float t;
        if(rayCastObject(objects[i], orig, dir, autoRotRad, t) && t<bestT){
            bestT=t; bestObj=i;
        }
    }

    if (isDouble) {
        if (bestObj>=0){
            pivotSrcX_=camera.targetX; pivotSrcY_=camera.targetY; pivotSrcZ_=camera.targetZ;
            pivotDstX_=orig[0]+dir[0]*bestT;
            pivotDstY_=orig[1]+dir[1]*bestT;
            pivotDstZ_=orig[2]+dir[2]*bestT;
            pivotAnim_=true; pivotAnimT0_=nowMs();
            showHitMarker_=true;
        }
    } else {
        // Deselect all, then select hit object
        for (auto& o : objects) o.selected=false;
        selectedObj_=bestObj;
        if (bestObj>=0) objects[bestObj].selected=true;
    }
}

// ── drawMesh ──────────────────────────────────────────────────────────────────

void SceneRenderer::drawMesh(const Mesh& m) const {
    if (!m.vbo || !m.ibo) return;
    const GLsizei stride = 10 * sizeof(float);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glEnableVertexAttribArray(objAPos_);
    glVertexAttribPointer(objAPos_,  3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(objANorm_);
    glVertexAttribPointer(objANorm_, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(objACol_);
    glVertexAttribPointer(objACol_,  4, GL_FLOAT, GL_FALSE, stride, (void*)(6*sizeof(float)));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
    glDrawElements(GL_TRIANGLES, m.nIdx, GL_UNSIGNED_SHORT, nullptr);
    glDisableVertexAttribArray(objAPos_);
    glDisableVertexAttribArray(objANorm_);
    glDisableVertexAttribArray(objACol_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ── render ────────────────────────────────────────────────────────────────────

void SceneRenderer::render(float rx, float ry, float rw, float rh, ui::UIRenderer& uiR) {
    // Pivot animation
    if (pivotAnim_) {
        float t = (nowMs() - pivotAnimT0_) / kPivotAnimMs;
        if (t >= 1.f) { t=1.f; pivotAnim_=false; }
        else t = t*t*(3.f-2.f*t);
        camera.targetX = pivotSrcX_+(pivotDstX_-pivotSrcX_)*t;
        camera.targetY = pivotSrcY_+(pivotDstY_-pivotSrcY_)*t;
        camera.targetZ = pivotSrcZ_+(pivotDstZ_-pivotSrcZ_)*t;
    }

    vpX_=rx; vpY_=ry; vpW_=rw; vpH_=rh;

    GLint sy=(GLint)(screenH_-ry-rh);
    glEnable(GL_SCISSOR_TEST);
    glScissor((GLint)rx,sy,(GLsizei)rw,(GLsizei)rh);
    glClearColor(0.05f,0.06f,0.10f,1.f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glViewport((GLint)rx,sy,(GLsizei)rw,(GLsizei)rh);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);  glCullFace(GL_BACK); glFrontFace(GL_CCW);

    float aspect = (rh>0)?rw/rh:1.f;
    float proj[16], view[16], mvp[16], vm[16];
    float nearP = std::max(0.001f, camera.distance * 0.005f);
    camera.projMatrix(aspect, nearP, 500.f, proj);
    camera.viewMatrix(view);

    const float rad = autoRotation * (3.14159265f / 180.f);
    const float cR  = cosf(rad), sR = sinf(rad);

    // Proj*view (used for grid/axis/marker)
    mat4Mul(proj, view, mvp);

    // -- Draw grid --
    glUseProgram(gridProg_);
    glUniformMatrix4fv(gridUMVP_,1,GL_FALSE,mvp);
    glUniform4f(gridUColor_,0.28f,0.32f,0.42f,1.f);
    glBindBuffer(GL_ARRAY_BUFFER,gridVBO_);
    glEnableVertexAttribArray(gridAPos_);
    glVertexAttribPointer(gridAPos_,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glDrawArrays(GL_LINES,0,gridNVerts_);

    // -- Draw RGB axes --
    glBindBuffer(GL_ARRAY_BUFFER,axisVBO_);
    glVertexAttribPointer(gridAPos_,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glUniform4f(gridUColor_,1.f,0.22f,0.22f,1.f); glDrawArrays(GL_LINES,0,2);
    glUniform4f(gridUColor_,0.22f,1.f,0.22f,1.f); glDrawArrays(GL_LINES,2,2);
    glUniform4f(gridUColor_,0.22f,0.45f,1.f,1.f); glDrawArrays(GL_LINES,4,2);
    glDisableVertexAttribArray(gridAPos_);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    // -- Draw objects --
    glUseProgram(objProg_);
    for (int i=0;i<(int)objects.size();++i) {
        const SceneObject& obj = objects[i];

        // Model = Translate(pos) * RotateY(autoRotation) * Scale(scale)
        float model[16]; mat4Identity(model);
        model[0]=cR*obj.scale; model[8]=sR*obj.scale;
        model[2]=-sR*obj.scale; model[10]=cR*obj.scale;
        model[5]=obj.scale;
        model[12]=obj.px; model[13]=obj.py; model[14]=obj.pz;

        float objMVP[16];
        mat4Mul(view,model,vm);
        mat4Mul(proj,vm,objMVP);

        glUniformMatrix4fv(objUMVP_,  1,GL_FALSE,objMVP);
        glUniformMatrix4fv(objUModel_,1,GL_FALSE,model);

        switch(obj.type){
            case ObjType::CUBE:     drawMesh(cubeMesh_);     break;
            case ObjType::SPHERE:   drawMesh(sphereMesh_);   break;
            case ObjType::CYLINDER: drawMesh(cylinderMesh_); break;
        }

        // Selection wireframe — drawn slightly outside the mesh, GL_LEQUAL to avoid z-fight
        if (obj.selected) {
            glUseProgram(gridProg_);
            glDepthFunc(GL_LEQUAL);
            glUniformMatrix4fv(gridUMVP_,1,GL_FALSE,objMVP);
            glUniform4f(gridUColor_,0.05f,0.85f,1.f,1.f);
            glEnableVertexAttribArray(gridAPos_);

            if (obj.type == ObjType::CUBE) {
                const float S=0.83f;  // slightly outside 0.80 mesh
                const float wv[]={
                    -S,-S,-S, S,-S,-S,  S,-S,-S, S,-S, S,  S,-S, S,-S,-S, S,  -S,-S, S,-S,-S,-S,
                    -S, S,-S, S, S,-S,  S, S,-S, S, S, S,  S, S, S,-S, S, S,  -S, S, S,-S, S,-S,
                    -S,-S,-S,-S, S,-S,  S,-S,-S, S, S,-S,  S,-S, S, S, S, S,  -S,-S, S,-S, S, S,
                };
                glVertexAttribPointer(gridAPos_,3,GL_FLOAT,GL_FALSE,0,wv);
                glDrawArrays(GL_LINES,0,24);
            } else if (obj.type == ObjType::SPHERE) {
                // 5 latitude rings + 8 longitude arcs
                const float Rw=0.83f;
                const int N_LAT=5, N_LON=8, SEG=32;
                std::vector<float> wv;
                wv.reserve((N_LAT-1 + N_LON)*SEG*6);
                for (int li=1; li<N_LAT; ++li) {
                    float phi=(float)M_PI*li/N_LAT, y=Rw*cosf(phi), r=Rw*sinf(phi);
                    for (int si=0;si<SEG;++si){
                        float a0=2.f*(float)M_PI*si/SEG, a1=2.f*(float)M_PI*(si+1)/SEG;
                        wv.insert(wv.end(),{r*cosf(a0),y,r*sinf(a0), r*cosf(a1),y,r*sinf(a1)});
                    }
                }
                for (int li=0; li<N_LON; ++li) {
                    float th=2.f*(float)M_PI*li/N_LON;
                    float ct=cosf(th), st=sinf(th);
                    for (int si=0;si<SEG;++si){
                        float p0=(float)M_PI*si/SEG, p1=(float)M_PI*(si+1)/SEG;
                        wv.insert(wv.end(),{Rw*sinf(p0)*ct,Rw*cosf(p0),Rw*sinf(p0)*st,
                                            Rw*sinf(p1)*ct,Rw*cosf(p1),Rw*sinf(p1)*st});
                    }
                }
                glVertexAttribPointer(gridAPos_,3,GL_FLOAT,GL_FALSE,0,wv.data());
                glDrawArrays(GL_LINES,0,(GLsizei)(wv.size()/3));
            } else { // CYLINDER
                // Top ring + bottom ring + 8 vertical lines
                const float Rw=0.63f, Hw=0.84f;
                const int CSEGS=32, VLINES=8;
                std::vector<float> wv;
                wv.reserve((CSEGS*2*2 + VLINES)*6);
                for (float hw : {Hw,-Hw})
                    for (int si=0;si<CSEGS;++si){
                        float a0=2.f*(float)M_PI*si/CSEGS, a1=2.f*(float)M_PI*(si+1)/CSEGS;
                        wv.insert(wv.end(),{Rw*cosf(a0),hw,Rw*sinf(a0), Rw*cosf(a1),hw,Rw*sinf(a1)});
                    }
                for (int vi=0;vi<VLINES;++vi){
                    float a=2.f*(float)M_PI*vi/VLINES;
                    wv.insert(wv.end(),{Rw*cosf(a),Hw,Rw*sinf(a), Rw*cosf(a),-Hw,Rw*sinf(a)});
                }
                glVertexAttribPointer(gridAPos_,3,GL_FLOAT,GL_FALSE,0,wv.data());
                glDrawArrays(GL_LINES,0,(GLsizei)(wv.size()/3));
            }

            glDisableVertexAttribArray(gridAPos_);
            glDepthFunc(GL_LESS);
            glUseProgram(objProg_);
        }
    }

    // -- Hit marker at current orbit pivot --
    if (showHitMarker_) {
        glUseProgram(gridProg_);
        glUniformMatrix4fv(gridUMVP_,1,GL_FALSE,mvp);
        float mx=camera.targetX, my=camera.targetY, mz=camera.targetZ;
        const float sz=0.07f;
        const float mv[]={
            mx-sz,my,mz,  mx+sz,my,mz,
            mx,my-sz,mz,  mx,my+sz,mz,
            mx,my,mz-sz,  mx,my,mz+sz,
        };
        glUniform4f(gridUColor_,1.f,1.f,0.1f,1.f);
        glEnableVertexAttribArray(gridAPos_);
        glVertexAttribPointer(gridAPos_,3,GL_FLOAT,GL_FALSE,0,mv);
        glDrawArrays(GL_LINES,0,6);
        glDisableVertexAttribArray(gridAPos_);
    }

    // -- Restore state for UI --
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0,0,(GLsizei)screenW_,(GLsizei)screenH_);

    // -- FPS overlay --
    if (showFps && !fpsText.empty()) {
        float fs=std::max(11.f,rh*0.045f); fs=std::min(fs,20.f);
        uiR.drawText(fpsText, rx+8.f, ry+6.f, fs, ui::Color{1.f,1.f,0.3f,0.85f});
    }
}

// ── onInput ───────────────────────────────────────────────────────────────────

bool SceneRenderer::onInput(const ui::InputEvent& e) {
    using ui::InputType;

    if (e.type == InputType::TOUCH_DOWN) {
        int slot=(ptrs_[0].id==-1)?0:(ptrs_[1].id==-1?1:-1);
        if(slot<0)return false;
        ptrs_[slot]={e.pointerId,e.x,e.y}; nPtrs_++;

        if(nPtrs_==1&&!blockOrbit_){
            int64_t now=nowMs();
            float dxL=e.x-lastTapX_, dyL=e.y-lastTapY_;
            // Second tap within window → enter double-tap-drag-zoom mode
            dtZoom_ = (lastTapMs_>0) && (now-lastTapMs_)<300LL
                      && sqrtf(dxL*dxL+dyL*dyL)<40.f;
            dtZoomY_     = e.y;
            prevOrbitX_  = e.x; prevOrbitY_  = e.y;
            tapStartX_   = e.x; tapStartY_   = e.y;
            tapStartMs_  = now; tapMoved_    = false;
        } else if(nPtrs_==2){
            blockOrbit_=true; tapMoved_=true;
            float dx=ptrs_[1].x-ptrs_[0].x, dy=ptrs_[1].y-ptrs_[0].y;
            prevDist_=sqrtf(dx*dx+dy*dy);
            prevMidX_=(ptrs_[0].x+ptrs_[1].x)*0.5f;
            prevMidY_=(ptrs_[0].y+ptrs_[1].y)*0.5f;
            twoFingerMode_=TwoFingerMode::UNDECIDED;
        }
        return true;
    }

    if (e.type == InputType::TOUCH_MOVE) {
        for(int i=0;i<2;++i)
            if(ptrs_[i].id==e.pointerId){ptrs_[i].x=e.x;ptrs_[i].y=e.y;}

        if(nPtrs_==1&&!blockOrbit_){
            float ddx=e.x-prevOrbitX_, ddy=e.y-prevOrbitY_;
            if (dtZoom_) {
                // Double-tap + drag vertical → zoom
                camera.distance *= 1.f + ddy * 0.004f;
                camera.clamp();
            } else {
                camera.azimuth  -= ddx * 0.25f;
                camera.elevation += ddy * 0.25f;
                camera.clamp();
            }
            prevOrbitX_=e.x; prevOrbitY_=e.y;
            float md=sqrtf((e.x-tapStartX_)*(e.x-tapStartX_)+(e.y-tapStartY_)*(e.y-tapStartY_));
            if(md>10.f)tapMoved_=true;
        } else if(nPtrs_==2){
            float dx=ptrs_[1].x-ptrs_[0].x, dy=ptrs_[1].y-ptrs_[0].y;
            float dist=sqrtf(dx*dx+dy*dy);
            float midX=(ptrs_[0].x+ptrs_[1].x)*0.5f, midY=(ptrs_[0].y+ptrs_[1].y)*0.5f;
            float ddx=midX-prevMidX_, ddy=midY-prevMidY_;

            if(twoFingerMode_==TwoFingerMode::UNDECIDED){
                float pinch=std::abs(dist-prevDist_), pan=sqrtf(ddx*ddx+ddy*ddy);
                if(pinch>10.f)twoFingerMode_=TwoFingerMode::ZOOM;
                else if(pan>10.f)twoFingerMode_=TwoFingerMode::PAN;
            }
            if(twoFingerMode_==TwoFingerMode::ZOOM){
                if(dist>1e-3f&&prevDist_>1e-3f){
                    float ratio=prevDist_/dist;
                    camera.distance*=1.f+(ratio-1.f)*0.6f;
                }
                camera.clamp();
            } else if(twoFingerMode_==TwoFingerMode::PAN){
                float tanH=tanf(camera.fovY*(3.14159265f/360.f));
                float panScale=2.f*camera.distance*tanH/(vpH_>0?vpH_:1.f);
                float r[3],u[3]; camera.rightAndUp(r,u);
                camera.targetX+=(-r[0]*ddx+u[0]*ddy)*panScale;
                camera.targetY+=(-r[1]*ddx+u[1]*ddy)*panScale;
                camera.targetZ+=(-r[2]*ddx+u[2]*ddy)*panScale;
            }
            prevDist_=dist; prevMidX_=midX; prevMidY_=midY;
        }
        return true;
    }

    if(e.type==InputType::TOUCH_UP||e.type==InputType::TOUCH_CANCEL){
        for(int i=0;i<2;++i){
            if(ptrs_[i].id!=e.pointerId)continue;
            ptrs_[i].id=-1; nPtrs_--; if(nPtrs_<0)nPtrs_=0;

            if(nPtrs_==0){
                blockOrbit_=false;
                if(e.type==InputType::TOUCH_UP&&!tapMoved_&&(nowMs()-tapStartMs_)<300LL){
                    int64_t now=nowMs();
                    float dx=e.x-lastTapX_, dy=e.y-lastTapY_;
                    bool isDouble=(lastTapMs_>0)&&(now-lastTapMs_)<300LL&&sqrtf(dx*dx+dy*dy)<40.f;
                    handleTap(e.x,e.y,isDouble);
                    if(isDouble){lastTapMs_=0;}
                    else{lastTapMs_=now;lastTapX_=e.x;lastTapY_=e.y;}
                }
                dtZoom_=false;
            }
            break;
        }
        return true;
    }
    return false;
}

} // namespace scene
