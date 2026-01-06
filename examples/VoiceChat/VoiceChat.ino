/*
 * VoiceChat.ino - Songbird VoiceChat
 *
 * A push-to-talk voice chat application with Opus compression
 *
 * Button controls:
 * - TOP (UP): Push-to-Talk (hold to record, release to send)
 * - LEFT: Previous channel / Long press to show users
 * - RIGHT: Next channel
 * - CENTER (DOWN): Skip current message / Long press to mute
 */

#include <Arduino.h>
#include <Wire.h>
#include <Audio.h>

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
SystemState previousState = STATE_IDLE;
ErrorType currentError = ERROR_NONE;

// Settings
Settings currentSettings;

// Timing
elapsedMillis displayUpdateTimer = 0;
elapsedMillis playbackCheckTimer = 0;

// State variables
bool isConnected = false;
uint8_t queuedMessages[NUM_CHANNELS] = {0};
bool wasPlayingBeforePTT = false;
String currentSender = "";
uint32_t currentMessageDuration = 0;

// User list display
uint8_t userListScrollOffset = 0;

// =============================================================================
// Forward Declarations
// =============================================================================

void processIdleState();
void processRecordingState();
void processPlayingState();
void processSwitchingState();
void processUsersState();
void processDisconnectedState();

void startRecording();
void stopRecording();
void startPlayback();
void stopPlayback();
void switchChannel(int8_t direction);
void showUserList();
void hideUserList();

void updateDisplay();
void handleButtonEvents();
void processProtocol();
void updateQueueCounts();

// =============================================================================
// Setup
// =============================================================================

void setup()
{
    Serial.begin(SERIAL_BAUD_RATE);

    // Initialize I2C for display
    Wire1.begin();
    Wire1.setSDA(OLED_SDA_PIN);
    Wire1.setSCL(OLED_SCL_PIN);

    // Initialize display
    if (!display.begin())
    {
        // Continue anyway
    }

    display.showStartupScreen();
    display.update();
    delay(1000);

    // Initialize UI and LEDs
    ui.begin();
    leds.begin();

    // Initialize storage and load settings
    storage.begin();
    if (!storage.loadSettings(currentSettings))
    {
        currentSettings = storage.getDefaultSettings();
    }
    currentSettings.currentChannel = DEFAULT_CHANNEL;

    // Initialize audio system
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
        // Non-fatal, but recording won't work
    }

    // Initialize playback engine
    if (!player.begin())
    {
        // Non-fatal, but playback won't work
    }

    // Initialize serial protocol
    protocol.begin(&Serial);

    // Set initial LED state
    leds.setBlueLED(false);  // Will turn on when connected
}

// =============================================================================
// Main Loop
// =============================================================================

void loop()
{
    ui.update();
    leds.update();

    // Process serial protocol (receives files, join/part messages, pings)
    processProtocol();

    // Update connection status
    bool wasConnected = isConnected;
    isConnected = protocol.isConnected();
    
    if (isConnected && !wasConnected)
    {
        protocol.sendLog("Connected");
        leds.setBlueLED(true);
        if (currentState == STATE_DISCONNECTED)
        {
            currentState = STATE_IDLE;
        }
    }
    else if (!isConnected && wasConnected)
    {
        protocol.sendLog("Disconnected");
        leds.setBlueLED(false);
        if (currentState != STATE_RECORDING)
        {
            currentState = STATE_DISCONNECTED;
        }
    }

    // Handle button events
    handleButtonEvents();

    // State processing
    switch (currentState)
    {
        case STATE_IDLE: processIdleState(); break;
        case STATE_RECORDING: processRecordingState(); break;
        case STATE_PLAYING: processPlayingState(); break;
        case STATE_SWITCHING: processSwitchingState(); break;
        case STATE_USERS: processUsersState(); break;
        case STATE_DISCONNECTED: processDisconnectedState(); break;
        case STATE_ERROR: break;
        default: break;
    }

    // Update display
    updateDisplay();
}

// =============================================================================
// Protocol Processing
// =============================================================================

