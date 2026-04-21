/*
 * StorageManager.cpp - Stubbed storage manager for VoiceInterface
 */

#include "StorageManager.h"

StorageManager::StorageManager() : initialized(false)
{
}

void StorageManager::begin()
{
    initialized = true;
}

Settings StorageManager::getDefaultSettings()
{
    Settings defaults;
    defaults.micGain = DEFAULT_MIC_GAIN;
    defaults.playbackVolume = DEFAULT_PLAYBACK_VOLUME;
    return defaults;
}
