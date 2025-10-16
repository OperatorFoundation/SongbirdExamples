//
// Created by Mafalda on 9/2/25.
//

#include "StorageManager.h"

StorageManager::StorageManager() {

}

void StorageManager::begin()
{
    // Initialize EEPROM
    EEPROM.begin();

    // Check if we need to initialize with defaults
    Settings test;
    if (!loadSettings(test))
    {
        DEBUG_PRINTLN("No valid settings found, initializing defaults");
        Settings defaults = getDefaultSettings();
        saveSettings(defaults);
    }

    DEBUG_PRINTLN("Storage Manager initialized");
}

bool StorageManager::loadSettings(Settings& settings)
{
    // Read settings from EEPROM
    EEPROM.get(EEPROM_SETTINGS_ADDR, settings);

    // Validate the settings
    if (!validateSettings(settings))
    {
        DEBUG_PRINTLN("Settings validation failed");
        return false;
    }

    DEBUG_PRINTLN("Settings loaded successfully");
    DEBUG_PRINTF("  AGC: %s\n", settings.agcEnabled ? "ON" : "OFF");
    DEBUG_PRINTF("  Gain: %d\n", settings.micGain);
    DEBUG_PRINTF("  Volume: %.2f\n", settings.playbackVolume);
    DEBUG_PRINTF("  Wind-cut: %s\n", settings.windCutEnabled ? "ON" : "OFF");

    return true;
}

bool StorageManager::saveSettings(const Settings& settings)
{
    // Calculate checksum
    Settings toSave = settings;
    toSave.checksum = calculateChecksum(toSave);

    // Write settings to EEPROM
    EEPROM.put(EEPROM_SETTINGS_ADDR, toSave);

    DEBUG_PRINTLN("Settings saved");
    return true;
}

void StorageManager::factoryReset()
{
    Settings defaults = getDefaultSettings();
    saveSettings(defaults);
    DEBUG_PRINTLN("Factory reset complete");
}

Settings StorageManager::getDefaultSettings()
{
    Settings defaults;

    defaults.version = SETTINGS_VERSION;
    defaults.micGain = DEFAULT_MIC_GAIN;
    defaults.playbackVolume = DEFAULT_PLAYBACK_VOLUME;
    defaults.agcEnabled = true;
    defaults.windCutEnabled = false;
    defaults.sequenceNumber = 1;
    defaults.checksum = 0;  // Calculated when saved

    return defaults;
}

uint32_t StorageManager::calculateChecksum(const Settings& settings)
{
    // XOR all bytes except the checksum field
    uint32_t checksum = 0;
    const uint8_t* data = (const uint8_t*)&settings;
    size_t checksumOffset = offsetof(Settings, checksum);

    for (size_t i = 0; i < checksumOffset; i++)
    {
        checksum ^= data[i];
        checksum = (checksum << 1) | (checksum >> 31);  // Rotate left
    }

    return checksum;
}

bool StorageManager::validateSettings(const Settings& settings)
{
    // Check the version
    if (settings.version == 0xFF)
    {
        // When EEPROM memory is erased or uninitialized, all bits are typically set to `1`,
        // which means all bytes contain the value `0xFF` (255 in decimal, 11111111 in binary)
        return false;
    }

    // Settings should be reasonable
    if (settings.micGain > MAX_MIC_GAIN)
    {
        return false;
    }

    if (settings.playbackVolume < 0.0 || settings.playbackVolume > 1.0) {
        return false;
    }

    // Checksum
    uint32_t calculatedChecksum = calculateChecksum(settings);
    if (calculatedChecksum != settings.checksum)
    {
        DEBUG_PRINTF("Checksum mismatch: calc=%08X, stored=%08X\n", calculatedChecksum, settings.checksum);
        return false;
    }

    return true;
}

void StorageManager::migrateSettings(Settings& settings) {
    // Handle settings migration from older versions
    // For now, just update the version number

    if (settings.version < SETTINGS_VERSION) {
        DEBUG_PRINTF("Migrating settings from v%d to v%d\n",
                    settings.version, SETTINGS_VERSION);

        // Future migrations would go here
        // For example:
        // if (settings.version < 2) {
        //     // Migrate from v1 to v2
        // }

        settings.version = SETTINGS_VERSION;
    }
}