void processProtocol()
{
    if (protocol.processIncoming())
    {
        // A complete file was received
        uint8_t channel = protocol.getReceivedChannel();
        
        protocol.sendLogf("File received on ch %d", channel);
        
        // Update queue count for this channel
        if (channel >= 1 && channel <= NUM_CHANNELS)
        {
            queuedMessages[channel - 1]++;
        }
        
        protocol.clearReceivedFile();
        
        // If idle and this is our channel, start playback
        if (currentState == STATE_IDLE && 
            channel == currentSettings.currentChannel + 1)
        {
            startPlayback();
        }
    }
}

// =============================================================================
// State Processing
// =============================================================================

void processIdleState()
{
    // Check if there are queued messages on current channel
    if (playbackCheckTimer > 500)
    {
        playbackCheckTimer = 0;
        
        if (queuedMessages[currentSettings.currentChannel] > 0 && !currentSettings.muted)
        {
            startPlayback();
        }
    }
}

void processRecordingState()
{
    // Process audio from record queue
    AudioRecordQueue* queue = audioSystem.getRecordQueue();
    
    if (queue->available() > 0)
    {
        recorder.processRecording(queue);
    }
    
    // Update LED with audio level
    float level = audioSystem.getPeakLevel();
    leds.setAudioLevel(level);
    
    if (audioSystem.isClipping())
    {
        leds.setClipping(true);
    }
}

void processPlayingState()
{
    // Feed audio to play queue
    if (!player.processPlayback(audioSystem.getPlayQueue()))
    {
        // Playback finished or no more messages
        stopPlayback();
    }
}

void processSwitchingState()
{
    // Channel switch animation is handled by timer in switchChannel()
    // This state just displays the channel switch screen
}

void processUsersState()
{
    // User list display - handled by button events
}

void processDisconnectedState()
{
    // Just wait for connection
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

    protocol.sendLog("Starting recording");
    
    // Start the audio record queue
    AudioRecordQueue* queue = audioSystem.getRecordQueue();
    if (!queue)
    {
        protocol.sendLog("ERROR: Record queue is null");
        return;
    }
    
    queue->begin();
    
    // Start recording to file (channel is 0-indexed here, will be converted)
    if (!recorder.startRecording(currentSettings.currentChannel))
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

    // Stop the audio record queue
    audioSystem.getRecordQueue()->end();
    
    // Stop recording (closes file, returns filename)
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

    // Send the recorded file (channel is 1-indexed for protocol)
    String filename = recorder.getCurrentFileName();
    if (filename.length() > 0)
    {
        protocol.sendLogf("Sending: %s", filename.c_str());
        
        uint8_t channel = currentSettings.currentChannel + 1;  // Convert to 1-indexed
        if (protocol.sendFile(filename.c_str(), channel))
        {
            protocol.sendLog("File sent successfully");
            // Delete the file after sending
            SD.remove(filename.c_str());
            protocol.sendLogf("Deleted: %s", filename.c_str());
        }
        else
        {
            protocol.sendLog("Failed to send file");
        }
    }
    else
    {
        protocol.sendLog("No filename to send");
    }

    // Return to previous state
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
    protocol.sendLog("Starting playback");
    
    // Load queue for current channel
    player.loadChannelQueue(currentSettings.currentChannel);
    
    if (!player.hasMessages())
    {
        protocol.sendLog("No messages to play");
        queuedMessages[currentSettings.currentChannel] = 0;
        return;
    }

    if (!player.startPlayback(audioSystem.getPlayQueue()))
    {
        protocol.sendLog("Failed to start playback");
        return;
    }

    // Get sender info from the opened file
    currentSender = player.getSenderName();
    currentMessageDuration = player.getFileDuration();
    currentState = STATE_PLAYING;
    
    protocol.sendLogf("Playing message from: %s", currentSender.c_str());
    
    audioSystem.enableHeadphoneAmp(true);
}

void stopPlayback()
{
    protocol.sendLog("Stopping playback");
    
    player.stopPlayback();
    audioSystem.enableHeadphoneAmp(false);
    
    // Update queue count
    updateQueueCounts();
    
    currentState = STATE_IDLE;
}

