/*
 * FieldRecorder.ino - Songbird Field Recorder
 *
 * A field recording application for Songbird
 *
 * Features:
 * - 48kHz/16-bit mono recording
 * - Automatic Gain Control with manual override
 * - Wind-cut filter for outdoor recording
 * - Input monitoring during recording
 * - Playback with volume control
 * - OLED display with recording status
 * - LED indicators for recording and audio levels
 * - Persistent settings storage
 *
 * Developed for the Songbird hardware
 *
 * Button controls:
 * - UP: Start recording (long press to stop)
 * - DOWN: Play/pause
 * - LEFT: Previous file / Decrease gain/volume
 * - RIGHT: Next file / Increase gain/volume
 * - LEFT+RIGHT (long press): Enable AGC
 * - All buttons on boot: Factory reset
 *
 * Version: 1.0.0
 * Author: Mafalda
 * License: MIT
 */

#include <Arduino.h>
#include <Wire.h>
#include <Audio.h>

// Include all modules
#include "Config.h"
#include "AudioSystem.h"
#include "RecordingEngine.h"
#include "PlaybackEngine.h"
#include "DisplayManager.h"
#include "UIController.h"
#include "LEDControl.h"
#include "StorageManager.h"

// =============================================================================
// Global Objects
// =============================================================================

// Core modules
AudioSystem audioSystem;
RecordingEngine recorder;
PlaybackEngine player;
DisplayManager display;
UIController ui;
LEDControl leds;
StorageManager storage;

// System state
SystemState currentState = STATE_IDLE;
SystemState previousState = STATE_IDLE;
ErrorType currentError = ERROR_NONE;

// Settings
Settings currentSettings;

// Timing
elapsedMillis displayUpdateTimer = 0;
elapsedMillis recordingTimer = 0;
elapsedMillis countdownTimer = 0;
elapsedMillis gainDisplayTimer = 0;
elapsedMillis volumeDisplayTimer = 0;

// State variables
uint8_t countdownSeconds = COUNTDOWN_SECONDS;
bool showGainOverlay = false;
bool showVolumeOverlay = false;
bool agcJustEnabled = false;

// =============================================================================
// Forward Declarations
// =============================================================================

// State processing functions
void processIdleState();
void processCountdownState();
void processRecordingState();
void processPlaybackState();
void processErrorState();

// State transition functions
void startCountdown();
void cancelCountdown();
void startRecording();
void stopRecording();
void startPlayback();
void stopPlayback();

// Settings functions
void enableAGC();
void performFactoryReset();

// Display function
void updateDisplay();

// Buttons
void handleButtonEvents();


// =============================================================================
// Setup
// =============================================================================

