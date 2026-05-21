#include "font_atlas.h"
#include <android/asset_manager.h>
#include <android/log.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <cmath>
#include <cstring>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"FontAtlas",__VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "FontAtlas",__VA_ARGS__)

namespace ui {

FontAtlas::~FontAtlas() {
    if (ftFace_)    FT_Done_Face    ((FT_Face)ftFace_);
    if (ftLibrary_) FT_Done_FreeType((FT_Library)ftLibrary_);
    if (tex_)       glDeleteTextures(1, &tex_);
}

bool FontAtlas::init(AAssetManager* mgr, const char* assetPath, const int* sizes) {
    // ── Load TTF from APK assets ─────────────────────────────────────────────
    AAsset* asset = AAssetManager_open(mgr, assetPath, AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("Cannot open font asset: %s", assetPath);
        return false;
    }
    off_t len = AAsset_getLength(asset);
    fontData_.resize((size_t)len);
    AAsset_read(asset, fontData_.data(), (size_t)len);
    AAsset_close(asset);

    // ── Init FreeType ────────────────────────────────────────────────────────
    FT_Library lib;
    if (FT_Init_FreeType(&lib) != 0) {
        LOGE("FT_Init_FreeType failed");
        return false;
    }
    ftLibrary_ = lib;

    FT_Face face;
    if (FT_New_Memory_Face(lib,
                           (const FT_Byte*)fontData_.data(),
                           (FT_Long)fontData_.size(), 0, &face) != 0) {
        LOGE("FT_New_Memory_Face failed for: %s", assetPath);
        return false;
    }
    ftFace_ = face;

    // ── Bake glyphs into the CPU-side atlas ──────────────────────────────────
    pixels_.assign(ATLAS_W * ATLAS_H, 0);
    if (!bakeSizes(sizes)) return false;

    // ── Upload to GPU ────────────────────────────────────────────────────────
    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                 ATLAS_W, ATLAS_H, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels_.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    LOGI("FontAtlas ready — %zu glyphs in %dx%d atlas",
         glyphs_.size(), ATLAS_W, ATLAS_H);
    return true;
}

bool FontAtlas::bakeSizes(const int* sizes) {
    FT_Face face = (FT_Face)ftFace_;
    for (int i = 0; sizes[i] != 0; ++i) {
        int sz = sizes[i];
        if (FT_Set_Pixel_Sizes(face, 0, (FT_UInt)sz) != 0) {
            LOGE("FT_Set_Pixel_Sizes(%d) failed", sz);
            continue;
        }
        LineMetrics lm{};
        lm.ascender = (float)(face->size->metrics.ascender >> 6);
        lm.height   = (float)(face->size->metrics.height   >> 6);
        lineMetrics_[sz] = lm;
        bakedSizes_.push_back(sz);

        for (uint32_t cp = 32; cp <= 126; ++cp)
            bakeGlyph(cp, sz);
    }
    return true;
}

bool FontAtlas::bakeGlyph(uint32_t cp, int size) {
    FT_Face face = (FT_Face)ftFace_;
    if (FT_Set_Pixel_Sizes(face, 0, (FT_UInt)size) != 0) return false;

    FT_UInt idx = FT_Get_Char_Index(face, cp);
    if (FT_Load_Glyph(face, idx, FT_LOAD_RENDER) != 0) return false;

    FT_GlyphSlot slot = face->glyph;
    int bw = (int)slot->bitmap.width;
    int bh = (int)slot->bitmap.rows;

    // Shelf-pack: start a new shelf if the glyph doesn't fit horizontally.
    const int pad = 1;
    if (penX_ + bw + pad > ATLAS_W) {
        penX_  = pad;
        penY_ += shelfH_ + pad;
        shelfH_ = 0;
    }
    if (penY_ + bh + pad > ATLAS_H) {
        LOGE("Font atlas overflow at size=%d cp=%u", size, cp);
        return false;
    }
    shelfH_ = std::max(shelfH_, bh);

    // Copy bitmap rows into the atlas buffer.
    for (int row = 0; row < bh; ++row) {
        const uint8_t* src = slot->bitmap.buffer + row * slot->bitmap.pitch;
        uint8_t*       dst = pixels_.data() + (penY_ + row) * ATLAS_W + penX_;
        memcpy(dst, src, (size_t)bw);
    }

    GlyphMetrics g{};
    g.u0       = (float)penX_                / ATLAS_W;
    g.v0       = (float)penY_                / ATLAS_H;
    g.u1       = (float)(penX_ + bw)        / ATLAS_W;
    g.v1       = (float)(penY_ + bh)        / ATLAS_H;
    g.bearingX = (float)slot->bitmap_left;
    g.bearingY = (float)slot->bitmap_top;
    g.advanceX = (float)(slot->advance.x >> 6);
    g.bitmapW  = bw;
    g.bitmapH  = bh;
    glyphs_[{cp, size}] = g;

    penX_ += bw + pad;
    return true;
}

const GlyphMetrics* FontAtlas::getGlyph(uint32_t cp, int pixelSize) const {
    auto it = glyphs_.find({cp, pixelSize});
    return (it != glyphs_.end()) ? &it->second : nullptr;
}

float FontAtlas::ascender(int sz) const {
    auto it = lineMetrics_.find(sz);
    return it != lineMetrics_.end() ? it->second.ascender : (float)sz;
}

float FontAtlas::lineHeight(int sz) const {
    auto it = lineMetrics_.find(sz);
    return it != lineMetrics_.end() ? it->second.height : (float)sz * 1.2f;
}

int FontAtlas::snapSize(int req) const {
    if (bakedSizes_.empty()) return req;
    int best = bakedSizes_[0];
    int bestDist = std::abs(req - best);
    for (int s : bakedSizes_) {
        int d = std::abs(req - s);
        if (d < bestDist) { bestDist = d; best = s; }
    }
    return best;
}

} // namespace ui
