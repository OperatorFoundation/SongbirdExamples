#ifndef FIELDRECORDER_UICONTROLLER_H
#define FIELDRECORDER_UICONTROLLER_H

#include <Arduino.h>
#include "Config.h"

enum Button { BTN_NONE, BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT };
enum ButtonEvent { EVT_NONE, EVT_PRESS, EVT_RELEASE, EVT_LONG_PRESS, EVT_EXTRA_LONG_PRESS };

struct ButtonState {
    bool pressed;
    uint32_t lastChangeTime;
    uint32_t pressStartTime;
    bool longPressTriggered;
    bool extraLongPressTriggered;
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
    bool isLongPressed(Button btn);
    bool isExtraLongPressed(Button btn);

    bool arePressed(Button btn1, Button btn2);
    bool isComboLongPressed(Button btn1, Button btn2);

    uint8_t getPressedButtons();
    void clearEvents();

private:
    ButtonState buttons[4];
    uint8_t justPressed;
    uint8_t justReleased;
    uint8_t longPressed;
    uint8_t extraLongPressed;

    const uint8_t buttonPins[4] = { BTN_UP_PIN, BTN_DOWN_PIN, BTN_LEFT_PIN, BTN_RIGHT_PIN };

    void updateButton(uint8_t index);
    bool readButtonRaw(uint8_t index);
    uint8_t buttonToMask(Button btn);
    uint8_t buttonToIndex(Button btn);
};

#endif