void setup()
{
    // Initialize serial for debugging
    #ifdef DEBUG_MODE

    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        // Wait for serial or timeout
    }

    Serial.println("\n========================================");
    Serial.println("Songbird Field Recorder");
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
    delay(3000);  // Show startup screen briefly

    // Initialize UI
    ui.begin();
    ui.update();

    // Check for factory reset (all buttons pressed)
    if (ui.getPressedButtons() == (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)) performFactoryReset();

    // Initialize LEDs
    leds.begin();

    // Initialize storage and load settings
    storage.begin();
    if (!storage.loadSettings(currentSettings))
    {
        DEBUG_PRINTLN("Using default settings");
        currentSettings = storage.getDefaultSettings();
    }

    // Show settings status on startup if not using AGC
    if (!currentSettings.agcEnabled)
    {
        display.clear();
        display.showGainAdjustment(currentSettings.micGain);
        display.update();
        delay(1000);
    }

    // Initialize audio system
    if (!audioSystem.begin())
    {
        DEBUG_PRINTLN("Audio system initialization failed");
        currentState = STATE_ERROR;
        currentError = ERROR_NONE;  // Generic error
    }
    else
    {
        // Apply loaded settings
        audioSystem.setMicGain(currentSettings.micGain);
        audioSystem.enableAutoGainControl(currentSettings.agcEnabled);
        audioSystem.setPlaybackVolume(currentSettings.playbackVolume);
        audioSystem.enableWindCut(currentSettings.windCutEnabled);
    }

    // Initialize recording engine
    if (!recorder.begin())
    {
        DEBUG_PRINTLN("Recording engine initialization failed");

        if (recorder.getLastError() == ERROR_NO_SD_CARD)
        {
            currentState = STATE_ERROR;
            currentError = ERROR_NO_SD_CARD;
        }
    }

    // Initialize playback engine
    if (!player.begin()) DEBUG_PRINTLN("Playback engine initialization failed"); // Not critical - can work without playback

    DEBUG_PRINTLN("\nInitialization complete");
    DEBUG_PRINTLN("========================================\n");
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

    handleButtonEvents();

    // Handle state-specific processing
    switch (currentState) 
    {
        case STATE_IDLE: processIdleState(); break;
        case STATE_COUNTDOWN: processCountdownState(); break;
        case STATE_RECORDING: processRecordingState(); break;
        case STATE_PLAYBACK: processPlaybackState(); break;
        case STATE_ERROR: processErrorState(); break;
    }

    // Update display if needed
    updateDisplay();
}

// =============================================================================
// State Processing Functions
// =============================================================================

void processIdleState()
{
}

void processCountdownState()
{
    // Check for countdown completion
    if (countdownTimer >= 1000)
    {
        countdownTimer = 0;
        countdownSeconds--;

        if (countdownSeconds == 0) startRecording();
        else leds.flashCountdown(2);  // Two flashes per second
    }
}

void processRecordingState()
{
    // Process audio recording
    AudioRecordQueue* queue = audioSystem.getRecordQueue();
    recorder.processRecording(queue);

    // Update audio level display
    float level = audioSystem.getPeakLevel();
    leds.setAudioLevel(level);
    leds.setClipping(audioSystem.isClipping());
}

void processPlaybackState()
{
    // Check if playback finished
    if (!AudioSystem::playWav.isPlaying()) 
    {
        stopPlayback();
    }
}

void processErrorState() 
{
    // Check if error has been resolved
    if (currentError == ERROR_NO_SD_CARD) {
        // Check for SD card insertion
        if (recorder.begin()) {
            currentState = STATE_IDLE;
            currentError = ERROR_NONE;
            DEBUG_PRINTLN("SD card detected, returning to idle");
        }
    }
}

// =============================================================================
// State Transition Functions
// =============================================================================

void startCountdown() 
{
    DEBUG_PRINTLN("Starting countdown");

    // Check for SD card
    if (recorder.hasError()) 
    {
        currentState = STATE_ERROR;
        currentError = recorder.getLastError();
        DEBUG_PRINTLN("Recorder error");
        DEBUG_PRINTLN(currentError);
        return;
    }

    currentState = STATE_COUNTDOWN;
    countdownSeconds = COUNTDOWN_SECONDS;
    countdownTimer = 0;

    // Start countdown LED flashing
    leds.flashCountdown(COUNTDOWN_SECONDS * 2);
}

void cancelCountdown() {
    DEBUG_PRINTLN("Countdown cancelled");
    currentState = STATE_IDLE;
    leds.setRecording(false);
}

void startRecording() 
{
    DEBUG_PRINTLN("Starting recording");

    if (!recorder.startRecording()) {
        currentState = STATE_ERROR;
        currentError = recorder.getLastError();
        return;
    }

    // Save the updated sequence number to EEPROM
    currentSettings.sequenceNumber = recorder.getNextSequenceNumber();
    storage.saveSettings(currentSettings);

    currentState = STATE_RECORDING;
    recordingTimer = 0;

    // Configure audio for recording
    audioSystem.enableInputMonitoring(true);

    // Set LED indicators
    leds.setRecording(true);
}

