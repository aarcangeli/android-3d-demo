#pragma once
#include "input.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace ui {

class UIRenderer;

struct Color {
    float r, g, b, a;
    constexpr Color(float r, float g, float b, float a = 1.f) : r(r), g(g), b(b), a(a) {}
    static constexpr Color rgba(float r, float g, float b, float a = 1.f) { return {r,g,b,a}; }
    Color withAlpha(float na) const { return {r,g,b,na}; }
    Color lerp(Color o, float t) const {
        return {r+(o.r-r)*t, g+(o.g-g)*t, b+(o.b-b)*t, a+(o.a-a)*t};
    }
};

namespace Colors {
    constexpr Color transparent{0,0,0,0};
    constexpr Color black      {0,0,0,1};
    constexpr Color white      {1,1,1,1};
    constexpr Color red        {1,0,0,1};
    constexpr Color green      {0,1,0,1};
    constexpr Color blue       {0,0,1,1};
    constexpr Color gray       {0.5f,0.5f,0.5f,1};
    constexpr Color darkGray   {0.2f,0.2f,0.2f,1};
    constexpr Color lightGray  {0.75f,0.75f,0.75f,1};
    constexpr Color accent     {0.2f,0.6f,1.0f,1};
    constexpr Color accentDark {0.1f,0.4f,0.8f,1};
}

enum class TextAlign { LEFT, CENTER, RIGHT };

// ─── Base Widget ─────────────────────────────────────────────────────────────

class Widget {
public:
    float x = 0, y = 0, w = 0, h = 0;
    bool  visible = true;
    bool  enabled = true;
    Widget* parent = nullptr;

    // Preferred / minimum size hints (used by layouts; 0 = unconstrained)
    float prefW = 0, prefH = 0;
    float minW  = 0, minH  = 0;
    // Weight for flex-fill layouts (0 = fixed, >0 = fills proportionally)
    float weight = 0;

    virtual ~Widget() = default;

    virtual void draw(UIRenderer& r) = 0;
    // Returns true if the event was consumed.
    virtual bool onInput(const InputEvent& /*e*/) { return false; }
    // Called by the parent Container after it sets bounds.
    virtual void doLayout() {}

    bool contains(float px, float py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
    void setBounds(float nx, float ny, float nw, float nh) {
        x = nx; y = ny; w = nw; h = nh;
        doLayout();
    }
};

// ─── Layout strategies ───────────────────────────────────────────────────────

class Container; // forward

class Layout {
public:
    virtual ~Layout() = default;
    virtual void apply(Container& c) = 0;
};

// ─── Container ───────────────────────────────────────────────────────────────

class Container : public Widget {
public:
    Color bgColor = Colors::transparent;
    float padding = 0;
    std::unique_ptr<Layout> layout;

    void addChild(std::unique_ptr<Widget> w) {
        w->parent = this;
        children_.push_back(std::move(w));
    }

    // Typed helper: constructs T in place and returns raw pointer for further config.
    template<typename T, typename... Args>
    T* make(Args&&... args) {
        auto w = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = w.get();
        addChild(std::move(w));
        return ptr;
    }

    const std::vector<std::unique_ptr<Widget>>& children() const { return children_; }

    void clearChildren() { children_.clear(); }

    void draw(UIRenderer& r) override;
    bool onInput(const InputEvent& e) override;
    void doLayout() override;

private:
    std::vector<std::unique_ptr<Widget>> children_;
};

// ─── Label ───────────────────────────────────────────────────────────────────

class Label : public Widget {
public:
    std::string text;
    Color       textColor = Colors::white;
    float       fontSize  = 16.f;   // rendered pixel height
    TextAlign   align     = TextAlign::LEFT;

    Label() = default;
    explicit Label(const std::string& t) : text(t) {}

