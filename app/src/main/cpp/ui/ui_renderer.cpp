#include "ui_renderer.h"
#include "font_data.h"
#include <GLES2/gl2.h>
#include <android/log.h>
#include <cstring>
#include <cmath>
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
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512]; glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        LOGE("Shader error: %s", buf);
        glDeleteShader(s); return 0;
    }
    return s;
}

// ── Init / Shutdown ──────────────────────────────────────────────────────────

void UIRenderer::init() {
    buildShader();
    buildFontTexture();

    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ibo_);
}

void UIRenderer::shutdown() {
    if (prog_)    { glDeleteProgram(prog_);    prog_ = 0; }
    if (fontTex_) { glDeleteTextures(1, &fontTex_); fontTex_ = 0; }
    if (vbo_)     { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (ibo_)     { glDeleteBuffers(1, &ibo_); ibo_ = 0; }
}

void UIRenderer::buildShader() {
    GLuint v = compileShader(GL_VERTEX_SHADER,   UI_VERT);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, UI_FRAG);
    prog_ = glCreateProgram();
    glAttachShader(prog_, v);
    glAttachShader(prog_, f);
    glLinkProgram(prog_);
    glDeleteShader(v);
    glDeleteShader(f);

    aPos_    = glGetAttribLocation (prog_, "aPos");
    aUV_     = glGetAttribLocation (prog_, "aUV");
    aCol_    = glGetAttribLocation (prog_, "aCol");
    uRes_    = glGetUniformLocation(prog_, "uRes");
    uTex_    = glGetUniformLocation(prog_, "uTex");
    uUseTex_ = glGetUniformLocation(prog_, "uUseTex");
}

