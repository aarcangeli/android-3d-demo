#pragma once
#include <GLES2/gl2.h>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

struct AAssetManager;

namespace ui {

struct GlyphMetrics {
    float u0, v0, u1, v1;  // Atlas UVs (0..1)
    float bearingX;          // Pixels right from cursor to glyph left edge
    float bearingY;          // Pixels up from baseline to glyph top edge
    float advanceX;          // Cursor advance in pixels
    int   bitmapW, bitmapH;
};

class FontAtlas {
public:
    static constexpr int ATLAS_W = 1024;
    static constexpr int ATLAS_H = 1024;

    ~FontAtlas();

    // Load font from Android asset. sizes[] must end with 0.
    bool init(AAssetManager* mgr, const char* assetPath, const int* sizes);

    const GlyphMetrics* getGlyph(uint32_t codepoint, int pixelSize) const;

    GLuint texture() const { return tex_; }

    float ascender (int pixelSize) const;
    float lineHeight(int pixelSize) const;

    // Snap an arbitrary float size to the nearest baked size.
    int snapSize(int requestedSize) const;

private:
    bool bakeSizes(const int* sizes);
    bool bakeGlyph(uint32_t cp, int size);

    struct LineMetrics { float ascender, height; };
    std::unordered_map<int, LineMetrics> lineMetrics_;

    struct GlyphKey {
        uint32_t cp;
        int      size;
        bool operator==(const GlyphKey& o) const {
            return cp == o.cp && size == o.size;
        }
    };
    struct GlyphKeyHash {
        size_t operator()(const GlyphKey& k) const {
            return std::hash<uint64_t>()((uint64_t)k.cp << 32 | (uint32_t)k.size);
        }
    };
    std::unordered_map<GlyphKey, GlyphMetrics, GlyphKeyHash> glyphs_;

    std::vector<uint8_t> pixels_;
    int penX_ = 1, penY_ = 1, shelfH_ = 0;

    std::vector<int>     bakedSizes_;
    std::vector<uint8_t> fontData_;

    GLuint tex_ = 0;

    // FreeType handles stored as void* to avoid exposing ft2build.h.
    void* ftLibrary_ = nullptr;
    void* ftFace_    = nullptr;
};

} // namespace ui
