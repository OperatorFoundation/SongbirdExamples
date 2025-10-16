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
#define DEVICE_NAME "Ukulele Tuner"

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
#define LED_BLUE_PIN     35    // Signal level indicator
#define LED_PINK_PIN     31    // Tuning accuracy indicator

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

// AGC hint timing (all in milliseconds)
#define HINT_INTERVAL_1_MIN    60000    // First 5 minutes: every 1 minute
#define HINT_INTERVAL_2_MIN    120000   // Next 55 minutes: every 2 minutes
#define HINT_INTERVAL_10_MIN   600000   // After 1 hour: every 10 minutes
#define HINT_PHASE1_DURATION   300000   // 5 minutes
#define HINT_PHASE2_DURATION   3600000  // 60 minutes total

// Display regions
#define NOTE_DISPLAY_Y         0
#define TARGET_DISPLAY_Y       16
#define CENTS_DISPLAY_Y        24
#define METER_WIDTH           50

// ============================================================================
// System States
// ============================================================================

enum SystemState {
  STATE_TUNING,
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
// Tuner Configuration
// ============================================================================

// FFT settings
#define FFT_SIZE               1024     // FFT bins (gives ~43Hz resolution at 44.1kHz)
#define FFT_OVERLAP            2        // Process every other block for speed
#define MIN_SIGNAL_THRESHOLD   0.01     // Minimum peak level to attempt detection

// Pitch detection
#define CONCERT_A              440.0    // Reference pitch in Hz
#define CENTS_PER_SEMITONE     100
#define TUNING_TOLERANCE_CENTS 2        // Within this = "in tune"
#define DETECTION_MIN_FREQ     65.0     // C2 - lowest detection
#define DETECTION_MAX_FREQ     2000.0   // Upper detection limit

// String definitions for Standard GCEA (High G)
#define NUM_STRINGS            4

// Tuning modes
enum TuningMode {
    MODE_STANDARD_GCEA,
    MODE_LOW_G,
    MODE_BARITONE,
    MODE_CHROMATIC,
    MODE_COUNT  // Number of modes
};

// LED brightness levels for tuning accuracy
#define LED_OFF                0
#define LED_DIM                32
#define LED_MEDIUM             128
#define LED_BRIGHT             200
#define LED_MAX                255

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
