/*
 * DisplayManager.cpp - Display implementation
 */

#include "DisplayManager.h"
#include <Arduino.h>
#include <Wire.h>

DisplayManager::DisplayManager() : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, -1)
{
    showingHint = false;
    hintStartTime = 0;
    lastHintTime = 0;
    manualGainStartTime = 0;
    hintShownThisSession = false;
    hintPhase = 0;
    lastUpdateTime = 0;
    needsUpdate = false;
}

bool DisplayManager::begin()
{
    // Initialize display
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) 
    {
        DEBUG_PRINTLN("SSD1306 allocation failed");
        return false;
    }

    display.setRotation(2);

    // Clear and configure display
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.display();

    DEBUG_PRINTLN("Display initialized");
    return true;
}

void DisplayManager::showIdleScreen(float hoursRemaining, const String& fileName, uint32_t fileIndex, uint32_t totalFiles, bool agcEnabled, uint8_t gain)
{
    display.clearDisplay();

    // Top line: Status and remaining time
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.print("Ready!");

    // if (hoursRemaining < 0.1) display.print("Ready - SD Full!");
    // else display.printf("Ready - %.1f hrs left", hoursRemaining);

    // Show AGC or gain status
    if (agcEnabled)
    {
        display.setCursor(100, 0);
        display.print("[AGC]");
    }
    else
    {
        display.setCursor(94, 0);
        display.printf("[+%ddB]", gain);
    }

    // Bottom line: File info or hint
    if (showingHint && !agcEnabled)
    {
        display.setCursor(0, 16);
        display.print("Hold L+R for AGC");
    }
    else if (totalFiles > 0)
    {
        display.setCursor(0, 16);

        // Truncate filename if too long
        String shortName = fileName;
        if (shortName.length() > 20)
        {
            shortName = shortName.substring(0, 17) + "...";
        }

        display.print(shortName);
        display.printf(" [%d/%d]", fileIndex, totalFiles);
    }
    else
    {
        display.setCursor(0, 16);
        display.print("No recordings");
    }

    needsUpdate = true;
}

void DisplayManager::showCountdownScreen(uint8_t secondsRemaining)
{
    display.clearDisplay();

    if (secondsRemaining > 0) {
        // Show large countdown number
        display.setTextSize(3);
        String numStr = String(secondsRemaining);

        // Center the number
        int16_t x = (SCREEN_WIDTH - (numStr.length() * 18)) / 2;
        int16_t y = 8;

        display.setCursor(x, y);
        display.print(secondsRemaining);

        // Show "Ready..." text above
        display.setTextSize(1);
        centerText("Ready...", 0);
    } else {
        // About to start
        display.setTextSize(2);
        centerText("GO!", 8);
    }

    needsUpdate = true;
}

void DisplayManager::showRecordingScreen(uint32_t elapsedSeconds, float audioLevel,
                                        const String& fileName, bool windCutEnabled) {
    display.clearDisplay();

    // Top line: Recording indicator and time
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("REC ");

    // Recording dot (blinking effect could be added)
    display.fillCircle(28, 3, 3, SSD1306_WHITE);

    // Time
    display.setCursor(36, 0);
    display.print(formatTime(elapsedSeconds));

    // Audio level meter
    drawLevelMeter(80, 0, METER_WIDTH, 7, audioLevel);

    // Bottom line: Filename and wind-cut indicator
    display.setCursor(0, 16);
    display.setTextSize(1);

    // Extract just the filename without path
    String shortName = fileName;
    int lastSlash = shortName.lastIndexOf('/');
    if (lastSlash >= 0) {
        shortName = shortName.substring(lastSlash + 1);
    }

    // Truncate if needed
    if (shortName.length() > 16) {
        shortName = shortName.substring(0, 13) + "...";
    }

    display.print(shortName);

    // Wind-cut indicator
    if (windCutEnabled) {
        display.setCursor(104, 16);
        display.print("[WC]");
    }

    needsUpdate = true;
}

