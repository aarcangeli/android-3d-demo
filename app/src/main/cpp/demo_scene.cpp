#include "demo_scene.h"
#include "ui/widget.h"
#include <memory>
#include <string>
#include <algorithm>
#include <cstdio>

using namespace ui;

namespace pal {
    constexpr Color bg        {0.08f, 0.09f, 0.12f, 1.f};
    constexpr Color header    {0.10f, 0.12f, 0.18f, 1.f};
    constexpr Color panel     {0.12f, 0.14f, 0.20f, 1.f};
    constexpr Color panelDark {0.08f, 0.09f, 0.13f, 1.f};
    constexpr Color separator {0.25f, 0.28f, 0.35f, 1.f};
    constexpr Color textMain  {0.92f, 0.94f, 0.98f, 1.f};
    constexpr Color textDim   {0.55f, 0.60f, 0.72f, 1.f};
    constexpr Color accent    {0.20f, 0.60f, 1.00f, 1.f};
    constexpr Color green     {0.20f, 0.85f, 0.45f, 1.f};
    constexpr Color orange    {1.00f, 0.60f, 0.10f, 1.f};
    constexpr Color red       {0.95f, 0.25f, 0.25f, 1.f};
    constexpr Color purple    {0.65f, 0.35f, 1.00f, 1.f};
}

static void setStatus(Label** ppStatus, const std::string& msg, Color c) {
    if (ppStatus && *ppStatus) {
        (*ppStatus)->text      = msg;
        (*ppStatus)->textColor = c;
    }
}

