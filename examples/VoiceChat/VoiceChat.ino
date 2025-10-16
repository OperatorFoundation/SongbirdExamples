/*
 * VoiceChat.ino - Songbird VoiceChat
 *
 * A push-to-talk voice chat application with end-to-end encryption
 *
 * Features:
 * - Push-to-talk recording (PTT button)
 * - Multi-channel support (5 channels)
 * - Queue-based non-realtime messaging
 * - End-to-end encryption (AES-GCM-128)
 * - Codec2 compression for bandwidth efficiency
 * - USB Serial transport (future: LoRa)
 *
 * Button controls:
 * - TOP (UP): Push-to-Talk (hold to record, release to send)
 * - LEFT: Previous channel
 * - RIGHT: Next channel
 * - CENTER (DOWN): Skip current message / Long press to mute
 *
 * Version: 1.0.0
 * Author: Operator Foundation
 * License: MIT
 */

#include <Arduino.h>
#include <Wire.h>
#include <Audio.h>

// Include all modules
#include "Config.h"
#include "AudioSystem.h"
#include "DisplayManager.h"
#include "UIController.h"
#include "LEDControl.h"
#include "StorageManager.h"

// =============================================================================
// Global Objects
// =============================================================================

// Core modules
AudioSystem audioSystem;
DisplayManager display;
UIController ui;
LEDControl leds;
StorageManager storage;

// System state
SystemState currentState = STATE_IDLE;
ErrorType currentError = ERROR_NONE;

// Settings
Settings currentSettings;

// Timing
elapsedMillis displayUpdateTimer = 0;
elapsedMillis recordingTimer = 0;
elapsedMillis channelSwitchTimer = 0;
elapsedMillis connectionCheckTimer = 0;
elapsedMillis playbackTimer = 0;

// State variables
bool isConnected = true;  // Assume connected initially
uint8_t queuedMessages[NUM_CHANNELS] = {0};  // Messages queued per channel
bool wasPlayingBeforePTT = false;
String currentSender = "";
uint32_t currentMessageDuration = 0;

// =============================================================================
// Forward Declarations
// =============================================================================

// State processing functions
void processIdleState();
void processRecordingState();
void processPlayingState();
void processSwitchingState();
void processDisconnectedState();

// State transition functions
void startRecording();
void stopRecording();
void startPlayback();
void stopPlayback();
void switchChannel(int8_t direction);  // -1 for left, +1 for right
void playChannelSwitchBeep();

// Display function
void updateDisplay();

// Buttons
void handleButtonEvents();

// Connection
void checkConnection();

// =============================================================================
// Setup
// =============================================================================

void setup()
{
    // Initialize serial for communication and debugging
    Serial.begin(SERIAL_BAUD_RATE);
    
    #ifdef DEBUG_MODE
    // Wait briefly for serial connection (but don't block)
    uint32_t startTime = millis();
    while (!Serial && (millis() - startTime) < 3000) {
        // Wait for serial or timeout
    }

    Serial.println("\n========================================");
    Serial.println("Songbird VoiceChat");
    Serial.println("Version: " FIRMWARE_VERSION);
    Serial.println("========================================\n");
    #endif

    // Initialize I2C for display
    Wire1.begin();
    Wire1.setSDA(OLED_SDA_PIN);
    Wire1.setSCL(OLED_SCL_PIN);

    // Initialize display
    if (!display.begin())
    {
        DEBUG_PRINTLN("Display initialization failed");
        // Continue anyway - can work without display
    }

    display.showStartupScreen();
    display.update();
    delay(2000);  // Show startup screen briefly

    // Initialize UI
    ui.begin();
    ui.update();

    // Initialize LEDs
    leds.begin();

    // Initialize storage
    storage.begin();

    // Initialize settings
    currentSettings.currentChannel = 0;  // Channel 1 (0-indexed)
    currentSettings.micGain = DEFAULT_MIC_GAIN;
    currentSettings.playbackVolume = DEFAULT_PLAYBACK_VOLUME;
    currentSettings.agcEnabled = true;
    currentSettings.muted = false;

    // Initialize audio system
    if (!audioSystem.begin())
    {
        DEBUG_PRINTLN("Audio system initialization failed");
        currentState = STATE_DISCONNECTED;
        currentError = ERROR_NONE;
    }
    else
    {
        // Apply settings
        audioSystem.setMicGain(currentSettings.micGain);
        audioSystem.enableAutoGainControl(currentSettings.agcEnabled);
        audioSystem.setPlaybackVolume(currentSettings.playbackVolume);
    }

    // Create channel directories
    for (uint8_t i = 0; i < NUM_CHANNELS; i++)
    {
        String dirPath = String(RX_DIR_PREFIX) + String(i + 1);
        SD.mkdir(dirPath.c_str());
    }
    SD.mkdir(TX_DIR);

    DEBUG_PRINTLN("\nInitialization complete");
    DEBUG_PRINTLN("========================================\n");
    
    // Set initial LED state
    leds.setBlueLED(true);  // Show connected
}

