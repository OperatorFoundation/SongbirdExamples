//
// Created by Mafalda on 8/30/25.
//

/*
 * RecordingEngine.cpp - Recording implementation
 */

#include "RecordingEngine.h"
#include "Config.h"
#include <TimeLib.h>
#include <WAVMaker.h>

RecordingEngine::RecordingEngine()
{
    recording = false;
    sdCardPresent = false;
    lastError = ERROR_NONE;
    recordingStartTime = 0;
    lastAutoSaveTime = 0;
    bytesWritten = 0;
    fileCount = 0;
    nextSequenceNumber = 1; // Loads from EEPROM
    currentFileName = "";
}

bool RecordingEngine::begin()
{
    // Initialize the SD card
    if (!checkSDCard()) {
        // FIXME: checkSDCard should set the error so that the specific issue can be reported
        lastError = ERROR_NO_SD_CARD;
        return false;
    }

    // Create recording directory if it doesn't exist
    if (!createRecordingsDirectory()) {
        DEBUG_PRINTLN("Warning: Could not create recordings directory");
    }

    // Scan existing files to get the count
    fileCount = scanExistingFiles();
    DEBUG_PRINTF("Found %d existing recordings\n", fileCount);

    // Find the highest sequence number from existing filenames
    // This helps if EEPROM was reset but files still exist
    uint32_t highestSequenceNumber = findHighestSequenceNumber();

    // nextSequence number is set from EEPROM in main
    // But let's make sure it's higher than the highestSequenceNumber we just found
    if (nextSequenceNumber <= highestSequenceNumber) {
        nextSequenceNumber = highestSequenceNumber + 1;
        DEBUG_PRINTF("Adjusted sequence number to %d based on existing files\n", nextSequenceNumber);
    }

    return true;
}

bool RecordingEngine::checkSDCard()
{
    // Check for physical card presence if detect pin is available
    if (digitalRead(SDCARD_DETECT_PIN) == HIGH)
    {
        sdCardPresent = false;
        return false;
    }

    // Try to initialize the SD card
    if (!SD.begin(SDCARD_CS_PIN)) {
        sdCardPresent = false;
        return false;
    }

    sdCardPresent = true;
    return true;
}

bool RecordingEngine::createRecordingsDirectory()
{
    if (!sdCardPresent)
    {
        return false;
    }

    // Check if the directory exists
    if (SD.exists(RECORDINGS_DIR))
    {
        return true;
    }

    // Create the directory
    return SD.mkdir(RECORDINGS_DIR);
}

String RecordingEngine::generateNextFilename()
{
    // Format: REC_NNNNN.WAV (sequential)
    char filename[MAX_FILENAME_LENGTH];
    snprintf(filename, sizeof(filename), "%s/%s%05lu%s", RECORDINGS_DIR, FILE_PREFIX, nextSequenceNumber, FILE_EXTENSION);

    return String(filename);
}

uint32_t RecordingEngine::findHighestSequenceNumber()
{
    uint32_t highestSequenceNumber = 0;

    File recordingsDirectory = SD.open(RECORDINGS_DIR);
    if (!recordingsDirectory)
    {
        return 0;
    }

    while (true)
    {
        File entry = recordingsDirectory.openNextFile();

        if (!entry)
        {
            break;
        }

        String entryName = entry.name();
        entry.close();

        // Check if the file matches our pattern
        if (entryName.startsWith(FILE_PREFIX) && entryName.endsWith(FILE_EXTENSION))
        {
            // Extract the sequence number
            int prefixLength = strlen(FILE_PREFIX);
            int extensionPosition = entryName.lastIndexOf('.');

            if (extensionPosition > prefixLength)
            {
                String numberString = entryName.substring(prefixLength, extensionPosition);
                uint32_t number = numberString.toInt();

                if (number > highestSequenceNumber && number < MAX_SEQUENCE_NUMBER)
                {
                    highestSequenceNumber = number;
                }
            }
        }
    }

    recordingsDirectory.close();
    return highestSequenceNumber;
}

void RecordingEngine::setNextSequenceNumber(uint32_t seq)
{
    nextSequenceNumber = seq;

    if (nextSequenceNumber > MAX_SEQUENCE_NUMBER)
    {
        nextSequenceNumber = 1; // Wrap
    }
}

