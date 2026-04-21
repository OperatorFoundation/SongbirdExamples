/*
 * VoiceChat.ino - VoiceInterface
 *
 * A simplified push-to-talk voice interface with Opus compression
 *
 * Button controls:
 * - TOP (UP): Push-to-Talk (hold to record, release to send)
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
#include "SerialProtocol.h"
#include "RecordingEngine.h"
#include "PlaybackEngine.h"

// =============================================================================
// Global Objects
// =============================================================================

AudioSystem audioSystem;
DisplayManager display;
UIController ui;
LEDControl leds;
StorageManager storage;
SerialProtocol protocol;
RecordingEngine recorder;
PlaybackEngine player;

// System state
SystemState currentState = STATE_IDLE;
ErrorType currentError = ERROR_NONE;

// Timing
elapsedMillis playbackCheckTimer = 0;

// State variables
bool isConnected = false;
bool sdCardReady = false;

// =============================================================================
// Forward Declarations
// =============================================================================

void startRecording();
void stopRecording();
void startPlayback();
void stopPlayback();

void handleButtonEvents();
void processProtocol();
void cleanupFilesOnBoot();
bool retrySDCard();

// =============================================================================
// Setup
// =============================================================================

void setup()
{
    Serial.begin(SERIAL_BAUD_RATE);

    // Initialize I2C for display (stubbed)
    Wire1.begin();
    Wire1.setSDA(17);
    Wire1.setSCL(16);

    // Initialize display (stubbed - no-op)
    display.begin();
    delay(1000);

    // Initialize UI and LEDs
    ui.begin();
    leds.begin();

    // Initialize storage (stubbed - no EEPROM)
    storage.begin();

    // Initialize audio system
    if (!audioSystem.begin())
    {
        currentState = STATE_ERROR;
        currentError = ERROR_NO_SD_CARD;
        protocol.sendLog("Audio init failed");
    }
    else
    {
        audioSystem.setMicGain(DEFAULT_MIC_GAIN);
        audioSystem.setPlaybackVolume(DEFAULT_PLAYBACK_VOLUME);
    }

    // Initialize recording engine
    if (!recorder.begin())
    {
        currentState = STATE_ERROR;
        currentError = ERROR_NO_SD_CARD;
        protocol.sendLog("Recording engine init failed");
    }
    else
    {
        sdCardReady = true;
    }

    // Initialize playback engine
    if (!player.begin())
    {
        protocol.sendLog("Playback engine init failed");
    }

    // Initialize serial protocol
    protocol.begin(&Serial);

    // Clean up files on boot
    if (sdCardReady)
    {
        cleanupFilesOnBoot();
    }

    // Set initial LED state
    leds.setBlueLED(false);
    
    protocol.sendLog("VoiceInterface ready");
}

// =============================================================================
// Main Loop
// =============================================================================

void loop()
{
    ui.update();
    leds.update();

    // Process serial protocol
    processProtocol();

    // Connection status tracking (simplified - always assume connected for standalone use)
    if (!isConnected)
    {
        isConnected = true;
        protocol.sendLog("Connected");
        leds.setBlueLED(true);
    }

    // Handle button events
    handleButtonEvents();

    // State processing
    switch (currentState)
    {
        case STATE_IDLE:
            // Check for received files
            if (playbackCheckTimer > 500)
            {
                playbackCheckTimer = 0;
                if (player.hasMessages() && !player.isPlaying())
                {
                    startPlayback();
                }
            }
            break;
            
        case STATE_RECORDING:
            {
                // Process audio from record queue
                AudioRecordQueue* queue = audioSystem.getRecordQueue();
                if (queue && queue->available() > 0)
                {
                    recorder.processRecording(queue);
                }
            }
            break;
            
        case STATE_PLAYING:
            // Feed audio to play queue
            if (!player.processPlayback(audioSystem.getPlayQueue()))
            {
                stopPlayback();
            }
            break;
            
        case STATE_ERROR:
            // Wait for button press to retry SD card
            break;
            
        default:
            break;
    }

    // Update display
    display.update();
}

// =============================================================================
// Protocol Processing
// =============================================================================

void processProtocol()
{
    if (protocol.processIncoming())
    {
        // A complete file was received
        protocol.sendLog("File received");
        
        // Reload playback queue
        player.loadQueue();
        
        protocol.clearReceivedFile();
        
        // If idle, start playback
        if (currentState == STATE_IDLE && player.hasMessages())
        {
            startPlayback();
        }
    }
}

// =============================================================================
// File Cleanup on Boot
// =============================================================================

void cleanupFilesOnBoot()
{
    protocol.sendLog("Cleaning up files on boot");
    
    // Delete all files in /TX/
    File txDir = SD.open(TX_DIR);
    if (txDir)
    {
        while (true)
        {
            File entry = txDir.openNextFile();
            if (!entry) break;
            String name = entry.name();
            entry.close();
            SD.remove((String(TX_DIR) + "/" + name).c_str());
        }
        txDir.close();
        protocol.sendLog("TX directory cleaned");
    }
    
    // Delete all files in /RX/
    File rxDir = SD.open(RX_DIR);
    if (rxDir)
    {
        while (true)
        {
            File entry = rxDir.openNextFile();
            if (!entry) break;
            String name = entry.name();
            entry.close();
            SD.remove((String(RX_DIR) + "/" + name).c_str());
        }
        rxDir.close();
        protocol.sendLog("RX directory cleaned");
    }
}

// =============================================================================
// SD Card Retry
// =============================================================================

bool retrySDCard()
{
    protocol.sendLog("Retrying SD card...");
    
    if (SD.begin(SDCARD_CS_PIN))
    {
        protocol.sendLog("SD card initialized");
        sdCardReady = true;
        currentState = STATE_IDLE;
        currentError = ERROR_NONE;
        cleanupFilesOnBoot();
        return true;
    }
    else
    {
        protocol.sendLog("SD card init failed");
        return false;
    }
}

// =============================================================================
// State Transitions
// =============================================================================

void startRecording()
{
    // Stop playback if playing (interrupt speaker)
    if (currentState == STATE_PLAYING)
    {
        player.stopPlayback();
        // Delete current file when PTT pressed during playback
        String currentFile = player.getCurrentFileName();
        if (currentFile.length() > 0)
        {
            SD.remove(currentFile.c_str());
            protocol.sendLogf("Deleted interrupted file: %s", currentFile.c_str());
        }
    }

    protocol.sendLog("Starting recording");
    
    AudioRecordQueue* queue = audioSystem.getRecordQueue();
    if (!queue)
    {
        protocol.sendLog("ERROR: Record queue is null");
        return;
    }
    
    queue->begin();
    
    if (!recorder.startRecording())
    {
        protocol.sendLog("Failed to start recording");
        queue->end();
        return;
    }

    protocol.sendLog("Recording started");
    currentState = STATE_RECORDING;

    audioSystem.enableInputMonitoring(true);
    leds.setRecording(true);
}

void stopRecording()
{
    uint32_t duration = recorder.getRecordingDuration();
    protocol.sendLogf("Stopping recording: %lu ms", duration);

    audioSystem.getRecordQueue()->end();
    
    if (!recorder.stopRecording())
    {
        protocol.sendLog("Failed to stop recording");
        audioSystem.enableInputMonitoring(false);
        leds.setRecording(false);
        currentState = STATE_IDLE;
        return;
    }

    audioSystem.enableInputMonitoring(false);
    leds.setRecording(false);

    // Send the recorded file
    String filename = recorder.getCurrentFileName();
    if (filename.length() > 0)
    {
        protocol.sendLogf("Sending: %s", filename.c_str());
        
        if (protocol.sendFile(filename.c_str()))
        {
            protocol.sendLog("File sent successfully");
            SD.remove(filename.c_str());
            protocol.sendLogf("Deleted: %s", filename.c_str());
        }
        else
        {
            protocol.sendLog("Failed to send file");
        }
    }

    currentState = STATE_IDLE;
}

void startPlayback()
{
    protocol.sendLog("Starting playback");
    
    if (!player.startPlayback(audioSystem.getPlayQueue()))
    {
        protocol.sendLog("Failed to start playback");
        return;
    }

    currentState = STATE_PLAYING;
    
    protocol.sendLogf("Playing: %s", player.getCurrentFileName().c_str());
    
    audioSystem.enableHeadphoneAmp(true);
}

void stopPlayback()
{
    protocol.sendLog("Stopping playback");
    
    player.stopPlayback();
    audioSystem.enableHeadphoneAmp(false);
    
    currentState = STATE_IDLE;
}

// =============================================================================
// Button Handling
// =============================================================================

void handleButtonEvents()
{
    // PTT (TOP/UP button)
    if (ui.wasJustPressed(BTN_UP))
    {
        if (currentState == STATE_IDLE || currentState == STATE_PLAYING)
        {
            startRecording();
        }
        else if (currentState == STATE_ERROR)
        {
            // Retry SD card
            retrySDCard();
        }
    }
    else if (ui.wasJustReleased(BTN_UP))
    {
        if (currentState == STATE_RECORDING)
        {
            stopRecording();
        }
    }
}
