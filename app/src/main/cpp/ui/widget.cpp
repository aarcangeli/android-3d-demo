#include "widget.h"
#include "ui_renderer.h"
#include <algorithm>

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
    // Draw border frame around the GL viewport area.
    if (borderWidth > 0 && borderColor.a > 0.001f)
        r.drawRectBorder(x, y, w, h, borderWidth, borderColor);
    // The actual GL content is rendered by UISystem after flushing batches.
    if (onRender) onRender(x, y, w, h);
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
