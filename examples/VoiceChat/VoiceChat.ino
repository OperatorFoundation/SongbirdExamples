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
void processDisconnectedState();

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
    Serial.begin(SERIAL_BAUD_RATE);

    // Small delay for hardware to stabilize
    delay(100);

    // Initialize I2C for display
    Wire1.begin();
    Wire1.setSDA(OLED_SDA_PIN);
    Wire1.setSCL(OLED_SCL_PIN);

    // Initialize display first so we can show status
    bool displayOk = display.begin();
    if (displayOk)
    {
        display.showStartupScreen();
        display.update();
    }
    delay(1500);

    // Initialize subsystems
    ui.begin();
    leds.begin();
    storage.begin();

    // Initialize settings
    currentSettings.currentChannel = DEFAULT_CHANNEL;
    currentSettings.micGain = DEFAULT_MIC_GAIN;
    currentSettings.playbackVolume = DEFAULT_PLAYBACK_VOLUME;
    currentSettings.agcEnabled = true;
    currentSettings.muted = false;

    // Initialize audio
    if (!audioSystem.begin())
    {
        currentState = STATE_ERROR;
        currentError = ERROR_NO_SD_CARD;
    }
    else
    {
        audioSystem.setMicGain(currentSettings.micGain);
        audioSystem.enableAutoGainControl(currentSettings.agcEnabled);
        audioSystem.setPlaybackVolume(currentSettings.playbackVolume);
    }

    // Initialize recording engine
    if (!recorder.begin())
    {
        if (currentState != STATE_ERROR)
        {
            currentState = STATE_ERROR;
            currentError = recorder.getLastError();
        }
    }

    // Initialize playback engine
    player.begin();

    // Initialize serial protocol
    protocol.begin(&Serial);

    // Load initial channel queue
    player.loadChannelQueue(currentSettings.currentChannel);
    queuedMessages[currentSettings.currentChannel] = player.getQueuedCount();

    if (currentState != STATE_ERROR)
    {
        currentState = STATE_IDLE;
    }
    
    // Always show initial display state
    if (displayOk)
    {
        updateDisplay();
    }
    
    // Set LED based on connection (starts disconnected)
    leds.setBlueLED(false);
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
        case STATE_DISCONNECTED: processDisconnectedState(); break;
        case STATE_ERROR:       break;
    }

    updateDisplay();
}

// =============================================================================
// State Processing
// =============================================================================

void processIdleState()
{
    // Auto-play queued messages if not muted
    if (!currentSettings.muted && player.hasMessages() && !player.isPlaying())
    {
        startPlayback();
    }
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
    // Feed audio to playback queue
    AudioPlayQueue* playQueue = audioSystem.getPlayQueue();

    if (!player.processPlayback(playQueue))
    {
        stopPlayback();
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

void processDisconnectedState()
{
    if (isConnected)
    {
        currentState = STATE_IDLE;
        leds.setBlueLED(true);
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

    // Start recording to file
    if (!recorder.startRecording(currentSettings.currentChannel))
    {
        return;
    }

    // Start the audio queue
    audioSystem.getRecordQueue()->begin();

    currentState = STATE_RECORDING;
    recordingTimer = 0;

    audioSystem.enableInputMonitoring(true);
    leds.setRecording(true);
    leds.setBlueLED(false);
}

void stopRecording()
{
    // Stop audio queue
    audioSystem.getRecordQueue()->end();

    // Finalize recording
    recorder.stopRecording();

    // Send the recorded file over serial
    String filepath = recorder.getCurrentFileName();
    if (filepath.length() > 0)
    {
        protocol.sendFile(filepath.c_str(), currentSettings.currentChannel);
        
        // Delete local TX file after sending
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

    // Load new channel's message queue
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
    // Check for incoming files
    if (protocol.processIncoming())
    {
        if (protocol.hasReceivedFile())
        {
            // Reload queue for current channel
            uint8_t rxChannel = protocol.getReceivedChannel();
            if (rxChannel == currentSettings.currentChannel)
            {
                player.loadChannelQueue(currentSettings.currentChannel);
                queuedMessages[currentSettings.currentChannel] = player.getQueuedCount();
            }
            
            // Flash LED to indicate new message
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
                                        recorder.getRecordingDuration() / 1000);
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
        case STATE_DISCONNECTED:
            display.showDisconnected();
            break;
        case STATE_ERROR:
            display.showErrorScreen(currentError);
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