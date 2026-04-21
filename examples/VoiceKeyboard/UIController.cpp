/*
 * UIController.cpp - Button input handling for VoiceInterface
 */

#include "UIController.h"

UIController::UIController() {
    button = { false, 0, 0 };
    justPressed = false;
    justReleased = false;
}

void UIController::begin() {
    pinMode(BTN_UP_PIN, INPUT_PULLUP);
}

void UIController::update() {
    justPressed = false;
    justReleased = false;
    updateButton();
}

void UIController::updateButton() {
    bool current = readButtonRaw();
    uint32_t now = millis();

    // Debounce
    if (current != button.pressed && now - button.lastChangeTime > BUTTON_DEBOUNCE_MS) {
        button.lastChangeTime = now;
        if (current) {
            button.pressed = true;
            button.pressStartTime = now;
            justPressed = true;
        } else {
            button.pressed = false;
            justReleased = true;
        }
    }
}

bool UIController::readButtonRaw() {
    return !digitalRead(BTN_UP_PIN);
}

bool UIController::isPressed(Button) {
    return button.pressed;
}

bool UIController::wasJustPressed(Button) {
    return justPressed;
}

bool UIController::wasJustReleased(Button) {
    return justReleased;
}
