/*
 * LEDControl.h - LED indicator control for field recorder
 *
 * Manages recording indicators and audio level display
 *
 * Created by Mafalda on 9/3/25.
 */

#ifndef FIELDRECORDER_LEDCONTROL_H
#define FIELDRECORDER_LEDCONTROL_H

#include <Arduino.h>
#include "Config.h"

class LEDControl
{
public:
    LEDControl();

    // Initialization
    void begin();

    // Recording indicators
    void setRecording(bool active);
    void flashCountdown(uint8_t flashes);

    // Audio level indicator
    void setAudioLevel(float level);  // 0.0-1.0
    void setClipping(bool clipping);

    // Direct control
    void setBlueLED(bool on);
    void setPinkLED(uint8_t brightness);  // 0-255

    // Update - call this in loop for animations
    void update();

private:
    // LED states
    bool recordingActive;
    bool countdownActive;
    uint8_t countdownFlashes;
    uint32_t lastFlashTime;
    bool flashState;

    // Audio level
    float currentLevel;
    bool clippingActive;
    uint32_t clippingStartTime;

    // PWM for pink LED brightness
    void updateAudioLED();
};


#endif //FIELDRECORDER_LEDCONTROL_H