// =============================================================================
// Main Loop
// =============================================================================

void loop()
{
    // Update UI
    ui.update();

    // Update LED animations
    leds.update();

    // Check connection status periodically
    checkConnection();

    // Handle button events
    handleButtonEvents();

    // Handle state-specific processing
    switch (currentState) 
    {
        case STATE_IDLE: processIdleState(); break;
        case STATE_RECORDING: processRecordingState(); break;
        case STATE_PLAYING: processPlayingState(); break;
        case STATE_SWITCHING: processSwitchingState(); break;
        case STATE_DISCONNECTED: processDisconnectedState(); break;
    }

    // Update display if needed
    updateDisplay();
}

// =============================================================================
// State Processing Functions
// =============================================================================

void processIdleState()
{
    // TODO: Check for incoming messages in queue and start playback
    // For now, just idle
}

void processRecordingState()
{
    // TODO: Process audio recording to queue
    // For now, just track time
}

void processPlayingState()
{
    // TODO: Check if playback finished
    // For now, simulate playback
    
    // Placeholder: auto-stop after simulated duration
    if (playbackTimer > currentMessageDuration * 1000)
    {
        stopPlayback();
    }
}

void processSwitchingState()
{
    // Wait for channel switch animation to complete
    if (channelSwitchTimer >= CHANNEL_SWITCH_DISPLAY_MS)
    {
        currentState = STATE_IDLE;
        DEBUG_PRINTLN("Channel switch complete");
    }
}

void processDisconnectedState()
{
    // Check if connection restored
    if (isConnected)
    {
        currentState = STATE_IDLE;
        leds.setBlueLED(true);
        DEBUG_PRINTLN("Connection restored");
    }
}

// =============================================================================
// State Transition Functions
// =============================================================================

void startRecording()
{
    DEBUG_PRINTLN("Starting recording (PTT)");

    // If we were playing, pause playback
    if (currentState == STATE_PLAYING)
    {
        wasPlayingBeforePTT = true;
        // TODO: Pause playback
    }

    currentState = STATE_RECORDING;
    recordingTimer = 0;

    // Configure audio for recording
    audioSystem.enableInputMonitoring(true);

    // Set LED indicators
    leds.setRecording(true);
    leds.setBlueLED(false);  // Dim blue during recording
}

void stopRecording()
{
    DEBUG_PRINTLN("Stopping recording (PTT released)");

    // TODO: Save recording to TX queue

    // Disable input monitoring
    audioSystem.enableInputMonitoring(false);

    // Clear recording LED
    leds.setRecording(false);
    leds.setBlueLED(true);

    // Resume playback if we were playing before
    if (wasPlayingBeforePTT)
    {
        wasPlayingBeforePTT = false;
        // TODO: Resume playback
        currentState = STATE_PLAYING;
    }
    else
    {
        currentState = STATE_IDLE;
    }
}

void startPlayback()
{
    DEBUG_PRINTLN("Starting playback");
    
    currentState = STATE_PLAYING;
    playbackTimer = 0;
    
    // TODO: Actually start playing from queue
    // For now, simulate with placeholder values
    currentSender = "Alice";
    currentMessageDuration = 5;  // 5 seconds placeholder
    
    // Set LED to pulsing
    // TODO: Add pulse animation to LEDControl
}

void stopPlayback()
{
    DEBUG_PRINTLN("Stopping playback");
    
    // TODO: Stop actual playback
    
    currentState = STATE_IDLE;
    queuedMessages[currentSettings.currentChannel]--;
}

