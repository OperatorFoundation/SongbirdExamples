/*
 * AudioSystem.h - Audio system management for VoiceInterface
 *
 * Handles audio routing, gain control, and monitoring
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

    // Monitoring
    void enableInputMonitoring(bool enable);
    void setMonitorVolume(float volume);

    // Playback
    void setPlaybackVolume(float volume);
    float getPlaybackVolume() const { return playbackVolume; }

    void enableHeadphoneAmp(bool enable);
    void setHeadphoneVolume(uint8_t steps); // 0-15 steps (16 total levels)
    void headphoneVolumeUp();
    void headphoneVolumeDown();

    // Audio queues for recording and playback
    AudioRecordQueue* getRecordQueue() { return &recordQueue; }
    AudioPlayQueue* getPlayQueue() { return &playQueue; }

    // Audio Objects
    static AudioInputI2S audioInput;
    static AudioOutputI2S audioOutput;
    static AudioRecordQueue recordQueue;
    static AudioPlayQueue playQueue;
    static AudioPlaySdWav playWav;
    static AudioMixer4 inputMixer;
    static AudioMixer4 outputMixer;
    static AudioControlSGTL5000 audioShield;

private:
    // Current Settings
    uint8_t currentGain;
    float playbackVolume;
    float monitorVolume;
    bool monitoringEnabled;
    uint8_t headphoneVolumeSteps;

    void configureCodec();
};


#endif //FIELDRECORDER_AUDIOSYSTEM_H
