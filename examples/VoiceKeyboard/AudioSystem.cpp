//
// AudioSystem.cpp - Audio system management for VoiceInterface
//

#include "AudioSystem.h"
#include "Config.h"
#include <Wire.h>

// Audio objects - global instances
AudioInputI2S AudioSystem::audioInput;
AudioOutputI2S AudioSystem::audioOutput;
AudioRecordQueue AudioSystem::recordQueue;
AudioPlayQueue AudioSystem::playQueue;
AudioPlaySdWav AudioSystem::playWav;
AudioMixer4 AudioSystem::inputMixer;
AudioMixer4 AudioSystem::outputMixer;
AudioControlSGTL5000 AudioSystem::audioShield;

// Audio connections - created once at program initialization
static bool initializeAudioConnections()
{
    static AudioConnection patchCord1(AudioSystem::audioInput, 0, AudioSystem::recordQueue, 0);
    static AudioConnection patchCord2(AudioSystem::audioInput, 0, AudioSystem::inputMixer, 0);
    static AudioConnection patchCord3(AudioSystem::playWav, 0, AudioSystem::outputMixer, 0);
    static AudioConnection patchCord4(AudioSystem::inputMixer, 0, AudioSystem::outputMixer, 1);
    static AudioConnection patchCord5(AudioSystem::outputMixer, 0, AudioSystem::audioOutput, 0);
    static AudioConnection patchCord6(AudioSystem::outputMixer, 0, AudioSystem::audioOutput, 1);
    return true;
}

// Initialize audio connections at program startup
static bool audioConnectionsInitialized = initializeAudioConnections();

AudioSystem::AudioSystem()
{
    currentGain = DEFAULT_MIC_GAIN;
    playbackVolume = DEFAULT_PLAYBACK_VOLUME;
    monitorVolume = MONITOR_VOLUME;
    monitoringEnabled = false;
    headphoneVolumeSteps = 12;  // Start at mid-level
}

bool AudioSystem::begin()
{
    // Allocate audio memory
    AudioMemory(AUDIO_MEMORY_BLOCKS);

    // Initialize the audio shield
    if (!audioShield.enable())
    {
        DEBUG_PRINTLN("Failed to enable audio shield");
        return false;
    }

    // Initialize headphone amp pins
    pinMode(HPAMP_VOL_CLK, OUTPUT);
    pinMode(HPAMP_VOL_UD, OUTPUT);
    pinMode(HPAMP_SHUTDOWN, OUTPUT);

    digitalWrite(HPAMP_VOL_CLK, LOW);
    digitalWrite(HPAMP_VOL_UD, LOW);

    // Enable headphone amp and set to medium volume
    enableHeadphoneAmp(true);

    // Set initial headphone amp volume
    for (int i = 0; i < 12; i++) 
    {
        headphoneVolumeUp();
    }

    // Configure the codec
    configureCodec();

    // Set initial mixer levels
    outputMixer.gain(0, playbackVolume);     // Playback channel
    outputMixer.gain(1, 0.0);                // Monitor channel (off initially)
    outputMixer.gain(2, 0.0);                // Unused
    outputMixer.gain(3, 0.0);                // Unused

    inputMixer.gain(0, 1.0);                 // Full passthrough for monitoring
    inputMixer.gain(1, 0.0);                 // Unused
    inputMixer.gain(2, 0.0);                 // Unused
    inputMixer.gain(3, 0.0);                 // Unused

    recordQueue.begin();

    DEBUG_PRINTLN("Audio system initialized");
    return true;
}

void AudioSystem::configureCodec()
{
    // Configure for headset microphone input
    audioShield.inputSelect(AUDIO_INPUT_MIC);
    audioShield.micGain(currentGain);

    // Set output level (headphone)
    audioShield.lineOutLevel(13);  // Maximum line output for clean signal to external amp

    // Audio enhancement for clarity
    audioShield.audioProcessorDisable();
    audioShield.audioPreProcessorEnable();
}

void AudioSystem::setMicGain(uint8_t gain)
{
    if (gain > MAX_MIC_GAIN)
    {
        gain = MAX_MIC_GAIN;
    }

    currentGain = gain;
    audioShield.micGain(currentGain);

    DEBUG_PRINTF("Mic gain set to: %d\n", gain);
}

void AudioSystem::enableInputMonitoring(bool enable)
{
    monitoringEnabled = enable;

    if (enable)
    {
        outputMixer.gain(1, monitorVolume);
    }
    else
    {
        outputMixer.gain(1, 0.0);
    }

    DEBUG_PRINTF("Input monitoring: %s\n", enable ? "ON" : "OFF");
}

void AudioSystem::setMonitorVolume(float volume)
{
    monitorVolume = constrain(volume, 0.0, 1.0);

    if (monitoringEnabled)
    {
        outputMixer.gain(1, monitorVolume);
    }
}

void AudioSystem::setPlaybackVolume(float volume)
{
    playbackVolume = constrain(volume, 0.0, 1.0);
    outputMixer.gain(0, playbackVolume);

    DEBUG_PRINTF("Playback volume: %.2f\n", playbackVolume);
}

void AudioSystem::enableHeadphoneAmp(bool enable)
{
    digitalWrite(HPAMP_SHUTDOWN, enable ? LOW : HIGH);  // LOW = enabled
}

void AudioSystem::headphoneVolumeUp()
{
    if (headphoneVolumeSteps < 15) {  // 16 levels (0-15)
        digitalWrite(HPAMP_VOL_UD, HIGH);
        // Clock pulse
        digitalWrite(HPAMP_VOL_CLK, LOW);
        delay(1);
        digitalWrite(HPAMP_VOL_CLK, HIGH);
        delay(1);
        digitalWrite(HPAMP_VOL_CLK, LOW);
        headphoneVolumeSteps++;
    }
}

void AudioSystem::headphoneVolumeDown()
{
    if (headphoneVolumeSteps > 0) {
        digitalWrite(HPAMP_VOL_UD, LOW);
        // Clock pulse  
        digitalWrite(HPAMP_VOL_CLK, LOW);
        delay(1);
        digitalWrite(HPAMP_VOL_CLK, HIGH);
        delay(1);
        digitalWrite(HPAMP_VOL_CLK, LOW);
        headphoneVolumeSteps--;
    }
}
