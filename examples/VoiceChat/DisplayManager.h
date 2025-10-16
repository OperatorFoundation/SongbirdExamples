/*
 * DisplayManager.h - OLED display management for VoiceChat
 *
 * Handles all display rendering and screen updates
 */

#ifndef VOICECHAT_DISPLAYMANAGER_H
#define VOICECHAT_DISPLAYMANAGER_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "Config.h"

class DisplayManager
{
public:
    DisplayManager();

    // Initialization
    bool begin();

    // VoiceChat screen updates
    void showIdleScreen(uint8_t channelNum, bool connected, uint8_t queuedMessages);
    void showRecordingScreen(uint8_t channelNum, uint32_t elapsedSeconds);
    void showPlayingScreen(uint8_t channelNum, uint32_t currentSeconds, 
                          uint32_t totalSeconds, const String& sender);
    void showChannelSwitch(uint8_t channelNum, uint8_t queuedMessages);
    void showDisconnected();
    void showErrorScreen(ErrorType error);

    // Special displays
    void showStartupScreen();

    // Utility
    void clear();
    void update();  // Actually display the buffer

private:
    Adafruit_SSD1306 display;

    // Display update control
    uint32_t lastUpdateTime;
    bool needsUpdate;

    // Helper functions
    void drawProgressBar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, float progress);
    void centerText(const String& text, uint8_t y, uint8_t size = 1);
    String formatTime(uint32_t seconds);
    String getChannelName(uint8_t channelNum);
};

#endif // VOICECHAT_DISPLAYMANAGER_H