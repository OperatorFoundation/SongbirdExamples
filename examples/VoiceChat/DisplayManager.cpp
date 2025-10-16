/*
 * DisplayManager.cpp - OLED display management for VoiceChat
 */

#include "DisplayManager.h"

DisplayManager::DisplayManager() :
    display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, -1),
    lastUpdateTime(0),
    needsUpdate(false)
{
}

bool DisplayManager::begin()
{
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
    {
        return false;
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    return true;
}

void DisplayManager::showStartupScreen()
{
    display.clearDisplay();
    display.setTextSize(2);
    centerText("VoiceChat", 4);
    display.setTextSize(1);
    centerText("v" FIRMWARE_VERSION, 24);
}

void DisplayManager::showIdleScreen(uint8_t channelNum, bool connected, uint8_t queuedMessages)
{
    display.clearDisplay();
    display.setTextSize(1);
    
    // Channel name at top
    display.setCursor(0, 0);
    display.print("Channel: ");
    display.print(getChannelName(channelNum));
    
    // Status in middle
    display.setCursor(0, 12);
    if (!connected)
    {
        display.print("NO CONNECTION");
    }
    else if (queuedMessages > 0)
    {
        display.print("Queue: ");
        display.print(queuedMessages);
        display.print(" msg");
        if (queuedMessages > 1) display.print("s");
    }
    else
    {
        display.print("Status: IDLE");
    }
    
    // Connection indicator at bottom
    display.setCursor(0, 24);
    display.print("Connected: ");
    display.print(connected ? "Yes" : "No");
}

void DisplayManager::showRecordingScreen(uint8_t channelNum, uint32_t elapsedSeconds)
{
    display.clearDisplay();
    display.setTextSize(1);
    
    // Channel at top
    display.setCursor(0, 0);
    display.print("Channel: ");
    display.print(getChannelName(channelNum));
    
    // RECORDING indicator
    display.setTextSize(2);
    display.setCursor(0, 10);
    display.print("REC ");
    display.print(formatTime(elapsedSeconds));
    
    // Progress bar at bottom
    display.setTextSize(1);
    uint8_t barWidth = min((int)SCREEN_WIDTH, (int)(elapsedSeconds * 4));  // 4 pixels per second
    display.fillRect(0, 28, barWidth, 4, SSD1306_WHITE);
}

void DisplayManager::showPlayingScreen(uint8_t channelNum, uint32_t currentSeconds, 
                                      uint32_t totalSeconds, const String& sender)
{
    display.clearDisplay();
    display.setTextSize(1);
    
    // Channel at top
    display.setCursor(0, 0);
    display.print("Channel: ");
    display.print(getChannelName(channelNum));
    
    // Sender
    display.setCursor(0, 10);
    display.print("From: ");
    display.print(sender);
    
    // Time
    display.setCursor(0, 20);
    display.print("PLAYING ");
    display.print(formatTime(currentSeconds));
    display.print("/");
    display.print(formatTime(totalSeconds));
    
    // Progress bar
    if (totalSeconds > 0)
    {
        float progress = (float)currentSeconds / (float)totalSeconds;
        drawProgressBar(0, 28, SCREEN_WIDTH, 4, progress);
    }
}

void DisplayManager::showChannelSwitch(uint8_t channelNum, uint8_t queuedMessages)
{
    display.clearDisplay();
    
    // Large channel name in center
    display.setTextSize(2);
    String channelText = ">> " + getChannelName(channelNum) + " <<";
    centerText(channelText, 8);
    
    // Queue info below
    display.setTextSize(1);
    if (queuedMessages > 0)
    {
        String queueText = String(queuedMessages) + " unread";
        centerText(queueText, 24);
    }
}

void DisplayManager::showDisconnected()
{
    display.clearDisplay();
    display.setTextSize(1);
    
    centerText("NO CONNECTION", 8);
    centerText("Check USB cable", 20);
}

void DisplayManager::showErrorScreen(ErrorType error)
{
    display.clearDisplay();
    display.setTextSize(1);
    
    centerText("ERROR", 0);
    
    display.setCursor(0, 12);
    switch (error)
    {
        case ERROR_NO_SD_CARD:
            centerText("No SD card", 12);
            break;
        case ERROR_SD_CARD_FULL:
            centerText("SD card full", 12);
            break;
        case ERROR_FILE_CREATE_FAILED:
            centerText("File create failed", 12);
            break;
        default:
            centerText("Unknown error", 12);
            break;
    }
}

void DisplayManager::clear()
{
    display.clearDisplay();
}

void DisplayManager::update()
{
    display.display();
    lastUpdateTime = millis();
}

// =============================================================================
// Helper Functions
// =============================================================================

void DisplayManager::drawProgressBar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, float progress)
{
    // Draw outline
    display.drawRect(x, y, width, height, SSD1306_WHITE);
    
    // Fill progress
    uint8_t fillWidth = (uint8_t)(progress * (width - 2));
    if (fillWidth > 0)
    {
        display.fillRect(x + 1, y + 1, fillWidth, height - 2, SSD1306_WHITE);
    }
}

void DisplayManager::centerText(const String& text, uint8_t y, uint8_t size)
{
    display.setTextSize(size);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, y);
    display.print(text);
}

String DisplayManager::formatTime(uint32_t seconds)
{
    uint8_t mins = seconds / 60;
    uint8_t secs = seconds % 60;
    
    String result = "";
    if (mins < 10) result += "0";
    result += String(mins);
    result += ":";
    if (secs < 10) result += "0";
    result += String(secs);
    
    return result;
}

String DisplayManager::getChannelName(uint8_t channelNum)
{
    // Convert 0-indexed to 1-indexed for display
    return "#" + String(channelNum + 1);
}