//
// Created by Mafalda on 9/2/25.
//

/*
 * StorageManager.h - Settings persistence for field recorder
 *
 * Manages EEPROM storage of user settings
 */

#ifndef FIELDRECORDER_STORAGEMANAGER_H
#define FIELDRECORDER_STORAGEMANAGER_H

#include <Arduino.h>
#include <EEPROM.h>
#include "Config.h"

class StorageManager
{
public:
    StorageManager();

    // Initialization
    void begin();

    // Load/Save Settings
    bool loadSettings(Settings& settings);
    bool saveSettings(const Settings& settings);

    // Factory Reset
    void factoryReset();

    // Get Defaults
    Settings getDefaultSettings();

private:
    // Calculate checksum for settings validation
    uint32_t calculateChecksum(const Settings& settings);

    // Validate Settings
    bool validateSettings(const Settings& settings);

    // Migrate settings from older versions (not implemented)
    void migrateSettings(Settings& settings);
};


#endif //FIELDRECORDER_STORAGEMANAGER_H