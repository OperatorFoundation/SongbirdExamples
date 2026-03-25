/*
 * StorageManager.h - Stubbed storage manager for VoiceInterface
 *
 * No EEPROM persistence - hardcoded settings
 */

#ifndef FIELDRECORDER_STORAGEMANAGER_H
#define FIELDRECORDER_STORAGEMANAGER_H

#include <Arduino.h>
#include "Config.h"

struct Settings {
    uint8_t version;
    uint8_t micGain;
    float playbackVolume;
    uint32_t sequenceNumber;
    uint32_t checksum;
};

class StorageManager
{
public:
    StorageManager();

    void begin();
    Settings getDefaultSettings();

private:
    bool initialized;
};

#endif //FIELDRECORDER_STORAGEMANAGER_H