void DisplayManager::showPlaybackScreen(uint32_t currentSeconds, uint32_t totalSeconds,
                                       const String& fileName, uint32_t fileIndex,
                                       uint32_t totalFiles) {
    display.clearDisplay();

    // Top line: Playback status and progress
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("PLAY ");

    // Play symbol
    display.fillTriangle(30, 1, 30, 7, 36, 4, SSD1306_WHITE);

    // Time progress
    display.setCursor(42, 0);
    display.print(formatTime(currentSeconds));
    display.print(" / ");
    display.print(formatTime(totalSeconds));

    // Bottom line: Filename and navigation
    display.setCursor(0, 16);

    // Extract just the filename
    String shortName = fileName;
    int lastSlash = shortName.lastIndexOf('/');
    if (lastSlash >= 0) {
        shortName = shortName.substring(lastSlash + 1);
    }

    // Truncate if needed
    if (shortName.length() > 16) {
        shortName = shortName.substring(0, 13) + "...";
    }

    display.print(shortName);
    display.printf(" [%d/%d]", fileIndex, totalFiles);

    needsUpdate = true;
}

void DisplayManager::showErrorScreen(ErrorType error) {
    display.clearDisplay();
    display.setTextSize(1);

    centerText("ERROR", 0);

    display.setTextSize(1);
    switch (error) {
        case ERROR_NO_SD_CARD:
            centerText("No SD Card", 12);
            centerText("Insert card", 22);
            break;
        case ERROR_SD_CARD_FULL:
            centerText("SD Card Full", 12);
            centerText("Free up space", 22);
            break;
        case ERROR_FILE_CREATE_FAILED:
            centerText("Cannot create file", 12);
            break;
        case ERROR_WRITE_FAILED:
            centerText("Write failed", 12);
            centerText("Check SD card", 22);
            break;
        default:
            centerText("Unknown error", 12);
            break;
    }

    needsUpdate = true;
}

void DisplayManager::showGainAdjustment(uint8_t gain) {
    // Temporary overlay showing gain adjustment
    display.fillRect(20, 8, 88, 16, SSD1306_BLACK);
    display.drawRect(20, 8, 88, 16, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(32, 12);
    display.printf("Gain: +%d dB", gain);

    needsUpdate = true;
}

void DisplayManager::showVolumeAdjustment(float volume) {
    // Temporary overlay showing volume adjustment
    display.fillRect(20, 8, 88, 16, SSD1306_BLACK);
    display.drawRect(20, 8, 88, 16, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(28, 12);
    display.printf("Volume: %d%%", (int)(volume * 100));

    needsUpdate = true;
}

void DisplayManager::showAGCEnabled() {
    display.clearDisplay();
    display.setTextSize(2);
    centerText("AGC ON", 8);
    needsUpdate = true;
}

void DisplayManager::showStartupScreen() {
    display.clearDisplay();
    display.setTextSize(1);
    centerText("Songbird", 0);
    centerText("Field Recorder", 10);
    centerText(FIRMWARE_VERSION, 22);
    needsUpdate = true;
}

void DisplayManager::showFactoryReset() {
    display.clearDisplay();
    display.setTextSize(1);
    centerText("Factory Reset", 8);
    centerText("Complete", 18);
    needsUpdate = true;
}

void DisplayManager::updateHintSystem(bool manualGainActive, bool agcJustEnabled) {
    // Reset hint system if AGC was just enabled
    if (agcJustEnabled) {
        hintShownThisSession = true;
        showingHint = false;
        hintStartTime = 0;
        return;
    }

    // Don't show hints if AGC is active or already shown this session
    if (!manualGainActive || hintShownThisSession) {
        showingHint = false;
        return;
    }

    // Track when manual gain started
    if (manualGainStartTime == 0) {
        manualGainStartTime = millis();
        lastHintTime = millis();
        hintPhase = 0;
    }

    uint32_t now = millis();
    uint32_t timeSinceManualGain = now - manualGainStartTime;

    // Determine hint phase
    if (timeSinceManualGain < HINT_PHASE1_DURATION) {
        hintPhase = 0;  // First 5 minutes: every 1 minute
    } else if (timeSinceManualGain < HINT_PHASE2_DURATION) {
        hintPhase = 1;  // Next 55 minutes: every 2 minutes
    } else {
        hintPhase = 2;  // After 1 hour: every 10 minutes
    }

    // Check if it's time to show hint
    uint32_t interval = getNextHintInterval();

    if (showingHint) {
        // Hint is currently showing
        if ((now - hintStartTime) > HINT_DISPLAY_MS) {
            showingHint = false;  // Hide after display duration
        }
    } else {
        // Check if we should show hint
        if ((now - lastHintTime) > interval) {
            showingHint = true;
            hintStartTime = now;
            lastHintTime = now;
        }
    }
}

uint32_t DisplayManager::getNextHintInterval() {
    switch (hintPhase) {
        case 0: return HINT_INTERVAL_1_MIN;   // 1 minute
        case 1: return HINT_INTERVAL_2_MIN;   // 2 minutes
        case 2: return HINT_INTERVAL_10_MIN;  // 10 minutes
        default: return HINT_INTERVAL_10_MIN;
    }
}

void DisplayManager::drawLevelMeter(uint8_t x, uint8_t y, uint8_t width, uint8_t height, float level) {
    // Draw meter outline
    display.drawRect(x, y, width, height, SSD1306_WHITE);

    // Fill based on level
    uint8_t fillWidth = (uint8_t)(level * (width - 2));
    if (fillWidth > 0) {
        display.fillRect(x + 1, y + 1, fillWidth, height - 2, SSD1306_WHITE);
    }
}

void DisplayManager::drawProgressBar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, float progress) {
    // Similar to level meter but for progress indication
    display.drawRect(x, y, width, height, SSD1306_WHITE);

    uint8_t fillWidth = (uint8_t)(progress * (width - 2));
    if (fillWidth > 0) {
        display.fillRect(x + 1, y + 1, fillWidth, height - 2, SSD1306_WHITE);
    }
}

void DisplayManager::centerText(const String& text, uint8_t y, uint8_t size) {
    display.setTextSize(size);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, y);
    display.print(text);
}

