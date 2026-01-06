/*
 * VoiceChat.ino - Songbird VoiceChat
 *
 * Push-to-talk voice chat with Opus compression.
 * Records complete messages, then transfers as files.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Audio.h>
#include <SD.h>

#include "Config.h"
#include "AudioSystem.h"
#include "DisplayManager.h"
#include "UIController.h"
#include "LEDControl.h"
#include "StorageManager.h"
#include "RecordingEngine.h"
#include "PlaybackEngine.h"
#include "SerialProtocol.h"

// =============================================================================
// Global Objects
// =============================================================================

AudioSystem audioSystem;
DisplayManager display;
UIController ui;
LEDControl leds;
StorageManager storage;
RecordingEngine recorder;
PlaybackEngine player;
SerialProtocol protocol;

// System state
SystemState currentState = STATE_IDLE;
ErrorType currentError = ERROR_NONE;

// Settings
Settings currentSettings;

// Timing
elapsedMillis displayUpdateTimer = 0;
elapsedMillis recordingTimer = 0;
elapsedMillis channelSwitchTimer = 0;
elapsedMillis playbackTimer = 0;

// State variables
bool isConnected = false;
uint8_t queuedMessages[NUM_CHANNELS] = {0};
bool wasPlayingBeforePTT = false;
String currentSender = "";
uint32_t currentMessageDuration = 0;

// =============================================================================
// Forward Declarations
// =============================================================================

void processIdleState();
void processRecordingState();
void processPlayingState();
void processSwitchingState();

void startRecording();
void stopRecording();
void startPlayback();
void stopPlayback();
void switchChannel(int8_t direction);
void playChannelSwitchBeep();
void updateDisplay();
void handleButtonEvents();
void processSerial();

// =============================================================================
// Setup
// =============================================================================

void setup()
{
    // Initialize serial for protocol (not debug)
    Serial.begin(SERIAL_BAUD_RATE);
    delay(100);

    // Initialize LEDs
    leds.begin();

    // Initialize I2C for display - set pins before begin
    Wire1.setSDA(OLED_SDA_PIN);
    Wire1.setSCL(OLED_SCL_PIN);
    Wire1.begin();
    delay(50);

    // Initialize display
    if (display.begin())
    {
        display.showStartupScreen();
        display.update();
    }
    delay(1000);

    // Initialize UI
    ui.begin();

    // Initialize SD card
    SD.begin(SDCARD_CS_PIN);

    // Create directories
    SD.mkdir("/RX");
    SD.mkdir(TX_DIR);
    for (uint8_t i = 0; i < NUM_CHANNELS; i++)
    {
        char dirPath[24];
        snprintf(dirPath, sizeof(dirPath), "%s%d", RX_DIR_PREFIX, i + 1);
        SD.mkdir(dirPath);
    }

    // Initialize settings
    currentSettings.currentChannel = DEFAULT_CHANNEL;
    currentSettings.micGain = DEFAULT_MIC_GAIN;
    currentSettings.playbackVolume = DEFAULT_PLAYBACK_VOLUME;
    currentSettings.agcEnabled = true;
    currentSettings.muted = false;

    // Initialize audio
    audioSystem.begin();
    audioSystem.setMicGain(currentSettings.micGain);
    audioSystem.enableAutoGainControl(currentSettings.agcEnabled);
    audioSystem.setPlaybackVolume(currentSettings.playbackVolume);

    // Initialize storage
    storage.begin();

    // Initialize recording engine
    recorder.begin();

    // Initialize playback engine
    player.begin();

    // Initialize serial protocol
    protocol.begin(&Serial);

    // Don't auto-load queue - it's finding phantom messages
    // player.loadChannelQueue(currentSettings.currentChannel);
    // queuedMessages[currentSettings.currentChannel] = player.getQueuedCount();

    currentState = STATE_IDLE;
    leds.setBlueLED(false);
    
    updateDisplay();
}

// =============================================================================
// Main Loop
// =============================================================================

void loop()
{
    ui.update();
    leds.update();

    // Process serial communication
    processSerial();

    // Update connection status
    isConnected = protocol.isConnected();
    leds.setBlueLED(isConnected);

    handleButtonEvents();

    switch (currentState)
    {
        case STATE_IDLE:        processIdleState(); break;
        case STATE_RECORDING:   processRecordingState(); break;
        case STATE_PLAYING:     processPlayingState(); break;
        case STATE_SWITCHING:   processSwitchingState(); break;
        default: break;
    }

    updateDisplay();
}

// =============================================================================
// State Processing
// =============================================================================

void processIdleState()
{
    // Auto-play disabled for now until phantom message issue is fixed
    // if (!currentSettings.muted && player.hasMessages())
    // {
    //     startPlayback();
    // }
}

void processRecordingState()
{
    // Process audio from queue to encoder
    AudioRecordQueue* queue = audioSystem.getRecordQueue();
    recorder.processRecording(queue);

    // Update audio level display
    float level = audioSystem.getPeakLevel();
    leds.setAudioLevel(level);
    if (audioSystem.isClipping())
    {
        leds.setClipping(true);
    }
}

void processPlayingState()
{
    AudioPlayQueue* playQueue = audioSystem.getPlayQueue();

    if (!player.processPlayback(playQueue))
    {
        stopPlayback();
        return;
    }

    currentMessageDuration = player.getFileDuration();
    currentSender = player.getSenderName();
}

void processSwitchingState()
{
    if (channelSwitchTimer >= CHANNEL_SWITCH_DISPLAY_MS)
    {
        currentState = STATE_IDLE;
    }
}

// =============================================================================
// State Transitions
// =============================================================================

void startRecording()
{
    if (currentState == STATE_PLAYING)
    {
        wasPlayingBeforePTT = true;
        player.pausePlayback();
    }

    if (!recorder.startRecording(currentSettings.currentChannel))
    {
        return;
    }

    audioSystem.getRecordQueue()->begin();

    currentState = STATE_RECORDING;
    recordingTimer = 0;

    audioSystem.enableInputMonitoring(true);
    leds.setRecording(true);
    leds.setBlueLED(false);
}

void stopRecording()
{
    audioSystem.getRecordQueue()->end();

    recorder.stopRecording();

    // Send the recorded file over serial
    String filepath = recorder.getCurrentFileName();
    if (filepath.length() > 0)
    {
        protocol.sendFile(filepath.c_str(), currentSettings.currentChannel);
        SD.remove(filepath.c_str());
    }

    audioSystem.enableInputMonitoring(false);
    leds.setRecording(false);
    leds.setBlueLED(isConnected);

    if (wasPlayingBeforePTT)
    {
        wasPlayingBeforePTT = false;
        player.resumePlayback(audioSystem.getPlayQueue());
        currentState = STATE_PLAYING;
    }
    else
    {
        currentState = STATE_IDLE;
    }
}

void startPlayback()
{
    AudioPlayQueue* playQueue = audioSystem.getPlayQueue();

    if (!player.startPlayback(playQueue))
    {
        return;
    }

    currentState = STATE_PLAYING;
    playbackTimer = 0;
    currentSender = player.getSenderName();
    currentMessageDuration = player.getFileDuration();

    audioSystem.enableHeadphoneAmp(true);
}

void stopPlayback()
{
    player.stopPlayback();
    audioSystem.enableHeadphoneAmp(false);
    currentState = STATE_IDLE;

    queuedMessages[currentSettings.currentChannel] = player.getQueuedCount();
}

void switchChannel(int8_t direction)
{
    if (currentState == STATE_PLAYING)
    {
        player.stopPlayback();
    }

    int8_t newChannel = currentSettings.currentChannel + direction;
    if (newChannel < 0) newChannel = NUM_CHANNELS - 1;
    else if (newChannel >= NUM_CHANNELS) newChannel = 0;

    currentSettings.currentChannel = newChannel;

    player.loadChannelQueue(currentSettings.currentChannel);
    queuedMessages[currentSettings.currentChannel] = player.getQueuedCount();

    playChannelSwitchBeep();
    currentState = STATE_SWITCHING;
    channelSwitchTimer = 0;
}

void playChannelSwitchBeep()
{
    leds.setBlueLED(false);
    delay(BEEP_DURATION_MS);
    leds.setBlueLED(isConnected);
}

// =============================================================================
// Serial Communication
// =============================================================================

void processSerial()
{
    if (protocol.processIncoming())
    {
        if (protocol.hasReceivedFile())
        {
            uint8_t rxChannel = protocol.getReceivedChannel();
            if (rxChannel == currentSettings.currentChannel)
            {
                player.loadChannelQueue(currentSettings.currentChannel);
                queuedMessages[currentSettings.currentChannel] = player.getQueuedCount();
            }
            
            leds.setPinkLED(128);
            protocol.clearReceivedFile();
        }
    }
}

// =============================================================================
// Display
// =============================================================================

void updateDisplay()
{
    uint32_t interval = (currentState == STATE_IDLE) ?
                        DISPLAY_IDLE_UPDATE_MS : DISPLAY_UPDATE_MS;

    if (displayUpdateTimer < interval) return;
    displayUpdateTimer = 0;

    switch (currentState)
    {
        case STATE_IDLE:
            display.showIdleScreen(currentSettings.currentChannel, isConnected,
                                   queuedMessages[currentSettings.currentChannel]);
            break;
        case STATE_RECORDING:
            display.showRecordingScreen(currentSettings.currentChannel,
                                        recordingTimer / 1000);
            break;
        case STATE_PLAYING:
            display.showPlayingScreen(currentSettings.currentChannel,
                                      player.getPlaybackPosition() / 1000,
                                      player.getFileDuration() / 1000,
                                      player.getSenderName());
            break;
        case STATE_SWITCHING:
            display.showChannelSwitch(currentSettings.currentChannel,
                                      queuedMessages[currentSettings.currentChannel]);
            break;
        case STATE_ERROR:
            display.showErrorScreen(currentError);
            break;
        default:
            break;
    }

    display.update();
}

// =============================================================================
// Button Handling
// =============================================================================

void handleButtonEvents()
{
    // PTT (TOP/UP button)
    if (ui.isPressed(BTN_UP))
    {
        if (currentState != STATE_RECORDING && currentState != STATE_ERROR)
        {
            startRecording();
        }
    }
    else if (ui.wasJustReleased(BTN_UP))
    {
        if (currentState == STATE_RECORDING)
        {
            stopRecording();
        }
    }

    // Channel switching (not while recording)
    if (currentState != STATE_RECORDING && currentState != STATE_ERROR)
    {
        if (ui.wasJustPressed(BTN_LEFT))
        {
            switchChannel(-1);
        }
        else if (ui.wasJustPressed(BTN_RIGHT))
        {
            switchChannel(1);
        }
    }

    // Skip/Mute (CENTER/DOWN button)
    if (ui.wasJustPressed(BTN_DOWN))
    {
        if (currentState == STATE_PLAYING)
        {
            player.skipToNext();
            queuedMessages[currentSettings.currentChannel] = player.getQueuedCount();

            if (!player.isPlaying())
            {
                currentState = STATE_IDLE;
            }
        }
    }
    else if (ui.isLongPressed(BTN_DOWN))
    {
        currentSettings.muted = !currentSettings.muted;

        if (currentSettings.muted && currentState == STATE_PLAYING)
        {
            player.stopPlayback();
            currentState = STATE_IDLE;
        }
    }
}