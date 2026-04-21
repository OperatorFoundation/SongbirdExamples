/*
 * UIController.h - Button input handling for VoiceInterface
 *
 * Handles button debouncing and press/release detection
 */

#ifndef FIELDRECORDER_UICONTROLLER_H
#define FIELDRECORDER_UICONTROLLER_H

#include <Arduino.h>
#include "Config.h"

enum Button { BTN_NONE, BTN_UP };
enum ButtonEvent { EVT_NONE, EVT_PRESS, EVT_RELEASE };

struct ButtonState {
    bool pressed;
    uint32_t lastChangeTime;
    uint32_t pressStartTime;
};

class UIController {
public:
    UIController();
    void begin();
    void update();

    // Button queries
    bool isPressed(Button btn);
    bool wasJustPressed(Button btn);
    bool wasJustReleased(Button btn);

private:
    ButtonState button;
    bool justPressed;
    bool justReleased;

    void updateButton();
    bool readButtonRaw();
};

#endif