void switchChannel(int8_t direction)
{
    DEBUG_PRINTF("Switching channel (direction: %d)\n", direction);

    // Stop any current playback
    if (currentState == STATE_PLAYING)
    {
        stopPlayback();
    }

    // Calculate new channel (with wrapping)
    int8_t newChannel = currentSettings.currentChannel + direction;
    if (newChannel < 0)
    {
        newChannel = NUM_CHANNELS - 1;
    }
    else if (newChannel >= NUM_CHANNELS)
    {
        newChannel = 0;
    }

    currentSettings.currentChannel = newChannel;

    // Play beep
    playChannelSwitchBeep();

    // Show channel switch screen
    currentState = STATE_SWITCHING;
    channelSwitchTimer = 0;

    DEBUG_PRINTF("Switched to channel %d\n", currentSettings.currentChannel + 1);

    // TODO: Start playing from new channel queue if messages exist
}

void playChannelSwitchBeep()
{
    // TODO: Generate a short beep tone
    // For now, just flash the LED
    leds.setBlueLED(false);
    delay(BEEP_DURATION_MS);
    leds.setBlueLED(true);
}

// =============================================================================
// Connection Management
// =============================================================================

void checkConnection()
{
    // For now, always assume connected
    // TODO: Implement actual connection checking via serial protocol
    isConnected = true;

    if (!isConnected && currentState != STATE_DISCONNECTED)
    {
        DEBUG_PRINTLN("Connection lost");
        currentState = STATE_DISCONNECTED;
        leds.setBlueLED(false);
    }
}

// =============================================================================
// Display Update Function
// =============================================================================

void updateDisplay()
{
    // Determine update rate based on state
    uint32_t updateInterval = (currentState == STATE_IDLE) ? 
                              DISPLAY_IDLE_UPDATE_MS : DISPLAY_UPDATE_MS;

    if (displayUpdateTimer < updateInterval)
    {
        return;  // Not time to update yet
    }

    displayUpdateTimer = 0;

    // Update based on current state
    switch (currentState) 
    {
        case STATE_IDLE:
            display.showIdleScreen(
                currentSettings.currentChannel,
                isConnected,
                queuedMessages[currentSettings.currentChannel]
            );
            break;

        case STATE_RECORDING:
            display.showRecordingScreen(
                currentSettings.currentChannel,
                recordingTimer / 1000
            );
            break;

        case STATE_PLAYING:
            display.showPlayingScreen(
                currentSettings.currentChannel,
                playbackTimer / 1000,
                currentMessageDuration,
                currentSender
            );
            break;

        case STATE_SWITCHING:
            display.showChannelSwitch(
                currentSettings.currentChannel,
                queuedMessages[currentSettings.currentChannel]
            );
            break;

        case STATE_DISCONNECTED:
            display.showDisconnected();
            break;
    }

    display.update();
}

// =============================================================================
// Button Event Handling
// =============================================================================

void handleButtonEvents()
{
    // PTT Button (TOP/UP)
    if (ui.isPressed(BTN_UP))
    {
        // PTT pressed - start recording if not already
        if (currentState != STATE_RECORDING)
        {
            startRecording();
        }
    }
    else if (ui.wasJustReleased(BTN_UP))
    {
        // PTT released - stop recording
        if (currentState == STATE_RECORDING)
        {
            stopRecording();
        }
    }

    // Channel switching (only when not recording)
    if (currentState != STATE_RECORDING)
    {
        if (ui.wasJustPressed(BTN_LEFT))
        {
            switchChannel(-1);  // Previous channel
        }
        else if (ui.wasJustPressed(BTN_RIGHT))
        {
            switchChannel(1);   // Next channel
        }
    }

    // Skip/Mute button (CENTER/DOWN)
    if (ui.wasJustPressed(BTN_DOWN))
    {
        if (currentState == STATE_PLAYING)
        {
            // Skip current message
            DEBUG_PRINTLN("Skipping message");
            stopPlayback();
            // TODO: Delete current message and play next
        }
    }
    else if (ui.isLongPressed(BTN_DOWN))
    {
        // Toggle mute
        currentSettings.muted = !currentSettings.muted;
        DEBUG_PRINTF("Mute: %s\n", currentSettings.muted ? "ON" : "OFF");
        
        // TODO: Show mute status on display briefly
    }
}