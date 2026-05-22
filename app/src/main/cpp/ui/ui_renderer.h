#pragma once
#include "widget.h"
#include "font_atlas.h"
#include <GLES2/gl2.h>
#include <vector>
#include <string>

struct AAssetManager;

namespace ui {

// Batched 2-D renderer for UI widgets.
// Coordinate system: pixels, (0,0) = top-left.
class UIRenderer {
public:
    // mgr: Android asset manager used to load the font TTF.
    // density: display density scalar (dp → pixels), e.g. 3.0 on xxhdpi.
    void init(AAssetManager* mgr, float density);
    void shutdown();

    // Call once per frame before any draw*().
    void begin(float screenW, float screenH);
    // Flush pending batches and restore GL state.
    void end();

    // ── Immediate draw calls (buffered internally) ──────────────────────────

    void drawRect     (float x, float y, float w, float h, Color c);
    void drawRoundRect(float x, float y, float w, float h, float radius, Color c);
    void drawRectBorder(float x, float y, float w, float h, float thickness, Color c);

    // Text: top of the line at (x, y); fontSize = desired pixel height.
    void drawText(const std::string& text, float x, float y,
                  float fontSize, Color c, TextAlign align = TextAlign::LEFT);

    float measureText(const std::string& text, float fontSize) const;

    // ── Scissor helpers ─────────────────────────────────────────────────────
    void pushScissor(float x, float y, float w, float h);
    void popScissor();

    // Submit all pending draw batches to the GPU immediately.
    // Must be called before any raw GL draw calls (e.g. inside GLWidget::onRender).
    void flush();

    float screenW() const { return sw_; }
    float screenH() const { return sh_; }

private:
    struct Vertex {
        float x, y, u, v, r, g, b, a;
    };

    enum class DrawMode { COLOR, TEXT };

    struct Batch {
        DrawMode             mode;
        std::vector<Vertex>   verts;
        std::vector<uint16_t> indices;
    };

    Batch&   currentBatch(DrawMode mode);
    void     submitBatch(Batch& b);

    void pushQuad(Batch& b,
                  float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  Color c);

    void buildShader();

    float sw_ = 0, sh_ = 0;

    GLuint prog_     = 0;
    GLint  aPos_     = -1, aUV_ = -1, aCol_ = -1;
    GLint  uRes_     = -1, uTex_ = -1, uUseTex_ = -1;

    GLuint vbo_ = 0, ibo_ = 0;

    FontAtlas            fontAtlas_;
    std::vector<Batch>   batches_;

    struct ScissorState { float x, y, w, h; };
    std::vector<ScissorState> scissorStack_;
};

} // namespace ui
