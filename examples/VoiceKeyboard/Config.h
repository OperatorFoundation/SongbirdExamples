/*
 * Config.h - Configuration for VoiceInterface
 *
 * Pin definitions, audio configuration, and timing constants
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Version info
#define FIRMWARE_VERSION "1.0.0"
#define DEVICE_NAME "VoiceInterface"

// ============================================================================
// Pin Definitions
// ============================================================================

// Buttons
#define BTN_UP_PIN       3     // PTT (Push-to-Talk)

// SD Card
#define SDCARD_CS_PIN    10
#define SDCARD_DETECT_PIN 9
#define SDCARD_MOSI_PIN  11
#define SDCARD_MISO_PIN  12
#define SDCARD_SCK_PIN   13

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

#define DEFAULT_PLAYBACK_VOLUME 0.5     // 0.0-1.0
#define MONITOR_VOLUME         0.3      // Input monitoring level during recording

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
#define LONG_PRESS_MS          1000     // Long press detection (for future use)

// ============================================================================
// File System
// ============================================================================

#define MAX_FILES_TO_SCAN     999       // Maximum recordings to index
#define MAX_SEQUENCE_NUMBER   99999     // Maximum file sequence number

// ============================================================================
// System States
// ============================================================================

enum SystemState {
    STATE_IDLE,          // Connected, listening
    STATE_RECORDING,     // PTT held, recording
    STATE_PLAYING,       // Playing received message
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
// VoiceInterface Configuration
// ============================================================================

// Serial communication
#define SERIAL_BAUD_RATE       115200

// File system
#define TX_DIR                "/TX"      // Outgoing messages
#define RX_DIR                "/RX"      // Incoming messages

#endif // CONFIG_H
