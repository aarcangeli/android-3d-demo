#pragma once
#include "ui/ui_system.h"
#include "scene_renderer.h"
#include <string>
#include <functional>

void buildDemoScene(ui::UISystem& sys,
                    float dp,
                    float screenW,
                    int apiLevel,
                    scene::SceneRenderer& scene,
                    ui::GLWidget*& outGLWidget,
                    ui::Label*& outAngleLabel,
                    ui::Label*& outStatusLabel);