void stopRecording() {
    DEBUG_PRINTLN("Stopping recording");

    // Stop recording and save file
    recorder.stopRecording();

    // Disable input monitoring
    audioSystem.enableInputMonitoring(false);

    // Clear LEDs
    leds.setRecording(false);

    // Reload file list for playback
    player.loadFileList();

    currentState = STATE_IDLE;
}

void startPlayback() 
{
    DEBUG_PRINTLN("Starting playback");
    
    if (!player.startPlayback(&AudioSystem::playWav)) 
    {
        DEBUG_PRINTLN("Playback failed to start");
        return;
    }
    
    currentState = STATE_PLAYBACK;
}

void stopPlayback() {
    DEBUG_PRINTLN("Stopping playback");
    
    player.stopPlayback(&AudioSystem::playWav);
    currentState = STATE_IDLE;
}

// =============================================================================
// Settings Functions
// =============================================================================

void enableAGC() {
    DEBUG_PRINTLN("Enabling AGC");

    currentSettings.agcEnabled = true;
    audioSystem.enableAutoGainControl(true);
    storage.saveSettings(currentSettings);

    // Show confirmation
    display.showAGCEnabled();
    display.update();
    delay(1000);

    agcJustEnabled = true;
}

void performFactoryReset() {
    DEBUG_PRINTLN("Performing factory reset");

    // Show confirmation
    display.showFactoryReset();
    display.update();
    leds.setBlueLED(true);
    leds.setPinkLED(255);

    // Reset to defaults
    storage.factoryReset();
    storage.loadSettings(currentSettings);

    // Apply defaults
    audioSystem.setMicGain(currentSettings.micGain);
    audioSystem.enableAutoGainControl(currentSettings.agcEnabled);
    audioSystem.setPlaybackVolume(currentSettings.playbackVolume);
    audioSystem.enableWindCut(currentSettings.windCutEnabled);

    delay(2000);

    // Clear LEDs
    leds.setBlueLED(false);
    leds.setPinkLED(0);
}

// =============================================================================
// Display Update Function
// =============================================================================

void updateDisplay()
{
    // Determine update rate based on state
    uint32_t updateInterval = (currentState == STATE_IDLE) ? DISPLAY_IDLE_UPDATE_MS : DISPLAY_UPDATE_MS;

    if (displayUpdateTimer < updateInterval)  return;  // Not time to update yet

    displayUpdateTimer = 0;

    // Update hint system
    display.updateHintSystem(!currentSettings.agcEnabled, agcJustEnabled);
    if (agcJustEnabled) agcJustEnabled = false;
    
    // Handle temporary overlays
    if (showGainOverlay && gainDisplayTimer < 1500) 
    {
        display.showGainAdjustment(currentSettings.micGain);
        display.update();
        return;
    } 
    else 
    {
        showGainOverlay = false;
    }

    if (showVolumeOverlay && volumeDisplayTimer < 1500) 
    {
        display.showVolumeAdjustment(currentSettings.playbackVolume);
        display.update();
        return;
    } 
    else 
    {
        showVolumeOverlay = false;
    }

    // Update based on current state
    switch (currentState) 
    {
        case STATE_IDLE:
            display.showIdleScreen(
                recorder.getAvailableHours(),
                player.getCurrentFileName(),
                player.getCurrentFileIndex() + 1,
                player.getTotalFiles(),
                currentSettings.agcEnabled,
                currentSettings.micGain
            );
            break;

        case STATE_COUNTDOWN:
            display.showCountdownScreen(countdownSeconds);
            break;

        case STATE_RECORDING:
            display.showRecordingScreen(
                recordingTimer / 1000,
                audioSystem.getPeakLevel(),
                recorder.getCurrentFileName(),
                currentSettings.windCutEnabled
            );
            break;

        case STATE_PLAYBACK:
            display.showPlaybackScreen(
                player.getPlaybackPosition(&AudioSystem::playWav) / 1000,
                player.getFileDuration(&AudioSystem::playWav) / 1000,
                player.getCurrentFileName(),
                player.getCurrentFileIndex() + 1,
                player.getTotalFiles()
            );
            break;

        case STATE_ERROR:
            display.showErrorScreen(currentError);
            break;
    }

    display.update();
}