bool RecordingEngine::startRecording()
{
    if (recording)
    {
        return false;   // We're already recording
    }

    // Check the SD card
    if (!checkSDCard()) {
        lastError = ERROR_NO_SD_CARD;
        return false;
    }

    // Check available space (we need at least 10MB)
    if (getSDCardFreeSpace() < 10 * 1024 * 1024) {
        lastError = ERROR_SD_CARD_FULL;
        return false;
    }

    // Generate filename
    currentFileName = generateNextFilename();
    DEBUG_PRINTF("Starting recording to %s\n", currentFileName.c_str());

    // Configure WAVMaker for field recording
    wavMaker = WAVMaker::configure(currentFileName.c_str())
        .sampleRate(RATE_44100)          // Teensy Audio Library native rate
        .channels(MONO)                  // Mono for voice recording
        .bitsPerSample(BITS_16)
        .bufferSize(WAV_BUFFER_SIZE)     // Large buffer for reliability
        .overwriteExisting(false);       // Never overwrite

    // Reset counters
    recordingStartTime = millis();
    lastAutoSaveTime = millis();
    bytesWritten = 0;
    recording = true;

    return true;
}

bool RecordingEngine::processRecording(AudioRecordQueue* queue)
{
    if (!recording || !queue)
    {
        return false;
    }

    bool dataWritten = false;

    // Process all available audio blocks
    while (queue->available() > 0)
    {
        int16_t* buffer = queue->readBuffer();

        // Write audio data
        if (wavMaker.write(buffer, AUDIO_BLOCK_SAMPLES)) {
            bytesWritten += AUDIO_BLOCK_SAMPLES * sizeof(int16_t);
            dataWritten = true;
        }
        else
        {
            // Write failed - try to save what we have
            lastError = ERROR_WRITE_FAILED;
            stopRecording();
            queue->freeBuffer();
            return false;
        }

        queue->freeBuffer();
    }

    // Auto-save periodically
    if ((millis() - lastAutoSaveTime) > AUTO_SAVE_INTERVAL_MS) {
        wavMaker.flush();
        lastAutoSaveTime = millis();
        DEBUG_PRINTLN("Auto-saved recording");
    }

    return dataWritten;
}

bool RecordingEngine::stopRecording()
{
    if (!recording)
    {
        return false;
    }

    recording = false;

    // Close the WAV file (header will be automatically updated with the final size)
    bool success = wavMaker.close();

    if (success) {
        fileCount++;
        DEBUG_PRINTF("Recording saved: %s (%.1f seconds)\n", currentFileName.c_str(), getRecordingDuration() / 1000.0);
    }
    else
    {
        lastError = ERROR_WRITE_FAILED;
        DEBUG_PRINTLN("Failed to close WAV file");
    }

    return success;
}

uint32_t RecordingEngine::getRecordingSize() const {
    return bytesWritten;
}

uint32_t RecordingEngine::getRecordingDuration() const
{
    if (recording)
    {
        return millis() - recordingStartTime;
    }

    return 0;
}

float RecordingEngine::getAvailableHours() const
{
    uint64_t freeSpace = getSDCardFreeSpace();

    // Calculate based on the recording format
    // 44,100 samples/second × 2 bytes/sample (16-bit = 2 bytes) × 1 channel = 88,200 bytes/second
    uint32_t bytesPerSecond = RECORDING_SAMPLE_RATE * 2;  // 44100 * 2 = 88,200
    uint32_t bytesPerHour = bytesPerSecond * 3600;        // 88,200 * 3600 = 317,520,000

    return static_cast<float>(freeSpace) / static_cast<float>(bytesPerHour);
}

uint64_t RecordingEngine::getSDCardFreeSpace() const
{
    if (!sdCardPresent)
    {
        return 0;
    }

    // FIXME: The below doesn't work. Returning a dummy value of 1GB instead
    // TODO: Consider switching to using the SDFat library directly, or submitting a request for volume to be made public
    // // For Teensy using SdFat library (underlying SD library)
    // uint32_t freeClusterCount = SD.vol()->freeClusterCount();
    // uint32_t blocksPerCluster = SD.vol()->blocksPerCluster();
    //
    // // Each block is 512 bytes
    // uint64_t freeSpace = (uint64_t)freeClusterCount *
    //                      (uint64_t)blocksPerCluster * 512ULL;
    //
    // return freeSpace;

    return 1073741824;
}



uint32_t RecordingEngine::scanExistingFiles()
{
    uint32_t count = 0;

    File recordingsDirectory = SD.open(RECORDINGS_DIR);
    if (!recordingsDirectory)
    {
        return 0;
    }

    while (count < MAX_FILES_TO_SCAN)
    {
        File entry = recordingsDirectory.openNextFile();
        if (!entry) {
            break;
        }

        String entryName = entry.name();
        entry.close();

        // Only count WAV files with the expected naming pattern
        if (entryName.startsWith(FILE_PREFIX) && entryName.endsWith(FILE_EXTENSION))
        {
            count++;
        }
    }

    recordingsDirectory.close();
    return count;
}