void buildDemoScene(UISystem& sys,
                    std::function<void(float,float,float,float)> glCb,
                    float dp,
                    Label*& outFps,
                    Label*& outAngle,
                    Label*& outStatus)
{
    Container& root = sys.root();
    root.bgColor = pal::bg;

    // Root: vertical  [header | body | footer]
    {
        auto l = std::make_unique<LinearLayout>(LinearLayout::Orientation::VERTICAL, 0.f);
        root.layout = std::move(l);
    }

    // ── HEADER ───────────────────────────────────────────────────────────────
    auto* header   = root.make<Container>();
    header->bgColor = pal::header;
    header->padding = 10.f * dp;
    header->prefH   = 52.f * dp;
    {
        auto l = std::make_unique<LinearLayout>(LinearLayout::Orientation::HORIZONTAL, 8.f * dp);
        l->crossGravity = LinearLayout::Gravity::CENTER;
        header->layout = std::move(l);
    }

    auto* titleLbl = header->make<Label>("OpenGL Widget Demo");
    titleLbl->textColor = pal::textMain;
    titleLbl->fontSize  = 18.f * dp;
    titleLbl->weight    = 1.f;

    auto* fpsLbl = header->make<Label>("-- FPS");
    fpsLbl->textColor = pal::accent;
    fpsLbl->fontSize  = 16.f * dp;
    fpsLbl->prefW     = 90.f * dp;
    fpsLbl->align     = TextAlign::RIGHT;
    outFps = fpsLbl;

    // ── BODY: horizontal [left | gl | right] ─────────────────────────────────
    auto* body = root.make<Container>();
    body->weight = 1.f;
    {
        auto l = std::make_unique<LinearLayout>(LinearLayout::Orientation::HORIZONTAL, 0.f);
        body->layout = std::move(l);
    }

    // ── LEFT PANEL (scrollable) ───────────────────────────────────────────────
    auto* scroll = body->make<ScrollContainer>();
    scroll->prefW      = 150.f * dp;
    scroll->scrollbarW   = 5.f  * dp;
    scroll->scrollbarPad = 2.f  * dp;
    scroll->trackColor   = pal::panelDark;
    scroll->thumbColor   = Color{0.35f, 0.42f, 0.62f, 0.9f};

    Container* leftPanel = &scroll->content();
    leftPanel->bgColor = pal::panel;
    leftPanel->padding = 10.f * dp;
    {
        auto l = std::make_unique<LinearLayout>(LinearLayout::Orientation::VERTICAL, 7.f * dp);
        l->crossGravity = LinearLayout::Gravity::FILL;
        leftPanel->layout = std::move(l);
    }

    auto makeSection = [dp](Container* p, const std::string& t) {
        auto* l = p->make<Label>(t);
        l->textColor = pal::accent;
        l->fontSize  = 12.f * dp;
        l->prefH     = 16.f * dp;
    };
    auto makeInfo = [dp](Container* p, const std::string& t) -> Label* {
        auto* l = p->make<Label>(t);
        l->textColor = pal::textDim;
        l->fontSize  = 13.f * dp;
        l->prefH     = 20.f * dp;
        return l;
    };
    auto makeSep = [dp](Container* p) {
        auto* s = p->make<Container>();
        s->bgColor = pal::separator;
        s->prefH   = std::max(1.f, 1.f * dp);
    };

    makeSection(leftPanel, "INFO");
    outAngle = makeInfo(leftPanel, "Angle: 0.0");
    makeInfo(leftPanel, "Vertices: 3");
    makeInfo(leftPanel, "Draw calls: 1");
    makeInfo(leftPanel, "API: GL ES 2.0");
    makeInfo(leftPanel, "Renderer: NDK");
    makeInfo(leftPanel, "VSync: on");
    makeSep(leftPanel);
    makeSection(leftPanel, "BUILD");
    makeInfo(leftPanel, "Min SDK: 21");
    makeInfo(leftPanel, "Target SDK: 35");
    makeInfo(leftPanel, "ABI: arm64-v8a");
    makeInfo(leftPanel, "C++ 17");
    makeInfo(leftPanel, "NDK r26");
    makeSep(leftPanel);
    makeSection(leftPanel, "RENDERER");
    makeInfo(leftPanel, "FreeType 2.13");
    makeInfo(leftPanel, "Font: FreeSans");
    makeInfo(leftPanel, "Atlas: 1024^2");
    makeInfo(leftPanel, "Glyphs: ASCII");
    makeSep(leftPanel);
    makeSection(leftPanel, "PALETTE");

    auto* chips = leftPanel->make<Container>();
    chips->prefH = 18.f * dp;
    {
        auto l = std::make_unique<LinearLayout>(LinearLayout::Orientation::HORIZONTAL, 4.f * dp);
        l->crossGravity = LinearLayout::Gravity::FILL;
        chips->layout = std::move(l);
    }
    for (Color col : {pal::red, pal::green, pal::accent, pal::purple, pal::orange}) {
        auto* chip = chips->make<Container>();
        chip->bgColor = col;
        chip->weight  = 1.f;
    }

    makeSep(leftPanel);
    makeSection(leftPanel, "TOUCH");
    makeInfo(leftPanel, "Multi-pointer");
    makeInfo(leftPanel, "Scroll: drag");
    makeInfo(leftPanel, "Buttons: tap");
    makeSep(leftPanel);
    makeSection(leftPanel, "ABOUT");
    makeInfo(leftPanel, "android-3d-demo");
    makeInfo(leftPanel, "MIT License");
    makeInfo(leftPanel, "github.com/");

    // ── GL WIDGET (center) ───────────────────────────────────────────────────
    auto* glw = body->make<GLWidget>();
    glw->weight       = 1.f;
    glw->onRender     = glCb;
    glw->borderColor  = Color{0.3f, 0.4f, 0.6f, 0.6f};
    glw->borderWidth  = 2.f * dp;

    // ── RIGHT PANEL ──────────────────────────────────────────────────────────
    auto* rightPanel = body->make<Container>();
    rightPanel->bgColor = pal::panel;
    rightPanel->padding = 10.f * dp;
    rightPanel->prefW   = 150.f * dp;
    {
        auto l = std::make_unique<LinearLayout>(LinearLayout::Orientation::VERTICAL, 8.f * dp);
        l->crossGravity = LinearLayout::Gravity::FILL;
        rightPanel->layout = std::move(l);
    }

    Label** pps = &outStatus;

    auto makeBtn = [&, dp](Container* p, const std::string& lbl, Color bg,
                        std::function<void()> cb) -> Button* {
        auto* b = p->make<Button>(lbl);
        b->bgNormal    = bg;
        b->bgHover     = bg.lerp(Colors::white, 0.15f);
        b->bgPressed   = bg.lerp(Colors::black, 0.20f);
        b->bgDisabled  = pal::separator;
        b->textColor   = Colors::white;
        b->fontSize    = 14.f * dp;
        b->prefH       = 38.f * dp;
        b->cornerRadius = 6.f * dp;
        b->onClick     = std::move(cb);
        return b;
    };

    makeSection(rightPanel, "ACTIONS");
    makeBtn(rightPanel, "Reset View",   pal::accent,  [pps](){
        setStatus(pps, "View reset", Color{0.35f,0.72f,1.f,1.f});
    });
    makeBtn(rightPanel, "Pause",        pal::orange,  [pps](){
        setStatus(pps, "Paused", pal::orange);
    });
    makeBtn(rightPanel, "Screenshot",   pal::green,   [pps](){
        setStatus(pps, "Screenshot!", pal::green);
    });
    makeBtn(rightPanel, "Toggle Color", pal::purple,  [pps](){
        setStatus(pps, "Color changed", pal::purple);
    });

    makeSep(rightPanel);
    makeSection(rightPanel, "STATUS");

    auto* statusLbl = rightPanel->make<Label>("Ready");
    statusLbl->textColor = pal::textDim;
    statusLbl->fontSize  = 13.f * dp;
    statusLbl->prefH     = 20.f * dp;
    outStatus = statusLbl;

    auto* rFill = rightPanel->make<Container>();
    rFill->weight = 1.f;

    makeSep(rightPanel);
    makeSection(rightPanel, "SPEED");

    auto* speedRow = rightPanel->make<Container>();
    speedRow->prefH = 38.f * dp;
    {
        auto l = std::make_unique<LinearLayout>(LinearLayout::Orientation::HORIZONTAL, 6.f * dp);
        l->crossGravity = LinearLayout::Gravity::FILL;
        speedRow->layout = std::move(l);
    }

    static float speedMul = 1.f;
    auto* btnMinus = makeBtn(speedRow, "-", pal::panelDark, [pps]() {
        speedMul = std::max(0.25f, speedMul * 0.5f);
        char buf[32];
        snprintf(buf, sizeof(buf), "Speed %.0f%%", speedMul * 100.f);
        setStatus(pps, buf, pal::orange);
    });
    btnMinus->weight = 1.f;

    auto* btnPlus = makeBtn(speedRow, "+", pal::panelDark, [pps]() {
        speedMul = std::min(8.f, speedMul * 2.f);
        char buf[32];
        snprintf(buf, sizeof(buf), "Speed %.0f%%", speedMul * 100.f);
        setStatus(pps, buf, pal::green);
    });
    btnPlus->weight = 1.f;

    // ── FOOTER ───────────────────────────────────────────────────────────────
    auto* footer = root.make<Container>();
    footer->bgColor = pal::header;
    footer->padding = 8.f * dp;
    footer->prefH   = 52.f * dp;
    {
        auto l = std::make_unique<LinearLayout>(LinearLayout::Orientation::HORIZONTAL, 8.f * dp);
        l->crossGravity = LinearLayout::Gravity::CENTER;
        footer->layout = std::move(l);
    }

    struct FBtn { const char* label; Color bg; const char* msg; Color msgCol; };
    FBtn fbtns[] = {
        {"About",    pal::accent,    "About",     Color{0.35f,0.72f,1.f,1.f}},
        {"Settings", pal::separator, "Settings",  pal::textDim               },
        {"Export",   pal::green,     "Exported!", pal::green                  },
        {"Share",    pal::purple,    "Shared!",   pal::purple                 },
    };
    for (auto& fb : fbtns) {
        std::string msg    = fb.msg;
        Color       msgCol = fb.msgCol;
        auto* b = makeBtn(footer, fb.label, fb.bg, [pps, msg, msgCol]() {
            setStatus(pps, msg, msgCol);
        });
        b->weight = 1.f;
    }
}
