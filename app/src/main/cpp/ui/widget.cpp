#include "widget.h"
#include "ui_renderer.h"
#include <algorithm>
#include <cmath>

namespace ui {

// ── Container ────────────────────────────────────────────────────────────────

void Container::doLayout() {
    if (layout) layout->apply(*this);
}

void Container::draw(UIRenderer& r) {
    if (!visible) return;
    if (bgColor.a > 0.001f) r.drawRect(x, y, w, h, bgColor);
    for (auto& child : children_)
        if (child->visible) child->draw(r);
}

bool Container::onInput(const InputEvent& e) {
    if (!visible || !enabled) return false;
    // Dispatch to children in reverse paint order (topmost first).
    for (int i = (int)children_.size() - 1; i >= 0; --i) {
        auto& child = children_[i];
        if (!child->visible || !child->enabled) continue;
        if (e.isTouchEvent() && !child->contains(e.x, e.y)) {
            // Still deliver MOVE/UP/CANCEL to the pressed child even outside bounds.
            if (e.type == InputType::TOUCH_DOWN) continue;
        }
        if (child->onInput(e)) return true;
    }
    return false;
}

// ── Label ────────────────────────────────────────────────────────────────────

void Label::draw(UIRenderer& r) {
    if (!visible || text.empty()) return;
    float cx = x;
    if (align == TextAlign::CENTER) cx = x + w * 0.5f;
    else if (align == TextAlign::RIGHT) cx = x + w;

    float ty = y + (h - fontSize) * 0.5f; // vertically centered
    r.drawText(text, cx, ty, fontSize, textColor, align);
}

// ── Button ───────────────────────────────────────────────────────────────────

void Button::draw(UIRenderer& r) {
    if (!visible) return;

    Color bg = enabled ? bgNormal : bgDisabled;
    if (enabled) {
        if (state_ == State::HOVERED) bg = bgHover;
        if (state_ == State::PRESSED) bg = bgPressed;
    }

    r.drawRoundRect(x, y, w, h, cornerRadius, bg);

    if (!text.empty()) {
        float tx = x + w * 0.5f;
        float ty = y + (h - fontSize) * 0.5f;
        r.drawText(text, tx, ty, fontSize, textColor, TextAlign::CENTER);
    }
}

bool Button::onInput(const InputEvent& e) {
    if (!visible || !enabled) return false;

    if (e.type == InputType::TOUCH_DOWN && contains(e.x, e.y)) {
        state_   = State::PRESSED;
        touchId_ = e.pointerId;
        return true;
    }
    if (e.type == InputType::TOUCH_MOVE && touchId_ == e.pointerId) {
        state_ = contains(e.x, e.y) ? State::PRESSED : State::HOVERED;
        return true;
    }
    if ((e.type == InputType::TOUCH_UP || e.type == InputType::TOUCH_CANCEL)
        && touchId_ == e.pointerId) {
        bool wasInside = contains(e.x, e.y);
        state_   = State::NORMAL;
        touchId_ = -1;
        if (wasInside && e.type == InputType::TOUCH_UP && onClick)
            onClick();
        return wasInside;
    }
    return false;
}

// ── GLWidget ─────────────────────────────────────────────────────────────────

void GLWidget::draw(UIRenderer& r) {
    if (!visible) return;
    // Flush all queued UI batches so content drawn before us reaches the GPU
    // before we make direct GL calls. Without this the full-screen background
    // quad would be submitted after the triangle and paint over it.
    r.flush();
    if (onRender) onRender(x, y, w, h);
    // Border is drawn as a new batch on top of the GL content.
    if (borderWidth > 0 && borderColor.a > 0.001f)
        r.drawRectBorder(x, y, w, h, borderWidth, borderColor);
}

// ── ScrollContainer ───────────────────────────────────────────────────────────

void ScrollContainer::clampScroll() {
    float maxScroll = std::max(0.f, contentH_ - h);
    scrollY_ = std::max(0.f, std::min(scrollY_, maxScroll));
}

void ScrollContainer::doLayout() {
    auto measure = [&](float cw) -> float {
        content_.x = 0; content_.y = 0;
        content_.w = cw; content_.h = 1e6f;
        content_.doLayout();
        float maxY = 0.f;
        for (auto& k : content_.children())
            if (k->visible) maxY = std::max(maxY, k->y + k->h);
        return maxY;
    };

    // Pass 1: measure without scrollbar.
    bool barBefore = showBar();
    contentH_ = measure(w);
    // If bar visibility changed, re-measure with narrower width.
    if (showBar() != barBefore)
        contentH_ = measure(contentW());

    clampScroll();
    // Final pass: position children with scroll offset and correct x/y.
    float cw = contentW();
    content_.x = x; content_.y = y - scrollY_ - overScrollY_;
    content_.w = cw; content_.h = std::max(h, contentH_);
    content_.doLayout();
}

void ScrollContainer::draw(UIRenderer& r) {
    if (!visible) return;
    // Spring-back animation for overscroll
    if (drag_ == DragState::NONE && std::abs(overScrollY_) > 0.3f) {
        float decay = (overscrollMode == OverscrollMode::STRETCH) ? 0.55f : 0.68f;
        overScrollY_ *= decay;
        doLayout();
    } else if (drag_ == DragState::NONE && overScrollY_ != 0.f) {
        overScrollY_ = 0.f;
        doLayout();
    }
    r.pushScissor(x, y, w, h);
    content_.draw(r);
    drawOverscrollEffect(r);   // <-- overscroll gradient inside scissor
    r.popScissor();
    if (showBar()) {
        float tx  = barX();
        float ty  = trackTop();
        float th  = trackH();
        float rad = scrollbarW * 0.5f;
        r.drawRoundRect(tx, ty, scrollbarW, th,    rad, trackColor);
        r.drawRoundRect(tx, thumbTop(), scrollbarW, thumbH(), rad, thumbColor);
    }
}

float ScrollContainer::thumbH() const {
    float ratio = std::min(1.f, h / contentH_);
    return std::max(scrollbarW * 2.f, trackH() * ratio);
}

float ScrollContainer::thumbTop() const {
    float maxY      = trackH() - thumbH();
    float maxScroll = contentH_ - h;
    float t = (maxScroll > 0.f) ? scrollY_ / maxScroll : 0.f;
    return trackTop() + maxY * t;
}

void ScrollContainer::drawOverscrollEffect(UIRenderer& r) {
    if (overscrollMode == OverscrollMode::NONE) return;
    if (std::abs(overScrollY_) < 0.5f) return;

    float os = overScrollY_;
    float t  = std::min(1.f, std::abs(os) / std::max(1.f, glowMaxH));

    if (overscrollMode == OverscrollMode::GLOW) {
        // Colored glow gradient at the edge, fading inward
        float gh    = glowMaxH * t;
        Color solid = glowColor.withAlpha(0.45f * t);
        Color clear = glowColor.withAlpha(0.f);
        if (os < 0.f) {
            r.drawGradientRect(x, y,           w, gh, solid, clear);
        } else {
            r.drawGradientRect(x, y + h - gh,  w, gh, clear, solid);
        }
    } else { // STRETCH
        // Fade content edge to bg color, simulating stretched thinning
        Color bg   = (content_.bgColor.a > 0.001f)
                         ? content_.bgColor
                         : Color{0.08f, 0.09f, 0.12f, 1.f};
        Color bgFt = bg.withAlpha(bg.a * std::min(1.f, t * 1.5f));
        Color bgNa = bg.withAlpha(0.f);
        float sh   = std::min(glowMaxH, std::abs(os) * 1.8f);
        if (os < 0.f) {
            r.drawGradientRect(x, y,           w, sh, bgFt, bgNa);
        } else {
            r.drawGradientRect(x, y + h - sh,  w, sh, bgNa, bgFt);
        }
    }
}

bool ScrollContainer::onInput(const InputEvent& e) {
    if (!visible || !enabled) return false;
    if (!e.isTouchEvent()) return false;

    bool inside = contains(e.x, e.y);

    if (e.type == InputType::TOUCH_DOWN) {
        if (!inside) return false;
        dragId_      = e.pointerId;
        dragStartY_  = e.y;
        dragScrollY_ = scrollY_;
        drag_        = DragState::TENTATIVE;
        content_.onInput(e);
        return true;
    }
    if (e.type == InputType::TOUCH_MOVE && dragId_ == e.pointerId) {
        float dy = e.y - dragStartY_;
        if (drag_ == DragState::TENTATIVE && std::abs(dy) > 8.f) {
            drag_ = DragState::SCROLLING;
            InputEvent cancel = e;
            cancel.type = InputType::TOUCH_CANCEL;
            content_.onInput(cancel);
        }
        if (drag_ == DragState::SCROLLING) {
            float raw      = dragScrollY_ - dy;
            float maxScroll = std::max(0.f, contentH_ - h);
            if (raw < 0.f) {
                scrollY_     = 0.f;
                overScrollY_ = raw * 0.3f;           // negative = top overscroll
            } else if (raw > maxScroll) {
                scrollY_     = maxScroll;
                overScrollY_ = (raw - maxScroll) * 0.3f;  // positive = bottom overscroll
            } else {
                scrollY_     = raw;
                overScrollY_ = 0.f;
            }
            doLayout();
        } else {
            content_.onInput(e);
        }
        return true;
    }
    if ((e.type == InputType::TOUCH_UP || e.type == InputType::TOUCH_CANCEL)
        && dragId_ == e.pointerId) {
        bool wasScrolling = (drag_ == DragState::SCROLLING);
        drag_   = DragState::NONE;
        dragId_ = -1;
        if (!wasScrolling) content_.onInput(e);
        return true;
    }
    return false;
}

// ── LinearLayout ─────────────────────────────────────────────────────────────

void LinearLayout::apply(Container& c) {
    bool horiz = (orientation == Orientation::HORIZONTAL);

    float px = c.x + c.padding;
    float py = c.y + c.padding;
    float avW = c.w - c.padding * 2;
    float avH = c.h - c.padding * 2;

    // Collect children pointers
    const auto& kids = c.children();
    int n = (int)kids.size();
    if (n == 0) return;

    // Compute total fixed size and total weight
    float fixedTotal = 0;
    float weightTotal = 0;
    for (auto& k : kids) {
        if (!k->visible) continue;
        float pref = horiz ? k->prefW : k->prefH;
        if (k->weight > 0) weightTotal += k->weight;
        else               fixedTotal  += pref;
    }
    fixedTotal += spacing * (n - 1);

    float main = horiz ? avW : avH;
    float flexPool = std::max(0.f, main - fixedTotal);

    float cursor = horiz ? px : py;

    for (auto& k : kids) {
        if (!k->visible) continue;
        float sz;
        if (k->weight > 0) sz = flexPool * (k->weight / weightTotal);
        else               sz = horiz ? k->prefW : k->prefH;

        float cx, cy, cw, ch;
        if (horiz) {
            cw = sz;
            ch = (crossGravity == Gravity::FILL) ? avH : k->prefH;
            cx = cursor;
            cy = py;
            if (crossGravity == Gravity::CENTER) cy = c.y + (c.h - ch) * 0.5f;
            else if (crossGravity == Gravity::END) cy = c.y + c.h - c.padding - ch;
        } else {
            ch = sz;
            cw = (crossGravity == Gravity::FILL) ? avW : k->prefW;
            cy = cursor;
            cx = px;
            if (crossGravity == Gravity::CENTER) cx = c.x + (c.w - cw) * 0.5f;
            else if (crossGravity == Gravity::END) cx = c.x + c.w - c.padding - cw;
        }
        k->setBounds(cx, cy, cw, ch);
        cursor += sz + spacing;
    }
}

// ── AbsoluteLayout ────────────────────────────────────────────────────────────

void AbsoluteLayout::apply(Container& c) {
    float ox = c.x + c.padding;
    float oy = c.y + c.padding;
    for (auto& k : c.children())
        k->setBounds(ox + k->x, oy + k->y, k->w, k->h);
}

} // namespace ui
