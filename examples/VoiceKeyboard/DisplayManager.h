/*
 * DisplayManager.h - Stubbed display manager for VoiceInterface
 *
 * Minimal stub - display functionality removed but interface preserved
 */

#ifndef VOICECHAT_DISPLAYMANAGER_H
#define VOICECHAT_DISPLAYMANAGER_H

#include <Arduino.h>
#include "Config.h"

class DisplayManager
{
public:
    DisplayManager();

    // Initialization
    bool begin();

    // Stubbed methods - no-op
    void showStartupScreen();
    void update();

private:
    bool initialized;
};

#endif // VOICECHAT_DISPLAYMANAGER_H
