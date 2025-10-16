#include "UIController.h"

UIController::UIController() {
    justPressed = justReleased = longPressed = extraLongPressed = 0;
    for (int i = 0; i < 4; i++) {
        buttons[i] = { false, 0, 0, false, false };
    }
}

void UIController::begin() {
    for (uint8_t i = 0; i < 4; i++) pinMode(buttonPins[i], INPUT_PULLUP);
}

void UIController::update() {
    justPressed = justReleased = longPressed = extraLongPressed = 0;
    for (uint8_t i = 0; i < 4; i++) updateButton(i);
}

void UIController::updateButton(uint8_t i) {
    ButtonState &btn = buttons[i];
    bool current = readButtonRaw(i);
    uint32_t now = millis();

    // Debounce
    if ((current != btn.pressed) && (now - btn.lastChangeTime > BUTTON_DEBOUNCE_MS)) {
        btn.lastChangeTime = now;
        if (current) {
            btn.pressed = true;
            btn.pressStartTime = now;
            btn.longPressTriggered = btn.extraLongPressTriggered = false;
            justPressed |= (1 << i);
        } else {
            btn.pressed = false;
            justReleased |= (1 << i);
        }
    }

    // Long press
    if (btn.pressed) {
        if (!btn.longPressTriggered && (now - btn.pressStartTime > LONG_PRESS_MS)) {
            btn.longPressTriggered = true;
            longPressed |= (1 << i);
        }
        if (!btn.extraLongPressTriggered && (now - btn.pressStartTime > EXTRA_LONG_PRESS_MS)) {
            btn.extraLongPressTriggered = true;
            extraLongPressed |= (1 << i);
        }
    }
}

bool UIController::readButtonRaw(uint8_t i) { return !digitalRead(buttonPins[i]); }

uint8_t UIController::buttonToMask(Button btn) { return 1 << buttonToIndex(btn); }

uint8_t UIController::buttonToIndex(Button btn) {
    switch (btn) {
        case BTN_UP: return 0;
        case BTN_DOWN: return 1;
        case BTN_LEFT: return 2;
        case BTN_RIGHT: return 3;
        default: return 0xFF;
    }
}

bool UIController::isPressed(Button btn) { return buttons[buttonToIndex(btn)].pressed; }
bool UIController::wasJustPressed(Button btn) { return justPressed & buttonToMask(btn); }
bool UIController::wasJustReleased(Button btn) { return justReleased & buttonToMask(btn); }
bool UIController::isLongPressed(Button btn) { return longPressed & buttonToMask(btn); }
bool UIController::isExtraLongPressed(Button btn) { return extraLongPressed & buttonToMask(btn); }

bool UIController::arePressed(Button btn1, Button btn2) {
    return isPressed(btn1) && isPressed(btn2);
}

bool UIController::isComboLongPressed(Button btn1, Button btn2) {
    if (!arePressed(btn1, btn2)) return false;
    uint32_t now = millis();
    uint32_t comboStart = max(buttons[buttonToIndex(btn1)].pressStartTime,
                              buttons[buttonToIndex(btn2)].pressStartTime);
    return (now - comboStart) > EXTRA_LONG_PRESS_MS;
}

uint8_t UIController::getPressedButtons() {
    uint8_t mask = 0;
    for (int i = 0; i < 4; i++) if (buttons[i].pressed) mask |= (1 << i);
    return mask;
}

void UIController::clearEvents() { justPressed = justReleased = longPressed = extraLongPressed = 0; }
