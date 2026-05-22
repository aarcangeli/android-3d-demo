#include "ui_renderer.h"
#include <GLES2/gl2.h>
#include <android/log.h>
#include <cmath>
#include <cstring>
#include <algorithm>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"UIRenderer",__VA_ARGS__)

namespace ui {

// ── Shaders ──────────────────────────────────────────────────────────────────

static const char* UI_VERT = R"glsl(
attribute vec2 aPos;
attribute vec2 aUV;
attribute vec4 aCol;
uniform vec2 uRes;
varying vec2 vUV;
varying vec4 vCol;
void main() {
    vec2 ndc = (aPos / uRes) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV  = aUV;
    vCol = aCol;
}
)glsl";

// uUseTex == 1: sample font atlas (GL_LUMINANCE → .r acts as alpha mask).
// uUseTex == 0: solid color quad.
static const char* UI_FRAG = R"glsl(
precision mediump float;
varying vec2 vUV;
varying vec4 vCol;
uniform sampler2D uTex;
uniform float uUseTex;
void main() {
    if (uUseTex > 0.5) {
        float a = texture2D(uTex, vUV).r;
        gl_FragColor = vec4(vCol.rgb, vCol.a * a);
    } else {
        gl_FragColor = vCol;
    }
}
)glsl";

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512]; glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        LOGE("Shader error: %s", buf);
        glDeleteShader(s); return 0;
    }
    return s;
}

// ── Init / Shutdown ──────────────────────────────────────────────────────────

// Base dp sizes used by the demo UI.
static const float BASE_DP_SIZES[] = {12.f, 13.f, 14.f, 16.f, 18.f};

void UIRenderer::init(AAssetManager* mgr, float density) {
    buildShader();
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ibo_);

    // Convert dp values to pixel sizes for the current display density.
    // Deduplicate in case low density collapses several dp values to the same px.
    int sizes[16];
    int n = 0;
    for (float dpSz : BASE_DP_SIZES) {
        int px = std::max(8, (int)std::round(dpSz * density));
        bool dup = false;
        for (int i = 0; i < n; ++i) if (sizes[i] == px) { dup = true; break; }
        if (!dup && n < 15) sizes[n++] = px;
    }
    sizes[n] = 0;

    fontAtlas_.init(mgr, "FreeSans.ttf", sizes);
}

void UIRenderer::shutdown() {
    if (prog_) { glDeleteProgram(prog_); prog_ = 0; }
    if (vbo_)  { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (ibo_)  { glDeleteBuffers(1, &ibo_); ibo_ = 0; }
}

void UIRenderer::buildShader() {
    GLuint v = compileShader(GL_VERTEX_SHADER,   UI_VERT);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, UI_FRAG);
    prog_ = glCreateProgram();
    glAttachShader(prog_, v);
    glAttachShader(prog_, f);
    glLinkProgram(prog_);
    glDeleteShader(v); glDeleteShader(f);

    aPos_    = glGetAttribLocation (prog_, "aPos");
    aUV_     = glGetAttribLocation (prog_, "aUV");
    aCol_    = glGetAttribLocation (prog_, "aCol");
    uRes_    = glGetUniformLocation(prog_, "uRes");
    uTex_    = glGetUniformLocation(prog_, "uTex");
    uUseTex_ = glGetUniformLocation(prog_, "uUseTex");
}

// ── Frame ────────────────────────────────────────────────────────────────────

void UIRenderer::begin(float screenW, float screenH) {
    sw_ = screenW; sh_ = screenH;
    batches_.clear();
    scissorStack_.clear();
}

void UIRenderer::end() {
    flush();
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
}

// ── Batch management ─────────────────────────────────────────────────────────

UIRenderer::Batch& UIRenderer::currentBatch(DrawMode mode) {
    if (batches_.empty() || batches_.back().mode != mode)
        batches_.push_back({mode, {}, {}});
    return batches_.back();
}

