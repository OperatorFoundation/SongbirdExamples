/*
 * LEDControl.cpp - LED implementation
 *
 * Created by Mafalda on 9/3/25.
 */

#include "LEDControl.h"

LEDControl::LEDControl()
{
    recordingActive = false;
    countdownActive = false;
    countdownFlashes = 0;
    lastFlashTime = 0;
    flashState = false;
    currentLevel = 0.0;
    clippingActive = false;
    clippingStartTime = 0;
}

void LEDControl::begin()
{
    // Configure LED pins
    pinMode(LED_BLUE_PIN, OUTPUT);
    pinMode(LED_PINK_PIN, OUTPUT);

    // Start with LEDs off
    digitalWrite(LED_BLUE_PIN, LOW);
    analogWrite(LED_PINK_PIN, 0);

    DEBUG_PRINTLN("LED Control initialized");
}

void LEDControl::setRecording(bool active)
{
    recordingActive = active;
    countdownActive = false;  // Cancel countdown if recording starts

    if (active)
    {
        setBlueLED(true);
    }
    else {
        setBlueLED(false);
        setPinkLED(0);  // Turn off level indicator
    }
}

void LEDControl::flashCountdown(uint8_t flashes)
{
    countdownActive = true;
    countdownFlashes = flashes;
    lastFlashTime = millis();
    flashState = true;
    setBlueLED(true);
}

void LEDControl::setAudioLevel(float level) {
    currentLevel = constrain(level, 0.0, 1.0);

    if (recordingActive && !clippingActive) updateAudioLED();
}

void LEDControl::setClipping(bool clipping)
{
    if (clipping && !clippingActive)
    {
        clippingActive = true;
        clippingStartTime = millis();
    }
    else if (!clipping)
    {
        clippingActive = false;
    }
}

void LEDControl::setBlueLED(bool on)
{
    digitalWrite(LED_BLUE_PIN, on ? HIGH : LOW);
}

void LEDControl::setPinkLED(uint8_t brightness)
{
    analogWrite(LED_PINK_PIN, brightness);
}

void LEDControl::update()
{
    uint32_t now = millis();

    // Handle countdown flashing
    if (countdownActive)
    {
        if ((now - lastFlashTime) > COUNTDOWN_FLASH_MS) {
            flashState = !flashState;
            setBlueLED(flashState);
            lastFlashTime = now;

            if (!flashState)
            {
                // Count completed flash cycles
                if (countdownFlashes > 0)  countdownFlashes--;

                if (countdownFlashes == 0)
                {
                    countdownActive = false;
                    setBlueLED(false);
                }
            }
        }
    }

    // Handle audio level/clipping display
    if (recordingActive)
    {
        if (clippingActive)
        {
            // Flash pink LED rapidly for clipping
            bool clipFlash = ((now / 100) % 2) == 0;
            setPinkLED(clipFlash ? 255 : 0);

            // Clear clipping after hold time
            if ((now - clippingStartTime) > CLIPPING_HOLD_MS) clippingActive = false;
        }
        else
        {
            // Normal level display
            updateAudioLED();
        }
    }
}

void LEDControl::updateAudioLED()
{
    // Map audio level to LED brightness
    // Use a curve for better visual response
    float adjustedLevel = currentLevel * currentLevel;  // Square for emphasis
    uint8_t brightness = (uint8_t)(adjustedLevel * 255);

    // Ensure minimum visibility when there's any signal
    if (currentLevel > 0.01 && brightness < 10) brightness = 10;

    setPinkLED(brightness);
}