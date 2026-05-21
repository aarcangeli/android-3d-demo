#include "ui_system.h"
#include <android/log.h>
#include <cstring>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"UISystem",__VA_ARGS__)

namespace ui {

UISystem::UISystem() = default;
UISystem::~UISystem() { shutdown(); }

void UISystem::init() {
    renderer_.init();
}

void UISystem::shutdown() {
    renderer_.shutdown();
}

void UISystem::resize(float w, float h) {
    screenW_ = w;
    screenH_ = h;
    root_.setBounds(0, 0, w, h);
}

void UISystem::draw() {
    drainQueue();
    renderer_.begin(screenW_, screenH_);
    root_.draw(renderer_);
    renderer_.end();
}

// ── Input queue ───────────────────────────────────────────────────────────────

void UISystem::enqueue(const InputEvent& e) {
    // Spinlock
    while (__sync_lock_test_and_set(&queueLock_, 1)) {}
    int next = (eventTail_ + 1) % MAX_EVENTS;
    if (next != eventHead_) {
        eventQueue_[eventTail_] = e;
        eventTail_ = next;
    } else {
        LOGE("Input queue full, dropping event");
    }
    __sync_lock_release(&queueLock_);
}

void UISystem::drainQueue() {
    while (__sync_lock_test_and_set(&queueLock_, 1)) {}
    int head = eventHead_, tail = eventTail_;
    __sync_lock_release(&queueLock_);

    while (head != tail) {
        dispatchInput(eventQueue_[head]);
        head = (head + 1) % MAX_EVENTS;
    }

    while (__sync_lock_test_and_set(&queueLock_, 1)) {}
    eventHead_ = head;
    __sync_lock_release(&queueLock_);
}

void UISystem::dispatchInput(const InputEvent& e) {
    root_.onInput(e);
}

// ── Public input API ──────────────────────────────────────────────────────────

void UISystem::onTouchDown(int id, float x, float y) {
    enqueue({InputType::TOUCH_DOWN, x, y, id});
}
void UISystem::onTouchMove(int id, float x, float y) {
    enqueue({InputType::TOUCH_MOVE, x, y, id});
}
void UISystem::onTouchUp(int id, float x, float y) {
    enqueue({InputType::TOUCH_UP, x, y, id});
}
void UISystem::onTouchCancel(int id, float x, float y) {
    enqueue({InputType::TOUCH_CANCEL, x, y, id});
}
void UISystem::onKey(int keyCode, int unicode, bool down) {
    InputEvent e{};
    e.type    = down ? InputType::KEY_DOWN : InputType::KEY_UP;
    e.keyCode = keyCode;
    e.unicode = unicode;
    enqueue(e);
}

} // namespace ui
