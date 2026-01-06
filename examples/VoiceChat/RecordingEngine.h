/*
 * RecordingEngine.h - Recording management for VoiceChat
 *
 * Handles Opus-compressed audio recording and file management
 */

#ifndef RECORDING_ENGINE_H
#define RECORDING_ENGINE_H

#include <Arduino.h>
#include <SD.h>
#include <Audio.h>
#include "Config.h"
#include "OpusCodec.h"

// Opus file format: simple packet container
// File structure: [packet_size:uint16][packet_data:bytes]...
// Each packet is prefixed with its size for easy reading

class RecordingEngine
{
public:
    RecordingEngine();

    // Initialization
    bool begin();

    // Recording control
    bool startRecording(uint8_t channel);
    bool processRecording(AudioRecordQueue* queue);
    bool stopRecording();
    bool isRecording() const { return recording; }

    // File management
    String getCurrentFileName() const { return currentFileName; }
    uint32_t getNextSequenceNumber() { return nextSequenceNumber; }

    // Status
    uint32_t getRecordingDuration() const;  // In milliseconds
    uint32_t getRecordingSize() const;      // In bytes (compressed)
    uint32_t getPacketCount() const { return packetCount; }

    // Error handling
    bool hasError() const { return lastError != ERROR_NONE; }
    ErrorType getLastError() const { return lastError; }
    void clearError() { lastError = ERROR_NONE; }

private:
    // State
    bool recording;
    bool sdCardPresent;
    ErrorType lastError;

    // Opus codec
    OpusCodec codec;

    // Current recording
    File currentFile;
    String currentFileName;
    uint32_t recordingStartTime;
    uint32_t bytesWritten;
    uint32_t packetCount;

    // File management
    uint32_t nextSequenceNumber;

    // Helper functions
    bool checkSDCard();
    bool createDirectories();
    String generateFilename(uint8_t channel);
    bool writePacket(const uint8_t* packet, size_t size);
};

#endif // RECORDING_ENGINE_H