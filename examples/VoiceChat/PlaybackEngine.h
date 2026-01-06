/*
 * PlaybackEngine.h - Audio playback management for VoiceChat
 *
 * Handles Opus-compressed audio decoding and playback queue
 */

#ifndef PLAYBACK_ENGINE_H
#define PLAYBACK_ENGINE_H

#include <Arduino.h>
#include <SD.h>
#include <Audio.h>
#include "Config.h"
#include "OpusCodec.h"

// Playback states
enum PlaybackState
{
    PLAYBACK_IDLE,
    PLAYBACK_PLAYING,
    PLAYBACK_PAUSED
};

class PlaybackEngine
{
public:
    PlaybackEngine();

    // Initialization
    bool begin();

    // Queue management for a channel
    bool loadChannelQueue(uint8_t channel);
    uint8_t getQueuedCount() const { return fileCount; }
    bool hasMessages() const { return fileCount > 0; }

    // Playback control
    bool startPlayback(AudioPlayQueue* playQueue);
    bool stopPlayback();
    bool pausePlayback();
    bool resumePlayback(AudioPlayQueue* playQueue);
    bool skipToNext();

    // Process loop - call this regularly to feed audio
    // Returns false when playback is complete
    bool processPlayback(AudioPlayQueue* playQueue);

    // State queries
    bool isPlaying() const { return state == PLAYBACK_PLAYING; }
    bool isPaused() const { return state == PLAYBACK_PAUSED; }
    PlaybackState getState() const { return state; }

    // Current file info
    String getCurrentFileName() const;
    String getSenderName() const { return currentSender; }
    uint32_t getPlaybackPosition() const;   // Milliseconds
    uint32_t getFileDuration() const;       // Milliseconds
    uint8_t getCurrentChannel() const { return currentChannel; }

private:
    // Opus codec
    OpusCodec codec;

    // Playback state
    PlaybackState state;
    uint8_t currentChannel;

    // File queue (simple array for current channel)
    String* fileList;
    uint8_t fileCount;
    uint8_t currentFileIndex;

    // Current file
    File currentFile;
    String currentSender;
    uint32_t playbackStartTime;
    uint32_t totalPackets;
    uint32_t packetsPlayed;
    bool fileHeaderRead;

    // Audio buffer for upsampled output
    int16_t outputBuffer[RESAMPLE_OUTPUT_SAMPLES];
    size_t outputBufferPos;
    size_t outputBufferCount;

    // Helper functions
    void cleanupFileList();
    bool openNextFile();
    bool readPacket(uint8_t* packet, size_t* packetSize);
    bool decodeAndBuffer();
    String extractSender(const String& filename);
    void deleteCurrentFile();
};

#endif // PLAYBACK_ENGINE_H