/*
* AudioSystem.h - Audio system management for field recorder
 *
 * Handles audio routing, gain control, monitoring, and effects
 */

#ifndef FIELDRECORDER_AUDIOSYSTEM_H
#define FIELDRECORDER_AUDIOSYSTEM_H

#include <Arduino.h>
#include <Audio.h>

class AudioSystem
{
public:
    AudioSystem();

     // Initialization
     bool begin();

     // Gain Control
     void setMicGain(uint8_t gain);
     uint8_t getMicGain() const { return currentGain; }
     void enableAutoGainControl(bool enable);
     bool isAutoGainControlEnabled() const { return agcEnabled; }

     // Effects
     void enableWindCut(bool enable);
     bool isWindCutEnabled() const { return windCutEnabled; }

     // Monitoring
     void enableInputMonitoring(bool enable);
     void setMonitorVolume(float volume);

     // Playback
     void setPlaybackVolume(float volume);
     float getPlaybackVolume() const { return playbackVolume; }

     // Level monitoring
     float getPeakLevel(); // 0.0 - 1.0
     bool isClipping();

     // Audio queues for recording and playback
     AudioRecordQueue* getRecordQueue() { return &recordQueue; }
     AudioPlayQueue* getPlayQueue() { return &playQueue; }

    // Audio Objects
    static AudioInputI2S audioInput;
    static AudioOutputI2S audioOutput;
    static AudioRecordQueue recordQueue;
    static AudioPlayQueue playQueue;
    static AudioPlaySdWav playWav;
    static AudioAnalyzePeak peakAnalyzer;
    static AudioFilterBiquad windCutFilter;
    static AudioMixer4 inputMixer;
    static AudioMixer4 outputMixer;
    static AudioControlSGTL5000 audioShield;

private:

    // Current Settings
    uint8_t currentGain;
    float playbackVolume;
    float monitorVolume;
    bool agcEnabled;
    bool windCutEnabled;
    bool monitoringEnabled;

    // Clipping Detection
    uint32_t lastClipTime;

    void configureCodec();
    void updateWindCutFilter();
};


#endif //FIELDRECORDER_AUDIOSYSTEM_H