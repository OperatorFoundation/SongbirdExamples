//
// Created by Mafalda on 9/3/25.
//

/*
 * DisplayManager.h - OLED display management for field recorder
 *
 * Handles all display rendering, screen updates, and AGC hint system
 */

#ifndef FIELDRECORDER_DISPLAYMANAGER_H
#define FIELDRECORDER_DISPLAYMANAGER_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "Config.h"

class DisplayManager
{
   public:
    DisplayManager();

    // Initialization
    bool begin();

    // Screen updates for different states
    void showTunerScreen(const char* mode, uint8_t stringNum,
                    const char* detectedNote, const char* targetNote,
                    float cents, bool inTune, bool hasSignal);

    void drawText(uint8_t x, uint8_t y, const char* text, uint8_t size);

    void showIdleScreen(float hoursRemaining, const String& fileName,
                       uint32_t fileIndex, uint32_t totalFiles,
                       bool agcEnabled, uint8_t gain);

    void showCountdownScreen(uint8_t secondsRemaining);

    void showRecordingScreen(uint32_t elapsedSeconds, float audioLevel,
                           const String& fileName, bool windCutEnabled);

    void showPlaybackScreen(uint32_t currentSeconds, uint32_t totalSeconds,
                          const String& fileName, uint32_t fileIndex,
                          uint32_t totalFiles);

    void showErrorScreen(ErrorType error);

    // Special displays
    void showGainAdjustment(uint8_t gain);
    void showVolumeAdjustment(float volume);
    void showAGCEnabled();
    void showStartupScreen();
    void showFactoryReset();

    // AGC hint system
    void updateHintSystem(bool manualGainActive, bool agcJustEnabled);
    bool shouldShowHint() const { return showingHint; }

    // Utility
    void clear();
    void update();  // Actually display the buffer

private:
    Adafruit_SSD1306 display;

    // Hint system state
    bool showingHint;
    uint32_t hintStartTime;
    uint32_t lastHintTime;
    uint32_t manualGainStartTime;
    bool hintShownThisSession;
    uint8_t hintPhase;  // 0=phase1 (1min), 1=phase2 (2min), 2=phase3 (10min)

    // Display update control
    uint32_t lastUpdateTime;
    bool needsUpdate;

    // Helper functions
    void drawLevelMeter(uint8_t x, uint8_t y, uint8_t width, uint8_t height, float level);
    void drawProgressBar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, float progress);
    void centerText(const String& text, uint8_t y, uint8_t size = 1);
    String formatTime(uint32_t seconds);
    String formatFileSize(uint32_t bytes);

    // Hint system helpers
    uint32_t getNextHintInterval();
    void showAGCHint();
};


#endif //FIELDRECORDER_DISPLAYMANAGER_H
