/*
 * DisplayManager.cpp - Stubbed display manager for VoiceInterface
 */

#include "DisplayManager.h"

DisplayManager::DisplayManager() : initialized(false)
{
}

bool DisplayManager::begin()
{
    initialized = true;
    return true;
}

void DisplayManager::showStartupScreen()
{
    // Stubbed - no display output
}

void DisplayManager::update()
{
    // Stubbed - no display output
}
