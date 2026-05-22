#pragma once

namespace ui {

enum class InputType {
    TOUCH_DOWN,
    TOUCH_MOVE,
    TOUCH_UP,
    TOUCH_CANCEL,
    KEY_DOWN,
    KEY_UP,
};

struct InputEvent {
    InputType type;
    // touch
    float x        = 0;
    float y        = 0;
    int   pointerId = 0;
    // key
    int   keyCode  = 0;
    int   unicode  = 0;

    bool isTouchEvent() const {
        return type == InputType::TOUCH_DOWN ||
               type == InputType::TOUCH_MOVE ||
               type == InputType::TOUCH_UP   ||
               type == InputType::TOUCH_CANCEL;
    }
    bool isKeyEvent() const {
        return type == InputType::KEY_DOWN || type == InputType::KEY_UP;
    }
};

} // namespace ui
