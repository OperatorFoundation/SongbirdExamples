/*
 * Config.h - Configuration and constants for Songbird Field Recorder
 *
 * Pin definitions, audio configuration, timing constants, and shared structures
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Version info
#define FIRMWARE_VERSION "1.0.0"
#define DEVICE_NAME "Songbird Field Recorder"

// ============================================================================
// Pin Definitions
// ============================================================================

// Buttons
#define BTN_UP_PIN       3     // Start/Stop recording
#define BTN_DOWN_PIN     29    // Play/Pause
#define BTN_LEFT_PIN     28    // Previous file / Gain down / Volume down
#define BTN_RIGHT_PIN    30    // Next file / Gain up / Volume up

// SD Card
#define SDCARD_CS_PIN    10
#define SDCARD_DETECT_PIN 9
#define SDCARD_MOSI_PIN  11
#define SDCARD_MISO_PIN  12
#define SDCARD_SCK_PIN   13

// LEDs
#define LED_BLUE_PIN     35    // Recording indicator
#define LED_PINK_PIN     31    // Audio level/clipping indicator

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
#define AUDIO_BLOCK_SAMPLES     128     // Teensy Audio block size

// Recording settings
#define RECORDING_SAMPLE_RATE   44100   // Teensy Audio Library native rate
#define RECORDING_CHANNELS      1        // Mono for voice recording
#define WAV_BUFFER_SIZE         4096     // Larger buffer for reliability

// Audio levels
#define DEFAULT_MIC_GAIN       10      // 0-63 for SGTL5000
#define MIN_MIC_GAIN           0
#define MAX_MIC_GAIN           63
#define GAIN_STEP              2        // Adjustment per button press

#define DEFAULT_PLAYBACK_VOLUME 0.5     // 0.0-1.0
#define VOLUME_STEP            0.05     // Adjustment per button press
#define MONITOR_VOLUME         0.3      // Input monitoring level during recording

// AGC (Automatic Gain Control) settings for SGTL5000
#define AGC_MAX_GAIN           2        // 0=3dB, 1=6dB, 2=12dB max gain boost (How much the AGC can boost quiet signals)
#define AGC_LVL_SELECT         1        // Target level (0-31, lower = louder) - (The output level AGC tries to maintain)
#define AGC_HARD_LIMIT         0        // 0=disabled, 1=enabled (prevents clipping)
#define AGC_THRESHOLD          -10      // dB below target to activate (-96 to 0) - (How far below target before AGC activates)
#define AGC_ATTACK             0.5      // Attack time (seconds) - (How quickly AGC responds to loud signals)
#define AGC_DECAY              0.5      // Decay time (seconds) - (How quickly AGC recovers after loud signals)

// Wind-cut filter (high-pass at 100Hz)
// Consider 80Hz for less aggressive filtering after field testing
#define WINDCUT_FREQUENCY      100      // Hz
#define WINDCUT_Q              0.707    // Butterworth response

// Clipping detection
#define CLIPPING_THRESHOLD     0.9      // Peak level that triggers clipping indicator
#define CLIPPING_HOLD_MS       500      // How long to show clipping indicator

// ============================================================================
// Timing Constants
// ============================================================================

// Button handling
#define BUTTON_DEBOUNCE_MS     50       // Debounce time for buttons
#define LONG_PRESS_MS          1000     // Long press detection (stop recording)
#define EXTRA_LONG_PRESS_MS    2000     // Extra long press (enable AGC)

// Display updates
#define DISPLAY_UPDATE_MS      100      // Update rate during recording/playback
#define DISPLAY_IDLE_UPDATE_MS 500      // Update rate when idle
#define HINT_DISPLAY_MS        5000     // How long to show AGC hint

// Recording
#define COUNTDOWN_SECONDS      3        // Pre-recording countdown
#define COUNTDOWN_FLASH_MS     200      // LED flash rate during countdown

// Auto-save
#define AUTO_SAVE_INTERVAL_MS  5000     // Flush WAV data every 5 seconds

// AGC hint timing (all in milliseconds)
#define HINT_INTERVAL_1_MIN    60000    // First 5 minutes: every 1 minute
#define HINT_INTERVAL_2_MIN    120000   // Next 55 minutes: every 2 minutes
#define HINT_INTERVAL_10_MIN   600000   // After 1 hour: every 10 minutes
#define HINT_PHASE1_DURATION   300000   // 5 minutes
#define HINT_PHASE2_DURATION   3600000  // 60 minutes total

// ============================================================================
// File System
// ============================================================================

#define RECORDINGS_DIR         "/RECORDINGS"
#define RECORDER_MAX_FILENAME_LEN    32
#define MAX_FILES_TO_SCAN     999       // Maximum recordings to index
#define MAX_SEQUENCE_NUMBER   99999     // Maximum file sequence number

// File naming: REC_NNNNN.WAV (simple sequential)
#define FILE_PREFIX           "REC_"
#define FILE_EXTENSION        ".WAV"

// ============================================================================
// System States
// ============================================================================

enum SystemState {
    STATE_IDLE,
    STATE_COUNTDOWN,
    STATE_RECORDING,
    STATE_PLAYBACK,
    STATE_ERROR
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
    uint8_t version;            // Settings version for migration
    uint8_t micGain;           // Current microphone gain (0-63)
    float playbackVolume;      // Playback volume (0.0-1.0)
    bool agcEnabled;           // Automatic Gain Control on/off
    bool windCutEnabled;       // Wind-cut filter on/off
    uint32_t sequenceNumber;   // Next file sequence number
    uint32_t checksum;         // Simple validity check
};

// EEPROM addresses
#define EEPROM_SETTINGS_ADDR   0
#define SETTINGS_VERSION       1

// ============================================================================
// Display Constants
// ============================================================================

// Display regions (for efficient updates)
#define STATUS_LINE_Y          0
#define FILE_LINE_Y           16
#define METER_WIDTH           50
#define METER_HEIGHT          8

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

#endif // CONFIG_H