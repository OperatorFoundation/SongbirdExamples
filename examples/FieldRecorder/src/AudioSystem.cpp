//
// Created by Mafalda on 9/2/25.
//

#include "AudioSystem.h"
#include "Config.h"
#include <Wire.h>

// Audio objects - global instances
AudioInputI2S AudioSystem::audioInput;
AudioOutputI2S AudioSystem::audioOutput;
AudioRecordQueue AudioSystem::recordQueue;
AudioPlayQueue AudioSystem::playQueue;
AudioAnalyzePeak AudioSystem::peakAnalyzer;
AudioFilterBiquad AudioSystem::windCutFilter;
AudioMixer4 AudioSystem::inputMixer;
AudioMixer4 AudioSystem::outputMixer;
AudioControlSGTL5000 AudioSystem::audioShield;

// Audio connections - created once at program initialization
static bool initializeAudioConnections()
{
    static AudioConnection patchCord1(AudioSystem::audioInput, 0, AudioSystem::windCutFilter, 0);
    static AudioConnection patchCord2(AudioSystem::windCutFilter, 0, AudioSystem::recordQueue, 0);
    static AudioConnection patchCord3(AudioSystem::windCutFilter, 0, AudioSystem::peakAnalyzer, 0);
    static AudioConnection patchCord4(AudioSystem::windCutFilter, 0, AudioSystem::inputMixer, 0);
    static AudioConnection patchCord5(AudioSystem::playQueue, 0, AudioSystem::outputMixer, 0);
    static AudioConnection patchCord6(AudioSystem::inputMixer, 0, AudioSystem::outputMixer, 1);
    static AudioConnection patchCord7(AudioSystem::outputMixer, 0, AudioSystem::audioOutput, 0);
    static AudioConnection patchCord8(AudioSystem::outputMixer, 0, AudioSystem::audioOutput, 1);
    return true;
}

// Initialize audio connections at program startup
static bool audioConnectionsInitialized = initializeAudioConnections();

AudioSystem::AudioSystem()
{
    currentGain = DEFAULT_MIC_GAIN;
    playbackVolume = DEFAULT_PLAYBACK_VOLUME;
    monitorVolume = MONITOR_VOLUME;
    agcEnabled = true;
    windCutEnabled = false;
    monitoringEnabled = false;
    lastClipTime = 0;
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

    // Configure the codec
    configureCodec();

    // Initialize wind-cut filter (but keep it bypassed initially)
    updateWindCutFilter();

    // Set initial mixer levels
    outputMixer.gain(0, playbackVolume);          // Playback channel
    outputMixer.gain(1, 0.0);                // Monitor channel (off initially)
    outputMixer.gain(2, 0.0);                // Unused
    outputMixer.gain(3, 0.0);                // Unused

    inputMixer.gain(0, 1.0);                 // Full passthrough for monitoring
    inputMixer.gain(1, 0.0);                 // Unused
    inputMixer.gain(2, 0.0);                 // Unused
    inputMixer.gain(3, 0.0);                 // Unused

    DEBUG_PRINTLN("Audio system initialized");
    return true;
}

void AudioSystem::configureCodec()
{
    // Configure for headset microphone input
    audioShield.inputSelect(AUDIO_INPUT_MIC);
    audioShield.micGain(currentGain);

    // Set output level (headphone)
    audioShield.volume(0.7);

    // Enable AGC if appropriate
    if (agcEnabled)
    {
        // SGTL500 AGC Settings for voice recording
        audioShield.autoVolumeControl(
           AGC_MAX_GAIN,     // Maximum gain boost (2 = 12dB)
           AGC_LVL_SELECT,   // Target level (1 = good for speech)
           AGC_HARD_LIMIT,   // Hard limiter (0 = disabled)
           AGC_THRESHOLD,    // Threshold in dB (-10dB below target)
           AGC_ATTACK,       // Attack time in seconds (0.5s)
           AGC_DECAY         // Decay time in seconds (0.5s)
       );
        audioShield.autoVolumeEnable();
    }
    else
    {
        audioShield.autoVolumeDisable();
    }

    // Audio enhancement for clarity
    audioShield.audioProcessorDisable();    // No need for extra bass here ;)
    audioShield.audioPreProcessorEnable();   // But let's enable the pre-processor for DC blocking
}

void AudioSystem::setMicGain(uint8_t gain)
{
    if (gain > MAX_MIC_GAIN)
    {
        gain = MAX_MIC_GAIN;
    }

    currentGain = gain;
    audioShield.micGain(currentGain);   // 0-63 range for MIC input

    // We're manually adjusting the gain, so we should disable AGC
    if (agcEnabled)
    {
        enableAutoGainControl(false);
    }

    DEBUG_PRINTF("Mic gain set to: %d\n", gain);
}

void AudioSystem::enableAutoGainControl(bool enable)
{
    if (enable)
    {
        audioShield.autoVolumeControl(
            AGC_MAX_GAIN,     // Maximum gain boost (2 = 12dB)
            AGC_LVL_SELECT,   // Target level (1 = good for speech)
            AGC_HARD_LIMIT,   // Hard limiter (0 = disabled)
            AGC_THRESHOLD,    // Threshold in dB (-10dB below target)
            AGC_ATTACK,       // Attack time in seconds (0.5s)
            AGC_DECAY         // Decay time in seconds (0.5s)
        );
        audioShield.autoVolumeEnable();
        DEBUG_PRINTLN("AGC enabled");
    }
    else
    {
        audioShield.autoVolumeDisable();
        DEBUG_PRINTLN("AGC disabled");
    }
}

void AudioSystem::enableWindCut(bool enable)
{
    windCutEnabled = enable;
    updateWindCutFilter();
    DEBUG_PRINTF("Wind-cut filter: %s\n", enable ? "ON" : "OFF");
}

void AudioSystem::updateWindCutFilter()
{
    if (windCutEnabled)
    {
        // High-pass filter at 100Hz to reduce wind noise
        // Butterworth response for smoothness
        windCutFilter.setHighpass(0, WINDCUT_FREQUENCY, WINDCUT_Q);
    }
    else
    {
        // Bypass the filter
        // 10Hz is below human hearing range (20Hz-20kHz), Only removes extreme sub-bass and DC offset
        // Q of 0.707 is Butterworth (flat, no resonance)
        windCutFilter.setHighpass(0, 10, 0.707);
    }
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

float AudioSystem::getPeakLevel()
{
    if (peakAnalyzer.available())
    {
        return peakAnalyzer.read();
    }

    return 0.0;
}

bool AudioSystem::isClipping() {
    float peak = getPeakLevel();

    if (peak >= CLIPPING_THRESHOLD) {
        lastClipTime = millis();
        return true;
    }

    // Hold clipping indicator for a short time
    return (millis() - lastClipTime) < CLIPPING_HOLD_MS;
}