    void draw(UIRenderer& r) override;
};

// ─── Button ──────────────────────────────────────────────────────────────────

class Button : public Widget {
public:
    std::string text;
    float       fontSize   = 16.f;
    Color bgNormal   = Colors::accent;
    Color bgHover    = Colors::accentDark;
    Color bgPressed  = Color{0.05f,0.25f,0.6f,1};
    Color bgDisabled = Colors::gray;
    Color textColor  = Colors::white;
    float cornerRadius = 6.f;

    std::function<void()> onClick;

    Button() = default;
    explicit Button(const std::string& t) : text(t) {}

    void draw(UIRenderer& r) override;
    bool onInput(const InputEvent& e) override;
    void doLayout() override { /* nothing */ }

private:
    enum class State { NORMAL, HOVERED, PRESSED };
    State state_    = State::NORMAL;
    int   touchId_  = -1;
};

// ─── GLWidget ────────────────────────────────────────────────────────────────
// A widget that hands off its pixel-rect to a custom GL render callback.
// The callback receives (x, y, w, h) in screen pixels (y from top).

class GLWidget : public Widget {
public:
    // Callback receives the widget bounds so it can set up the scissor / viewport.
    std::function<void(float x, float y, float w, float h)> onRender;

    Color borderColor = Color{1,1,1,0.15f};
    float borderWidth = 1.f;

    void draw(UIRenderer& r) override;
};

// ─── ScrollContainer ─────────────────────────────────────────────────────────

enum class OverscrollMode { NONE, GLOW, STRETCH };

// A clipped, vertically-scrollable container with a visual scrollbar.

class ScrollContainer : public Widget {
public:
    Color trackColor   = Color{0.12f, 0.14f, 0.20f, 1.f};
    Color thumbColor   = Color{0.40f, 0.45f, 0.62f, 0.85f};
    float scrollbarW   = 6.f;
    float scrollbarPad = 2.f;

    OverscrollMode overscrollMode = OverscrollMode::GLOW;
    Color          glowColor      = Colors::accent;
    float          glowMaxH       = 40.f;   // dp-scaled by caller

    Container& content() { return content_; }

    template<typename T, typename... Args>
    T* make(Args&&... args) { return content_.make<T>(std::forward<Args>(args)...); }

    void draw(UIRenderer& r) override;
    bool onInput(const InputEvent& e) override;
    void doLayout() override;

private:
    Container content_;
    float scrollY_     = 0.f;
    float contentH_    = 0.f;
    float overScrollY_ = 0.f;   // rubber-band extra; sign: + = bottom OS, - = top OS
    void  drawOverscrollEffect(UIRenderer& r);

    enum class DragState { NONE, TENTATIVE, SCROLLING };
    DragState drag_        = DragState::NONE;
    int       dragId_      = -1;
    float     dragStartY_  = 0.f;
    float     dragScrollY_ = 0.f;

    bool  showBar()  const { return contentH_ > h + 0.5f; }
    float barX()     const { return x + w - scrollbarW - scrollbarPad; }
    float trackTop() const { return y + scrollbarPad; }
    float trackH()   const { return h - scrollbarPad * 2.f; }
    float thumbH()   const;
    float thumbTop() const;
    float contentW() const { return showBar() ? w - scrollbarW - scrollbarPad * 2.f : w; }
    void  clampScroll();
};

// ─── LinearLayout ────────────────────────────────────────────────────────────

class LinearLayout : public Layout {
public:
    enum class Orientation { HORIZONTAL, VERTICAL };
    enum class Gravity     { START, CENTER, END, FILL };

    Orientation orientation = Orientation::VERTICAL;
    float       spacing     = 0;
    Gravity     crossGravity = Gravity::FILL;

    explicit LinearLayout(Orientation o = Orientation::VERTICAL, float spacing = 0)
        : orientation(o), spacing(spacing) {}

    void apply(Container& c) override;
};

// ─── AbsoluteLayout ──────────────────────────────────────────────────────────
// Children keep their own x,y,w,h relative to parent's top-left + padding.

class AbsoluteLayout : public Layout {
public:
    void apply(Container& c) override;
};

} // namespace ui