void switchChannel(int8_t direction)
{
    if (currentState == STATE_PLAYING)
    {
        stopPlayback();
    }

    int8_t newChannel = currentSettings.currentChannel + direction;
    if (newChannel < 0) newChannel = NUM_CHANNELS - 1;
    if (newChannel >= NUM_CHANNELS) newChannel = 0;

    currentSettings.currentChannel = newChannel;
    
    protocol.sendLogf("Switched to channel %d", newChannel + 1);

    currentState = STATE_SWITCHING;
    
    // Use a simple delay for the channel switch animation
    // This will block briefly but keeps the code simple
    delay(CHANNEL_SWITCH_DISPLAY_MS);
    
    currentState = STATE_IDLE;
}

void showUserList()
{
    previousState = currentState;
    currentState = STATE_USERS;
    userListScrollOffset = 0;
}

void hideUserList()
{
    currentState = previousState;
}

void updateQueueCounts()
{
    // Reload queue to get accurate count
    player.loadChannelQueue(currentSettings.currentChannel);
    queuedMessages[currentSettings.currentChannel] = player.getQueuedCount();
}

// =============================================================================
// Display
// =============================================================================

void updateDisplay()
{
    uint32_t interval = (currentState == STATE_IDLE) ? DISPLAY_IDLE_UPDATE_MS : DISPLAY_UPDATE_MS;
    if (displayUpdateTimer < interval) return;
    displayUpdateTimer = 0;

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
                recorder.getRecordingDuration() / 1000  // Convert ms to seconds
            );
            break;

        case STATE_PLAYING:
            display.showPlayingScreen(
                currentSettings.currentChannel,
                player.getPlaybackPosition() / 1000,
                player.getFileDuration() / 1000,
                currentSender
            );
            break;

        case STATE_SWITCHING:
            display.showChannelSwitch(
                currentSettings.currentChannel,
                queuedMessages[currentSettings.currentChannel]
            );
            break;

        case STATE_USERS:
            {
                uint8_t count = protocol.getUserCount();
                const char* users[MAX_USERS];
                for (uint8_t i = 0; i < count && i < MAX_USERS; i++)
                {
                    users[i] = protocol.getUser(i);
                }
                display.showUserListScreen(users, count, userListScrollOffset);
            }
            break;

        case STATE_DISCONNECTED:
            display.showDisconnected();
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
    if (ui.wasJustPressed(BTN_UP))
    {
        if (currentState != STATE_RECORDING && currentState != STATE_USERS)
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

    // User list and channel switching
    if (currentState == STATE_USERS)
    {
        if (ui.wasJustReleased(BTN_LEFT))
        {
            hideUserList();
        }
        else if (ui.wasJustPressed(BTN_UP))
        {
            if (userListScrollOffset > 0) userListScrollOffset--;
        }
        else if (ui.wasJustPressed(BTN_DOWN))
        {
            uint8_t maxVisible = 2;
            if (userListScrollOffset + maxVisible < protocol.getUserCount())
            {
                userListScrollOffset++;
            }
        }
    }
    else if (currentState != STATE_RECORDING)
    {
        if (ui.isLongPressed(BTN_LEFT))
        {
            showUserList();
        }
        else if (ui.wasJustPressed(BTN_LEFT))
        {
            switchChannel(-1);
        }
        else if (ui.wasJustPressed(BTN_RIGHT))
        {
            switchChannel(1);
        }
    }

    // Skip/Mute (DOWN button)
    if (currentState != STATE_USERS && currentState != STATE_RECORDING)
    {
        if (ui.wasJustPressed(BTN_DOWN) && currentState == STATE_PLAYING)
        {
            // Skip to next message
            protocol.sendLog("Skipping message");
            
            if (!player.skipToNext())
            {
                // No more messages
                stopPlayback();
            }
            else
            {
                // Update sender info for new message
                currentSender = player.getSenderName();
                currentMessageDuration = player.getFileDuration();
                protocol.sendLogf("Now playing from: %s", currentSender.c_str());
            }
        }
        else if (ui.isLongPressed(BTN_DOWN))
        {
            currentSettings.muted = !currentSettings.muted;
            protocol.sendLogf("Mute: %s", currentSettings.muted ? "ON" : "OFF");
        }
    }
}