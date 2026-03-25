/*
 * LEDControl.cpp - Stubbed LED control for VoiceInterface
 */

#include "LEDControl.h"

LEDControl::LEDControl() : initialized(false)
{
}

void LEDControl::begin()
{
    initialized = true;
}

void LEDControl::update()
{
    // Stubbed - no LED output
}

void LEDControl::setBlueLED(bool)
{
    // Stubbed - no LED output
}

void LEDControl::setRecording(bool)
{
    // Stubbed - no LED output
}
