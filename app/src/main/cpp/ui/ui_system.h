#pragma once
#include "widget.h"
#include "ui_renderer.h"

namespace ui {

// Top-level manager: owns the root Container and routes input.
class UISystem {
public:
    UISystem();
    ~UISystem();

    void init();
    void shutdown();

    // Called on GL thread.
    void resize(float w, float h);
    void draw();

    // Called from Android input thread (or UI thread); thread-safe via a queue.
    void onTouchDown  (int id, float x, float y);
    void onTouchMove  (int id, float x, float y);
    void onTouchUp    (int id, float x, float y);
    void onTouchCancel(int id, float x, float y);
    void onKey        (int keyCode, int unicode, bool down);

    Container& root() { return root_; }
    UIRenderer& renderer() { return renderer_; }

private:
    void dispatchInput(const InputEvent& e);

    UIRenderer renderer_;
    Container  root_;

    // Pending events to be consumed on the GL thread during draw().
    static constexpr int MAX_EVENTS = 64;
    InputEvent eventQueue_[MAX_EVENTS];
    int        eventHead_ = 0, eventTail_ = 0;
    // Minimal spinlock via atomic.
    volatile int queueLock_ = 0;

    void enqueue(const InputEvent& e);
    void drainQueue();

    float screenW_ = 0, screenH_ = 0;
};

} // namespace ui
