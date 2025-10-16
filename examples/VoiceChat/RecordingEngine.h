/*
* RecordingEngine.h - Recording management for field recorder
 *
 * Handles WAV file creation, audio data writing, and file management
 */

#ifndef RECORDING_ENGINE_H
#define RECORDING_ENGINE_H

#include <Arduino.h>
#include <SD.h>
#include <WAVMaker.h>
#include <Audio.h>

#include "Config.h"

class RecordingEngine {
public:
    RecordingEngine();

    // Initialization
    bool begin();

    // Recording control
    bool startRecording();
    bool processRecording(AudioRecordQueue* queue);
    bool stopRecording();
    bool isRecording() const { return recording; }

    // File management
    String getCurrentFileName() const { return currentFileName; }
    String generateNextFilename();
    uint32_t scanExistingFiles();  // Returns count of existing files
    uint32_t getNextSequenceNumber() { return nextSequenceNumber; }

    // Status
    uint32_t getRecordingDuration() const;  // In seconds
    uint32_t getRecordingSize() const;      // In bytes
    float getAvailableHours() const;        // Remaining SD card space in hours
    uint32_t getFileCount() const { return fileCount; }

    // Error handling
    bool hasError() const { return lastError != ERROR_NONE; }
    ErrorType getLastError() const { return lastError; }
    void clearError() { lastError = ERROR_NONE; }

private:
    // State
    bool recording;
    bool sdCardPresent;
    ErrorType lastError;

    // Current recording
    WAVMaker wavMaker;
    String currentFileName;
    uint32_t recordingStartTime;
    uint32_t lastAutoSaveTime;
    uint32_t bytesWritten;

    // File management
    uint32_t fileCount;
    uint32_t nextSequenceNumber;

    // Helper functions
    bool checkSDCard();
    bool createRecordingsDirectory();
    uint32_t findHighestSequenceNumber();
    void setNextSequenceNumber(uint32_t seq);
    uint64_t getSDCardFreeSpace() const;
};

#endif // RECORDING_ENGINE_H