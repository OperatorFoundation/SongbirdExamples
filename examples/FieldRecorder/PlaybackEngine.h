//
// Created by Mafalda on 9/3/25.
//

/*
 * PlaybackEngine.h - Audio playback management for field recorder
 *
 * Handles WAV file playback and navigation
 */

#ifndef FIELDRECORDER_PLAYBACKENGINE_H
#define FIELDRECORDER_PLAYBACKENGINE_H

#include <Arduino.h>
#include <SD.h>
#include <Audio.h>
#include "Config.h"

class PlaybackEngine
{
public:
    PlaybackEngine();

    // Initialization
    bool begin();

    // File navigation
    bool loadFileList();
    bool selectFile(uint32_t index);
    bool selectNextFile();
    bool selectPreviousFile();
    uint32_t getCurrentFileIndex() const { return currentFileIndex; }
    uint32_t getTotalFiles() const { return fileCount; }
    String getCurrentFileName() const;

    // Playback control - simplified to use AudioPlaySdWav
    bool startPlayback(AudioPlaySdWav* playWav);
    bool stopPlayback(AudioPlaySdWav* playWav);

    // Playback state - delegates to AudioPlaySdWav
    bool isPlaying(AudioPlaySdWav* playWav) const { return playWav->isPlaying(); }
    uint32_t getPlaybackPosition(AudioPlaySdWav* playWav) const;  // In milliseconds
    uint32_t getFileDuration(AudioPlaySdWav* playWav) const;      // In milliseconds

private:
    // File list
    String* fileList;
    uint32_t fileCount;
    uint32_t currentFileIndex;

    // Helper functions
    void cleanupFileList();
};

#endif //FIELDRECORDER_PLAYBACKENGINE_H