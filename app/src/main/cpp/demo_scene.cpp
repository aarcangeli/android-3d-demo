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
                    float dp,
                    float screenW,
                    int apiLevel,
                    scene::SceneRenderer& scene,
                    GLWidget*& outGLWidget,
                    Label*& outFps,
                    Label*& outAngle,
                    Label*& outStatus)
{
    // compact = portrait phone: width in dp < 480
    bool compact = (screenW > 0.f && dp > 0.f) ? (screenW / dp < 480.f) : false;

    Container& root = sys.root();
    root.bgColor = pal::bg;

    // Root: vertical  [header | body | (footer if wide)]
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

    auto* titleLbl = header->make<Label>("OpenGL NDK Demo");
    titleLbl->textColor = pal::textMain;
    titleLbl->fontSize  = 18.f * dp;
    titleLbl->weight    = 1.f;

    auto* fpsLbl = header->make<Label>("-- FPS");
    fpsLbl->textColor = pal::accent;
    fpsLbl->fontSize  = 16.f * dp;
    fpsLbl->prefW     = 90.f * dp;
    fpsLbl->align     = TextAlign::RIGHT;
    outFps = fpsLbl;

    // ── BODY ─────────────────────────────────────────────────────────────────
    auto* body = root.make<Container>();
    body->weight = 1.f;

    // Helper lambdas
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

    auto* sp = &scene;
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

    // Lambda to populate info content into a container
    auto buildInfoContent = [&](Container* p) {
        makeSection(p, "INFO");
        outAngle = makeInfo(p, "Angle: 0.0");
        makeInfo(p, "Vertices: 24");
        makeInfo(p, "Draw calls: 2");
        makeInfo(p, "API: GL ES 2.0");
        makeInfo(p, "Renderer: NDK");
        makeInfo(p, "VSync: on");
        makeSep(p);
        makeSection(p, "BUILD");
        makeInfo(p, "Min SDK: 21");
        makeInfo(p, "Target SDK: 35");
        makeInfo(p, "ABI: arm64-v8a");
        makeInfo(p, "C++ 20");
        makeInfo(p, "NDK r26");
        makeSep(p);
        makeSection(p, "RENDERER");
        makeInfo(p, "FreeType 2.13");
        makeInfo(p, "Font: FreeSans");
        makeInfo(p, "Atlas: 1024^2");
        makeInfo(p, "Glyphs: ASCII");
        makeSep(p);
        makeSection(p, "PALETTE");

        auto* chips = p->make<Container>();
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

        makeSep(p);
        makeSection(p, "TOUCH");
        makeInfo(p, "Multi-pointer");
        makeInfo(p, "Scroll: drag");
        makeInfo(p, "Buttons: tap");
        makeSep(p);
        makeSection(p, "ABOUT");
        makeInfo(p, "android-3d-demo");
        makeInfo(p, "MIT License");
        makeInfo(p, "github.com/");
    };

    // Lambda to populate controls content into a container (panel or tab)
    // statusInHere: whether to put the status label in this container
    auto buildControlsContent = [&](Container* p, bool statusInHere) {
        makeSection(p, "ACTIONS");
        makeBtn(p, "Reset View", pal::accent, [sp, pps](){
            sp->resetView();
            setStatus(pps, "View reset", Color{0.35f,0.72f,1.f,1.f});
        });

        // Pause toggle - use a static bool per-build (lambda captures sp)
        makeBtn(p, "Pause", pal::orange, [sp, pps](){
            if (sp->autoRotSpeed != 0.f) {
                sp->autoRotSpeed = 0.f;
                setStatus(pps, "Paused", pal::orange);
            } else {
                sp->autoRotSpeed = 0.5f;
                setStatus(pps, "Resumed", pal::green);
            }
        });
        makeBtn(p, "Screenshot", pal::green, [pps](){
            setStatus(pps, "Screenshot!", pal::green);
        });

        makeSep(p);
        makeSection(p, "SPEED");

        auto* speedRow = p->make<Container>();
        speedRow->prefH = 38.f * dp;
        {
            auto l = std::make_unique<LinearLayout>(LinearLayout::Orientation::HORIZONTAL, 6.f * dp);
            l->crossGravity = LinearLayout::Gravity::FILL;
            speedRow->layout = std::move(l);
        }

        auto* btnMinus = makeBtn(speedRow, "-", pal::panelDark, [sp, pps]() {
            sp->autoRotSpeed = std::max(0.1f, sp->autoRotSpeed * 0.5f);
            char buf[32];
            snprintf(buf, sizeof(buf), "Speed: %.2f", sp->autoRotSpeed);
            setStatus(pps, buf, pal::orange);
        });
        btnMinus->weight = 1.f;

        auto* btnPlus = makeBtn(speedRow, "+", pal::panelDark, [sp, pps]() {
            sp->autoRotSpeed = std::min(8.f, sp->autoRotSpeed * 2.f);
            char buf[32];
            snprintf(buf, sizeof(buf), "Speed: %.2f", sp->autoRotSpeed);
            setStatus(pps, buf, pal::green);
        });
        btnPlus->weight = 1.f;

        if (statusInHere) {
            makeSep(p);
            makeSection(p, "STATUS");
            auto* statusLbl = p->make<Label>("Ready");
            statusLbl->textColor = pal::textDim;
            statusLbl->fontSize  = 13.f * dp;
            statusLbl->prefH     = 20.f * dp;
            outStatus = statusLbl;
        }
    };

    if (!compact) {
        // ── WIDE LAYOUT: horizontal [left scroll | GLWidget | right panel] ──
        {
            auto l = std::make_unique<LinearLayout>(LinearLayout::Orientation::HORIZONTAL, 0.f);
            body->layout = std::move(l);
        }

        // Left scrollable panel
        auto* scroll = body->make<ScrollContainer>();
        scroll->prefW      = 150.f * dp;
        scroll->scrollbarW   = 5.f  * dp;
        scroll->scrollbarPad = 2.f  * dp;
        scroll->trackColor   = pal::panelDark;
        scroll->thumbColor   = Color{0.35f, 0.42f, 0.62f, 0.9f};
        scroll->overscrollMode = (apiLevel >= 31)
            ? ui::OverscrollMode::STRETCH
            : ui::OverscrollMode::GLOW;
        scroll->glowColor = pal::accent;
        scroll->glowMaxH  = 40.f * dp;

        Container* leftPanel = &scroll->content();
        leftPanel->bgColor = pal::panel;
        leftPanel->padding = 10.f * dp;
        {
            auto l = std::make_unique<LinearLayout>(LinearLayout::Orientation::VERTICAL, 7.f * dp);
            l->crossGravity = LinearLayout::Gravity::FILL;
            leftPanel->layout = std::move(l);
        }
        buildInfoContent(leftPanel);

        // GL Widget (center)
        auto* glw = body->make<GLWidget>();
        glw->weight       = 1.f;
        glw->borderColor  = Color{0.3f, 0.4f, 0.6f, 0.6f};
        glw->borderWidth  = 2.f * dp;
        outGLWidget = glw;

        // Right panel
        auto* rightPanel = body->make<Container>();
        rightPanel->bgColor = pal::panel;
        rightPanel->padding = 10.f * dp;
        rightPanel->prefW   = 150.f * dp;
        {
            auto l = std::make_unique<LinearLayout>(LinearLayout::Orientation::VERTICAL, 8.f * dp);
            l->crossGravity = LinearLayout::Gravity::FILL;
            rightPanel->layout = std::move(l);
        }

        buildControlsContent(rightPanel, false);

        makeSep(rightPanel);
        makeSection(rightPanel, "STATUS");
        auto* statusLbl = rightPanel->make<Label>("Ready");
        statusLbl->textColor = pal::textDim;
        statusLbl->fontSize  = 13.f * dp;
        statusLbl->prefH     = 20.f * dp;
        outStatus = statusLbl;

        auto* rFill = rightPanel->make<Container>();
        rFill->weight = 1.f;

        // ── FOOTER ───────────────────────────────────────────────────────────
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

    } else {
        // ── COMPACT LAYOUT: vertical [GLWidget | TabContainer] ───────────────
        {
            auto l = std::make_unique<LinearLayout>(LinearLayout::Orientation::VERTICAL, 0.f);
            body->layout = std::move(l);
        }

        // GL Widget (flex)
        auto* glw = body->make<GLWidget>();
        glw->weight       = 1.f;
        glw->borderColor  = Color{0.3f, 0.4f, 0.6f, 0.6f};
        glw->borderWidth  = 2.f * dp;
        outGLWidget = glw;

        // Tab container
        auto* tabs = body->make<TabContainer>();
        tabs->prefH       = 220.f * dp;
        tabs->tabBarH     = 36.f  * dp;
        tabs->fontSize    = 14.f  * dp;
        tabs->activeColor = pal::accent;
        tabs->inactiveColor = pal::panelDark;
        tabs->textColor   = Colors::white;

        // "Info" tab
        Container* infoTab = tabs->addTab("Info");
        infoTab->bgColor = pal::panel;
        infoTab->padding = 10.f * dp;
        {
            auto l = std::make_unique<LinearLayout>(LinearLayout::Orientation::VERTICAL, 7.f * dp);
            l->crossGravity = LinearLayout::Gravity::FILL;
            infoTab->layout = std::move(l);
        }

        // Wrap info tab in a scroll container
        auto* infoScroll = infoTab->make<ScrollContainer>();
        infoScroll->weight = 1.f;
        infoScroll->scrollbarW   = 5.f  * dp;
        infoScroll->scrollbarPad = 2.f  * dp;
        infoScroll->trackColor   = pal::panelDark;
        infoScroll->thumbColor   = Color{0.35f, 0.42f, 0.62f, 0.9f};
        infoScroll->overscrollMode = (apiLevel >= 31)
            ? ui::OverscrollMode::STRETCH
            : ui::OverscrollMode::GLOW;
        infoScroll->glowColor = pal::accent;
        infoScroll->glowMaxH  = 40.f * dp;

        Container* infoContent = &infoScroll->content();
        infoContent->bgColor = pal::panel;
        infoContent->padding = 0.f;
        {
            auto l = std::make_unique<LinearLayout>(LinearLayout::Orientation::VERTICAL, 7.f * dp);
            l->crossGravity = LinearLayout::Gravity::FILL;
            infoContent->layout = std::move(l);
        }
        buildInfoContent(infoContent);

        // "Controls" tab
        Container* ctrlTab = tabs->addTab("Controls");
        ctrlTab->bgColor = pal::panel;
        ctrlTab->padding = 10.f * dp;
        {
            auto l = std::make_unique<LinearLayout>(LinearLayout::Orientation::VERTICAL, 7.f * dp);
            l->crossGravity = LinearLayout::Gravity::FILL;
            ctrlTab->layout = std::move(l);
        }

        // Wrap controls in a scroll container
        auto* ctrlScroll = ctrlTab->make<ScrollContainer>();
        ctrlScroll->weight = 1.f;
        ctrlScroll->scrollbarW   = 5.f  * dp;
        ctrlScroll->scrollbarPad = 2.f  * dp;
        ctrlScroll->trackColor   = pal::panelDark;
        ctrlScroll->thumbColor   = Color{0.35f, 0.42f, 0.62f, 0.9f};
        ctrlScroll->overscrollMode = (apiLevel >= 31)
            ? ui::OverscrollMode::STRETCH
            : ui::OverscrollMode::GLOW;
        ctrlScroll->glowColor = pal::accent;
        ctrlScroll->glowMaxH  = 40.f * dp;

        Container* ctrlContent = &ctrlScroll->content();
        ctrlContent->bgColor = pal::panel;
        ctrlContent->padding = 0.f;
        {
            auto l = std::make_unique<LinearLayout>(LinearLayout::Orientation::VERTICAL, 7.f * dp);
            l->crossGravity = LinearLayout::Gravity::FILL;
            ctrlContent->layout = std::move(l);
        }
        buildControlsContent(ctrlContent, true);
    }
}
