/*
 * Config.h - Configuration and constants for Songbird VoiceChat
 *
 * Pin definitions, audio configuration, timing constants, and shared structures
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Version info
#define FIRMWARE_VERSION "1.0.0"
#define DEVICE_NAME "Songbird VoiceChat"

// ============================================================================
// Pin Definitions
// ============================================================================

// Buttons
#define BTN_UP_PIN       3     // PTT (Push-to-Talk)
#define BTN_DOWN_PIN     29    // Skip/Mute
#define BTN_LEFT_PIN     28    // Previous channel / Show users
#define BTN_RIGHT_PIN    30    // Next channel

// SD Card
#define SDCARD_CS_PIN    10
#define SDCARD_DETECT_PIN 9
#define SDCARD_MOSI_PIN  11
#define SDCARD_MISO_PIN  12
#define SDCARD_SCK_PIN   13

// LEDs
#define LED_BLUE_PIN     35    // Connection/Status indicator
#define LED_PINK_PIN     31    // Audio level/clipping indicator
#define COUNTDOWN_FLASH_MS     200      // LED flash rate during countdown

// Display (128x32 OLED)
#define OLED_ADDRESS     0x3C
#define OLED_SCL_PIN     16
#define OLED_SDA_PIN     17
#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT    32

// Headphones
#define HPAMP_VOL_CLK   52
#define HPAMP_VOL_UD     5
#define HPAMP_SHUTDOWN  45

// ============================================================================
// Audio Configuration
// ============================================================================

// Teensy Audio Library constants
#define TEENSY_AUDIO_SAMPLE_RATE       44100
#define AUDIO_BITS_PER_SAMPLE   16
#define AUDIO_MEMORY_BLOCKS     120      // Memory for audio processing
#define AUDIO_BLOCK_SAMPLES     128      // Teensy Audio block size

// Recording settings
#define RECORDING_SAMPLE_RATE   44100    // Teensy Audio Library native rate
#define RECORDING_CHANNELS      1        // Mono for voice recording

// Audio levels
#define DEFAULT_MIC_GAIN       10      // 0-63 for SGTL5000
#define MIN_MIC_GAIN           0
#define MAX_MIC_GAIN           63
#define GAIN_STEP              2        // Adjustment per button press

#define DEFAULT_PLAYBACK_VOLUME 0.5     // 0.0-1.0
#define VOLUME_STEP            0.05     // Adjustment per button press
#define MONITOR_VOLUME         0.3      // Input monitoring level during recording

// AGC (Automatic Gain Control) settings for SGTL5000
#define AGC_MAX_GAIN           2        // 0=3dB, 1=6dB, 2=12dB max gain boost
#define AGC_LVL_SELECT         1        // Target level (0-31, lower = louder)
#define AGC_HARD_LIMIT         0        // 0=disabled, 1=enabled
#define AGC_THRESHOLD          -10      // dB below target to activate
#define AGC_ATTACK             0.5      // Attack time (seconds)
#define AGC_DECAY              0.5      // Decay time (seconds)

// Wind-cut filter (high-pass at 100Hz)
#define WINDCUT_FREQUENCY      100      // Hz
#define WINDCUT_Q              0.707    // Butterworth response

// Clipping detection
#define CLIPPING_THRESHOLD     0.9      // Peak level that triggers clipping indicator
#define CLIPPING_HOLD_MS       500      // How long to show clipping indicator

// ============================================================================
// Opus Codec Configuration
// ============================================================================

// Opus settings (defined in OpusCodec.h but documented here)
// - Sample rate: 16kHz (wideband voice)
// - Frame size: 20ms (320 samples at 16kHz)
// - Bitrate: 16kbps (good voice quality)
// - Application: VOIP
// - Channels: Mono

// Resampling (44.1kHz Teensy -> 16kHz Opus -> 44.1kHz Teensy)
// Input: 882 samples at 44.1kHz (20ms) -> 320 samples at 16kHz
// Output: 320 samples at 16kHz -> 882 samples at 44.1kHz
#define RESAMPLE_INPUT_SAMPLES  882     // 20ms at 44.1kHz
#define RESAMPLE_OUTPUT_SAMPLES 882     // 20ms at 44.1kHz (after upsample)

// ============================================================================
// Timing Constants
// ============================================================================

// Button handling
#define BUTTON_DEBOUNCE_MS     50       // Debounce time for buttons
#define LONG_PRESS_MS          1000     // Long press detection
#define EXTRA_LONG_PRESS_MS    2000     // Extra long press

// Display updates
#define DISPLAY_UPDATE_MS      100      // Update rate during recording/playback
#define DISPLAY_IDLE_UPDATE_MS 500      // Update rate when idle

// ============================================================================
// File System
// ============================================================================

#define RECORDER_MAX_FILENAME_LEN    48
#define MAX_FILES_TO_SCAN     999       // Maximum recordings to index
#define MAX_SEQUENCE_NUMBER   99999     // Maximum file sequence number

// ============================================================================
// System States
// ============================================================================

enum SystemState {
    STATE_IDLE,          // Connected, listening
    STATE_RECORDING,     // PTT held, recording
    STATE_PLAYING,       // Playing received message
    STATE_SWITCHING,     // Channel switch animation
    STATE_USERS,         // Showing user list
    STATE_ERROR,         // Error state
    STATE_DISCONNECTED   // No serial connection
};

enum ErrorType {
    ERROR_NONE,
    ERROR_NO_SD_CARD,
    ERROR_SD_CARD_FULL,
    ERROR_FILE_CREATE_FAILED,
    ERROR_WRITE_FAILED,
    ERROR_READ_FAILED
};

// ============================================================================
// Settings Structure (stored in EEPROM)
// ============================================================================

struct Settings {
    uint8_t version;           // Settings version for migration
    uint8_t currentChannel;    // Voice channel 0-4 (displayed as 1-5)
    uint8_t micGain;           // Current microphone gain (0-63)
    float playbackVolume;      // Playback volume (0.0-1.0)
    bool agcEnabled;           // Automatic Gain Control on/off
    bool windCutEnabled;       // Wind-cut filter on/off
    bool muted;                // Mute incoming messages
    uint32_t sequenceNumber;   // Next file sequence number
    uint32_t checksum;         // Simple validity check
};

// EEPROM addresses
#define EEPROM_SETTINGS_ADDR   0
#define SETTINGS_VERSION       1

// ============================================================================
// Debug Configuration
// ============================================================================

// Uncomment for serial debug output
#define DEBUG_MODE

#ifdef DEBUG_MODE
  #define DEBUG_PRINT(x)     Serial.print(x)
  #define DEBUG_PRINTLN(x)   Serial.println(x)
  #define DEBUG_PRINTF(...)  Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

// ============================================================================
// VoiceChat Configuration
// ============================================================================

// Channels
#define NUM_CHANNELS           5        // Total number of channels
#define DEFAULT_CHANNEL        0        // Start on Channel 1 (0-indexed)

// Serial communication
#define SERIAL_BAUD_RATE       115200
#define CONNECTION_TIMEOUT_MS  3000     // Consider disconnected after 3s of no data

// Display timing
#define CHANNEL_SWITCH_DISPLAY_MS 800   // How long to show channel switch screen

// File system
#define RX_DIR_PREFIX         "/RX/CH"   // /RX/CH1/, /RX/CH2/, etc.
#define TX_DIR                "/TX"      // Outgoing messages
#define MAX_FILES_PER_CHANNEL 100        // Limit queue size per channel

#endif // CONFIG_H