void handleButtonEvents() 
{
    for (Button btn : {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT}) 
    {

        // Just pressed events
        if (ui.wasJustPressed(btn)) 
        {
            switch (currentState) 
            {
                case STATE_IDLE:
                    if (btn == BTN_UP) startCountdown();
                    else if (btn == BTN_DOWN && player.getTotalFiles() > 0) startPlayback();
                    else if (btn == BTN_LEFT && player.getTotalFiles() > 0) player.selectPreviousFile();
                    else if (btn == BTN_RIGHT && player.getTotalFiles() > 0) player.selectNextFile();
                    break;

                case STATE_RECORDING:
                    if (btn == BTN_LEFT) 
                    {
                        if (currentSettings.agcEnabled) 
                        {
                            currentSettings.windCutEnabled = !currentSettings.windCutEnabled;
                            audioSystem.enableWindCut(currentSettings.windCutEnabled);
                            storage.saveSettings(currentSettings);
                        } 
                        else 
                        {
                            if (currentSettings.micGain > MIN_MIC_GAIN) 
                            {
                                currentSettings.micGain = max(MIN_MIC_GAIN, currentSettings.micGain - GAIN_STEP);
                                audioSystem.setMicGain(currentSettings.micGain);
                                storage.saveSettings(currentSettings);
                                showGainOverlay = true;
                                gainDisplayTimer = 0;
                            }
                        }
                    }
                    else if (btn == BTN_RIGHT && !currentSettings.agcEnabled) 
                    {
                        if (currentSettings.micGain < MAX_MIC_GAIN) 
                        {
                            currentSettings.micGain = min(MAX_MIC_GAIN, currentSettings.micGain + GAIN_STEP);
                            audioSystem.setMicGain(currentSettings.micGain);
                            storage.saveSettings(currentSettings);
                            showGainOverlay = true;
                            gainDisplayTimer = 0;
                        }
                    }
                    break;

                case STATE_PLAYBACK:
                    if (btn == BTN_DOWN) 
                    {
                        stopPlayback();
                        player.selectNextFile();
                        startPlayback();
                    }
                    else if (btn == BTN_UP) stopPlayback();
                    else if (btn == BTN_LEFT) 
                    {
                        currentSettings.playbackVolume = max(0.0f, currentSettings.playbackVolume - VOLUME_STEP);
                        audioSystem.setPlaybackVolume(currentSettings.playbackVolume);
                        storage.saveSettings(currentSettings);
                        showVolumeOverlay = true;
                        volumeDisplayTimer = 0;
                    }
                    else if (btn == BTN_RIGHT) 
                    {
                        currentSettings.playbackVolume = min(1.0f, currentSettings.playbackVolume + VOLUME_STEP);
                        audioSystem.setPlaybackVolume(currentSettings.playbackVolume);
                        storage.saveSettings(currentSettings);
                        showVolumeOverlay = true;
                        volumeDisplayTimer = 0;
                    }
                    break;

                case STATE_ERROR:
                    currentState = STATE_IDLE;
                    currentError = ERROR_NONE;
                    break;

                default: break;
            }
        }

        // Long press events
        if (ui.isLongPressed(btn)) {
            if (currentState == STATE_RECORDING && btn == BTN_UP) stopRecording();
        }
    }

    // Combo press (AGC enable)
    if (ui.isComboLongPressed(BTN_LEFT, BTN_RIGHT) && !currentSettings.agcEnabled) {
        enableAGC();
    }
}