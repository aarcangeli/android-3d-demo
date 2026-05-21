#pragma once
#include "ui/ui_system.h"
#include <string>
#include <functional>

// Builds the demo UI layout and wires up the callbacks.
// glRenderCallback is called (from draw()) to render the spinning triangle
// inside the central GLWidget.
void buildDemoScene(ui::UISystem& sys,
                    std::function<void(float x, float y, float w, float h)> glRenderCallback,
                    ui::Label*& outFpsLabel,
                    ui::Label*& outAngleLabel,
                    ui::Label*& outStatusLabel);
