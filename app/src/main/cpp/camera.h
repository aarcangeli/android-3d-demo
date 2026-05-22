#pragma once
#include <cmath>
#include <cstring>

namespace scene {

// ── 4×4 column-major matrix helpers ──────────────────────────────────────────

static inline void mat4Identity(float m[16]) {
    memset(m, 0, 64);
    m[0] = m[5] = m[10] = m[15] = 1.f;
}

// out = a * b  (all column-major)
static inline void mat4Mul(const float a[16], const float b[16], float out[16]) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a[k*4+r] * b[c*4+k];
            out[c*4+r] = s;
        }
}

// ── Camera (spherical orbit around a target point) ────────────────────────────

struct Camera {
    float azimuth   =  45.f;   // degrees, horizontal orbit
    float elevation =  45.f;   // degrees, vertical orbit (clamped ±89°)
    float distance  =   5.f;   // distance from target
    float targetX   =   0.f;
    float targetY   =   0.f;
    float targetZ   =   0.f;
    float fovY      =  45.f;   // vertical field-of-view in degrees

    void clamp() {
        if (elevation >  89.f) elevation =  89.f;
        if (elevation < -89.f) elevation = -89.f;
        if (distance  <  0.05f) distance =  0.05f;
        if (distance  > 50.f)  distance  = 50.f;
    }

    void eyePos(float& ex, float& ey, float& ez) const {
        const float az = azimuth   * (3.14159265f / 180.f);
        const float el = elevation * (3.14159265f / 180.f);
        ex = targetX + distance * cosf(el) * sinf(az);
        ey = targetY + distance * sinf(el);
        ez = targetZ + distance * cosf(el) * cosf(az);
    }

    // Camera right and true-up vectors (rows 0 and 1 of the view matrix).
    void rightAndUp(float r[3], float u[3]) const {
        float vm[16]; viewMatrix(vm);
        r[0]=vm[0]; r[1]=vm[4]; r[2]=vm[8];
        u[0]=vm[1]; u[1]=vm[5]; u[2]=vm[9];
    }

    // Standard lookAt view matrix (column-major).
    void viewMatrix(float m[16]) const {
        float ex, ey, ez;
        eyePos(ex, ey, ez);

        // Forward = normalize(target − eye)
        float fx=targetX-ex, fy=targetY-ey, fz=targetZ-ez;
        float fl = sqrtf(fx*fx+fy*fy+fz*fz);
        if (fl < 1e-7f) fl = 1e-7f;
        fx/=fl; fy/=fl; fz/=fl;

        // Right = normalize(forward × worldUp)  [worldUp=(0,1,0)]
        // cross((fx,fy,fz),(0,1,0)) = (-fz, 0, fx)
        float rx=-fz, ry=0.f, rz=fx;
        float rl = sqrtf(rx*rx+rz*rz);
        if (rl < 1e-7f) { rx=1.f; rl=1.f; }
        rx/=rl; rz/=rl;  // ry stays 0

        // True up = right × forward
        float vx = /*ry*fz=*/0.f - rz*fy;
        float vy = rz*fx - rx*fz;
        float vz = rx*fy /*- ry*fx=0*/;

        // Column-major view matrix [col*4+row]
        //   row0=[R, -R·E]   row1=[V, -V·E]   row2=[-F, F·E]
        m[0]=rx;  m[4]=ry;  m[8] =rz;   m[12]=-(rx*ex+ry*ey+rz*ez);
        m[1]=vx;  m[5]=vy;  m[9] =vz;   m[13]=-(vx*ex+vy*ey+vz*ez);
        m[2]=-fx; m[6]=-fy; m[10]=-fz;  m[14]= (fx*ex+fy*ey+fz*ez);
        m[3]=0;   m[7]=0;   m[11]=0;    m[15]=1;
    }

    // Perspective projection matrix (column-major, reverse-z convention).
    void projMatrix(float aspect, float nearP, float farP, float m[16]) const {
        const float f = 1.f / tanf(fovY * (3.14159265f / 360.f));
        memset(m, 0, 64);
        m[0]  =  f / aspect;
        m[5]  =  f;
        m[10] = -(farP + nearP) / (farP - nearP);
        m[11] = -1.f;
        m[14] = -2.f * farP * nearP / (farP - nearP);
    }
};

} // namespace scene