void UIRenderer::buildFontTexture() {
    // Build a single-channel (R8 via GL_LUMINANCE on ES 2.0) texture atlas.
    // Atlas: 128 × 48 px, 16 cols × 6 rows of 8×8 glyph cells.
    std::vector<uint8_t> atlas(FONT_ATLAS_W * FONT_ATLAS_H, 0);

    for (int ch = 0; ch < 96; ++ch) {
        int col = ch % FONT_COLS;
        int row = ch / FONT_COLS;
        for (int gy = 0; gy < FONT_CELL_H; ++gy) {
            uint8_t row_bits = FONT_8X8[ch][gy];
            for (int gx = 0; gx < FONT_CELL_W; ++gx) {
                bool on = (row_bits >> (7 - gx)) & 1;
                int px = col * FONT_CELL_W + gx;
                int py = row * FONT_CELL_H + gy;
                atlas[py * FONT_ATLAS_W + px] = on ? 0xFF : 0x00;
            }
        }
    }

    glGenTextures(1, &fontTex_);
    glBindTexture(GL_TEXTURE_2D, fontTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                 FONT_ATLAS_W, FONT_ATLAS_H, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, atlas.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ── Frame ────────────────────────────────────────────────────────────────────

void UIRenderer::begin(float screenW, float screenH) {
    sw_ = screenW;
    sh_ = screenH;
    batches_.clear();
    scissorStack_.clear();
}

void UIRenderer::end() {
    flush();

    // Restore state that the 3-D renderer expects.
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
}

// ── Batch management ─────────────────────────────────────────────────────────

UIRenderer::Batch& UIRenderer::currentBatch(DrawMode mode) {
    if (batches_.empty() || batches_.back().mode != mode) {
        batches_.push_back({mode, {}, {}});
    }
    return batches_.back();
}

void UIRenderer::flush() {
    if (batches_.empty()) return;

    // Save & set GL state for UI rendering.
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

    for (auto& b : batches_) {
        if (b.verts.empty()) continue;
        submitBatch(b);
    }

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

    if (useTex) {
        glBindTexture(GL_TEXTURE_2D, fontTex_);
    } else {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 b.verts.size() * sizeof(Vertex),
                 b.verts.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 b.indices.size() * sizeof(uint16_t),
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
                           float x, float y, float w, float h,
                           float u0, float v0, float u1, float v1,
                           Color c) {
    uint16_t base = (uint16_t)b.verts.size();
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
    Batch& b = currentBatch(DrawMode::COLOR);
    pushQuad(b, x, y, w, h, 0, 0, 0, 0, c);
}

void UIRenderer::drawRoundRect(float x, float y, float w, float h,
                                float radius, Color c) {
    if (c.a < 0.001f || w <= 0 || h <= 0) return;
    radius = std::min(radius, std::min(w, h) * 0.5f);

    // Fill main body (cross)
    drawRect(x + radius, y,          w - radius*2, h,          c);
    drawRect(x,          y + radius, radius,        h - radius*2, c);
    drawRect(x + w - radius, y + radius, radius,   h - radius*2, c);

    // Rounded corners using triangle fans
    const int segs = 8;
    Batch& b = currentBatch(DrawMode::COLOR);

    auto corner = [&](float cx, float cy, float startAngle) {
        uint16_t center_idx = (uint16_t)b.verts.size();
        b.verts.push_back({cx, cy, 0, 0, c.r, c.g, c.b, c.a});
        uint16_t prev_idx = (uint16_t)b.verts.size();
        float a0 = startAngle;
        b.verts.push_back({cx + radius * cosf(a0), cy + radius * sinf(a0),
                           0, 0, c.r, c.g, c.b, c.a});
        for (int i = 1; i <= segs; ++i) {
            float a = startAngle + (float)i / segs * (3.14159265f * 0.5f);
            uint16_t cur = (uint16_t)b.verts.size();
            b.verts.push_back({cx + radius * cosf(a), cy + radius * sinf(a),
                               0, 0, c.r, c.g, c.b, c.a});
            b.indices.insert(b.indices.end(), {center_idx, prev_idx, cur});
            prev_idx = cur;
        }
    };
    static const float PI = 3.14159265f;
    corner(x + radius,         y + radius,         PI);         // top-left
    corner(x + w - radius,     y + radius,         PI * 1.5f);  // top-right
    corner(x + w - radius,     y + h - radius,     0.f);        // bottom-right
    corner(x + radius,         y + h - radius,     PI * 0.5f);  // bottom-left
}

void UIRenderer::drawRectBorder(float x, float y, float w, float h,
                                 float thickness, Color c) {
    if (c.a < 0.001f) return;
    float t = thickness;
    drawRect(x,         y,         w, t,          c); // top
    drawRect(x,         y+h-t,     w, t,          c); // bottom
    drawRect(x,         y+t,       t, h-2*t,      c); // left
    drawRect(x+w-t,     y+t,       t, h-2*t,      c); // right
}

void UIRenderer::drawText(const std::string& text,
                           float x, float y,
                           float fontSize, Color c, TextAlign align) {
    if (text.empty() || c.a < 0.001f || fontSize < 1.f) return;

    float scale = fontSize / (float)FONT_CELL_H;

    if (align != TextAlign::LEFT) {
        float tw = measureText(text, fontSize);
        if (align == TextAlign::CENTER) x -= tw * 0.5f;
        else                            x -= tw;
    }

    Batch& b = currentBatch(DrawMode::TEXT);

    float cx = x;
    for (unsigned char ch : text) {
        if (ch < 32 || ch > 127) { cx += fontSize * 0.5f; continue; }
        int idx = ch - 32;
        int col = idx % FONT_COLS;
        int row = idx / FONT_COLS;

        float u0 = (float)(col * FONT_CELL_W)       / FONT_ATLAS_W;
        float v0 = (float)(row * FONT_CELL_H)       / FONT_ATLAS_H;
        float u1 = (float)((col+1) * FONT_CELL_W)   / FONT_ATLAS_W;
        float v1 = (float)((row+1) * FONT_CELL_H)   / FONT_ATLAS_H;

        float gw = FONT_CELL_W * scale;
        float gh = FONT_CELL_H * scale;

        pushQuad(b, cx, y, gw, gh, u0, v0, u1, v1, c);
        cx += gw;
    }
}

float UIRenderer::measureText(const std::string& text, float fontSize) const {
    float scale = fontSize / (float)FONT_CELL_H;
    int   count = 0;
    for (unsigned char ch : text)
        count += (ch >= 32 && ch <= 127) ? 1 : 0;
    return count * FONT_CELL_W * scale;
}

// ── Scissor ──────────────────────────────────────────────────────────────────

void UIRenderer::pushScissor(float x, float y, float w, float h) {
    flush();
    scissorStack_.push_back({x, y, w, h, true});
    glEnable(GL_SCISSOR_TEST);
    // GL scissor origin is bottom-left; we store top-left coords.
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