String DisplayManager::formatTime(uint32_t seconds) {
    uint16_t hours = seconds / 3600;
    uint16_t minutes = (seconds % 3600) / 60;
    uint16_t secs = seconds % 60;

    char buffer[10];
    if (hours > 0) {
        snprintf(buffer, sizeof(buffer), "%d:%02d:%02d", hours, minutes, secs);
    } else {
        snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, secs);
    }

    return String(buffer);
}

String DisplayManager::formatFileSize(uint32_t bytes) {
    if (bytes < 1024) {
        return String(bytes) + "B";
    } else if (bytes < 1048576) {
        return String(bytes / 1024) + "KB";
    } else {
        return String(bytes / 1048576) + "MB";
    }
}

void DisplayManager::clear()
{
    display.clearDisplay();
    needsUpdate = true;
}

void DisplayManager::update()
{
    if (needsUpdate) {
        display.display();
        needsUpdate = false;
        lastUpdateTime = millis();
    }
}

void DisplayManager::showTunerScreen(const char* mode, uint8_t stringNum,
                                    const char* detectedNote, const char* targetNote,
                                    float cents, bool inTune, bool hasSignal)
{
    display.clearDisplay();
    
    // Top line: Mode and string
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(mode);
    display.print(" - STR ");
    display.print(stringNum);
    
    if (hasSignal) {
        // Show detected note (large)
        display.setTextSize(2);
        display.setCursor(0, 10);
        display.print(detectedNote);
        
        // Show target note and direction arrow
        display.setTextSize(2);
        display.setCursor(50, 10);
        display.print("->");
        display.print(targetNote);
        
        // Show arrow indicator
        if (!inTune) {
            display.setTextSize(1);
            display.setCursor(118, 14);
            if (cents > TUNING_TOLERANCE_CENTS) {
                display.print("^");  // Sharp
            } else if (cents < -TUNING_TOLERANCE_CENTS) {
                display.print("v");  // Flat
            }
        }
        
        // Bottom line: Cents or "IN TUNE"
        display.setTextSize(1);
        display.setCursor(0, 24);
        if (inTune) {
            display.print("IN TUNE!");
        } else {
            char centsStr[16];
            snprintf(centsStr, sizeof(centsStr), "%+.0f cents", cents);
            display.print(centsStr);
        }
    } else {
        // No signal detected
        display.setTextSize(1);
        display.setCursor(20, 14);
        display.print("NO SIGNAL");
    }
    
    needsUpdate = true;
}

void DisplayManager::drawText(uint8_t x, uint8_t y, const char* text, uint8_t size)
{
    display.setTextSize(size);
    display.setCursor(x, y);
    display.print(text);
    needsUpdate = true;
}
