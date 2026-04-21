/*
 * LEDControl.h - Stubbed LED control for VoiceInterface
 */

#ifndef FIELDRECORDER_LEDCONTROL_H
#define FIELDRECORDER_LEDCONTROL_H

#include <Arduino.h>
#include "Config.h"

class LEDControl
{
public:
    LEDControl();

    void begin();
    void update();
    void setBlueLED(bool on);
    void setRecording(bool active);

private:
    bool initialized;
};

#endif //FIELDRECORDER_LEDCONTROL_H
