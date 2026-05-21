#pragma once
#include "widget.h"
#include <GLES2/gl2.h>
#include <vector>
#include <string>

namespace ui {

// Batched 2-D renderer for UI widgets.
// Coordinate system: pixels, (0,0) = top-left.
class UIRenderer {
public:
    void init();
    void shutdown();

    // Call once per frame before any draw*().
    void begin(float screenW, float screenH);
    // Flush pending batches and restore GL state.
    void end();

    // ── Immediate draw calls (buffered internally) ──────────────────────────

    // Filled axis-aligned rectangle.
    void drawRect(float x, float y, float w, float h, Color c);

    // Rounded rectangle (radius 0 = sharp corners, uses more segments).
    void drawRoundRect(float x, float y, float w, float h, float radius, Color c);

    // 1-px border around a rectangle.
    void drawRectBorder(float x, float y, float w, float h, float thickness, Color c);

    // Text: baseline at (x, y+fontSize), fontSize = pixel height.
    void drawText(const std::string& text, float x, float y,
                  float fontSize, Color c, TextAlign align = TextAlign::LEFT);

    // Measured text width in pixels at given fontSize.
    float measureText(const std::string& text, float fontSize) const;

    // ── Scissor helpers ─────────────────────────────────────────────────────
    // Flushes the current batch, then sets the scissor rectangle.
    void pushScissor(float x, float y, float w, float h);
    void popScissor();

    // ── State ───────────────────────────────────────────────────────────────
    float screenW() const { return sw_; }
    float screenH() const { return sh_; }

private:
    // ── Vertex layout ───────────────────────────────────────────────────────
    struct Vertex {
        float x, y;
        float u, v;
        float r, g, b, a;
    };

    enum class DrawMode { COLOR, TEXT };

    // A batch groups vertices that share the same texture / shader mode.
    struct Batch {
        DrawMode mode;
        std::vector<Vertex>   verts;
        std::vector<uint16_t> indices;
    };

    Batch&   currentBatch(DrawMode mode);
    void     flush();
    void     submitBatch(Batch& b);
    uint32_t addVerts(Batch& b, int count); // returns base index

    void buildFontTexture();
    void buildShader();

    // Adds a quad (2 triangles) to the given batch.
    void pushQuad(Batch& b,
                  float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  Color c);

    float sw_ = 0, sh_ = 0;

    GLuint prog_    = 0;
    GLint  aPos_    = -1, aUV_ = -1, aCol_ = -1;
    GLint  uRes_    = -1, uTex_ = -1, uUseTex_ = -1;

    GLuint fontTex_ = 0;
    GLuint vbo_     = 0;
    GLuint ibo_     = 0;

    std::vector<Batch> batches_;

    struct ScissorState { float x,y,w,h; bool active; };
    std::vector<ScissorState> scissorStack_;
};

} // namespace ui