void UIRenderer::flush() {
    if (batches_.empty()) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(prog_);
    glUniform2f(uRes_, sw_, sh_);
    glUniform1i(uTex_, 0);
    glActiveTexture(GL_TEXTURE0);

    glEnableVertexAttribArray(aPos_);
    glEnableVertexAttribArray(aUV_);
    glEnableVertexAttribArray(aCol_);

    for (auto& b : batches_)
        if (!b.verts.empty()) submitBatch(b);

    glDisableVertexAttribArray(aPos_);
    glDisableVertexAttribArray(aUV_);
    glDisableVertexAttribArray(aCol_);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    batches_.clear();
}

void UIRenderer::submitBatch(Batch& b) {
    bool useTex = (b.mode == DrawMode::TEXT);
    glUniform1f(uUseTex_, useTex ? 1.f : 0.f);
    glBindTexture(GL_TEXTURE_2D, useTex ? fontAtlas_.texture() : 0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(b.verts.size() * sizeof(Vertex)),
                 b.verts.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(b.indices.size() * sizeof(uint16_t)),
                 b.indices.data(), GL_DYNAMIC_DRAW);

    const GLsizei stride = sizeof(Vertex);
    glVertexAttribPointer(aPos_, 2, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(Vertex, x));
    glVertexAttribPointer(aUV_,  2, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(Vertex, u));
    glVertexAttribPointer(aCol_, 4, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(Vertex, r));

    glDrawElements(GL_TRIANGLES, (GLsizei)b.indices.size(),
                   GL_UNSIGNED_SHORT, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void UIRenderer::pushQuad(Batch& b,
                            float x,  float y,  float w,  float h,
                            float u0, float v0, float u1, float v1,
                            Color c) {
    auto base = (uint16_t)b.verts.size();
    b.verts.push_back({x,   y,   u0, v0, c.r, c.g, c.b, c.a});
    b.verts.push_back({x+w, y,   u1, v0, c.r, c.g, c.b, c.a});
    b.verts.push_back({x+w, y+h, u1, v1, c.r, c.g, c.b, c.a});
    b.verts.push_back({x,   y+h, u0, v1, c.r, c.g, c.b, c.a});
    b.indices.insert(b.indices.end(), {
        base, (uint16_t)(base+1), (uint16_t)(base+2),
        base, (uint16_t)(base+2), (uint16_t)(base+3)
    });
}

// ── Draw calls ───────────────────────────────────────────────────────────────

void UIRenderer::drawRect(float x, float y, float w, float h, Color c) {
    if (c.a < 0.001f || w <= 0 || h <= 0) return;
    pushQuad(currentBatch(DrawMode::COLOR), x, y, w, h, 0, 0, 0, 0, c);
}

void UIRenderer::drawRoundRect(float x, float y, float w, float h,
                                float radius, Color c) {
    if (c.a < 0.001f || w <= 0 || h <= 0) return;
    radius = std::min(radius, std::min(w, h) * 0.5f);

    // Fill interior cross.
    drawRect(x + radius, y,          w - radius*2, h,           c);
    drawRect(x,          y + radius, radius,       h - radius*2, c);
    drawRect(x + w - radius, y + radius, radius,  h - radius*2, c);

    // Corner fans.
    Batch& b = currentBatch(DrawMode::COLOR);
    const int segs = 8;
    const float PI = 3.14159265f;

    auto corner = [&](float cx, float cy, float startAngle) {
        auto ci = (uint16_t)b.verts.size();
        b.verts.push_back({cx, cy, 0, 0, c.r, c.g, c.b, c.a});
        float a0 = startAngle;
        auto prev = (uint16_t)b.verts.size();
        b.verts.push_back({cx + radius*cosf(a0), cy + radius*sinf(a0),
                           0, 0, c.r, c.g, c.b, c.a});
        for (int i = 1; i <= segs; ++i) {
            float a   = startAngle + (float)i / segs * (PI * 0.5f);
            auto  cur = (uint16_t)b.verts.size();
            b.verts.push_back({cx + radius*cosf(a), cy + radius*sinf(a),
                               0, 0, c.r, c.g, c.b, c.a});
            b.indices.insert(b.indices.end(), {ci, prev, cur});
            prev = cur;
        }
    };
    corner(x + radius,     y + radius,     PI);
    corner(x + w - radius, y + radius,     PI * 1.5f);
    corner(x + w - radius, y + h - radius, 0.f);
    corner(x + radius,     y + h - radius, PI * 0.5f);
}

void UIRenderer::drawRectBorder(float x, float y, float w, float h,
                                  float t, Color c) {
    if (c.a < 0.001f) return;
    drawRect(x,       y,       w, t,      c);
    drawRect(x,       y+h-t,   w, t,      c);
    drawRect(x,       y+t,     t, h-2*t,  c);
    drawRect(x+w-t,   y+t,     t, h-2*t,  c);
}

void UIRenderer::drawText(const std::string& text, float x, float y,
                            float fontSize, Color c, TextAlign align) {
    if (text.empty() || c.a < 0.001f || fontSize < 1.f) return;

    int   sz    = fontAtlas_.snapSize((int)std::round(fontSize));
    float scale = fontSize / (float)sz;

    // Measure for alignment.
    if (align != TextAlign::LEFT) {
        float tw = measureText(text, fontSize);
        if (align == TextAlign::CENTER) x -= tw * 0.5f;
        else                            x -= tw;
    }

    // Baseline position: y is top of line, ascender is distance to baseline.
    float baseline = y + fontAtlas_.ascender(sz) * scale;

    Batch& b = currentBatch(DrawMode::TEXT);
    float  cx = x;

    for (unsigned char ch : text) {
        if (ch < 32 || ch > 126) {
            cx += fontSize * 0.5f;
            continue;
        }
        const GlyphMetrics* g = fontAtlas_.getGlyph(ch, sz);
        if (!g) { cx += fontSize * 0.4f; continue; }

        // Glyph top-left in screen pixels.
        float gx = cx + g->bearingX * scale;
        float gy = baseline - g->bearingY * scale;
        float gw = (float)g->bitmapW * scale;
        float gh = (float)g->bitmapH * scale;

        if (gw > 0 && gh > 0)
            pushQuad(b, gx, gy, gw, gh, g->u0, g->v0, g->u1, g->v1, c);

        cx += g->advanceX * scale;
    }
}

float UIRenderer::measureText(const std::string& text, float fontSize) const {
    int   sz    = fontAtlas_.snapSize((int)std::round(fontSize));
    float scale = fontSize / (float)sz;
    float total = 0;
    for (unsigned char ch : text) {
        if (ch < 32 || ch > 126) { total += fontSize * 0.5f; continue; }
        const GlyphMetrics* g = fontAtlas_.getGlyph(ch, sz);
        total += g ? g->advanceX * scale : fontSize * 0.4f;
    }
    return total;
}

// ── Scissor ──────────────────────────────────────────────────────────────────

void UIRenderer::pushScissor(float x, float y, float w, float h) {
    flush();
    scissorStack_.push_back({x, y, w, h});
    glEnable(GL_SCISSOR_TEST);
    glScissor((GLint)x, (GLint)(sh_ - y - h), (GLsizei)w, (GLsizei)h);
}

void UIRenderer::popScissor() {
    flush();
    scissorStack_.pop_back();
    if (scissorStack_.empty()) {
        glDisable(GL_SCISSOR_TEST);
    } else {
        auto& s = scissorStack_.back();
        glScissor((GLint)s.x, (GLint)(sh_ - s.y - s.h), (GLsizei)s.w, (GLsizei)s.h);
    }
}

} // namespace ui
