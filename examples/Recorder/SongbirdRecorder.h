/*
 * SongbirdRecorder.h
 * 
 * Header file for Songbird Phone Call Recorder
 * Contains all constants, pin definitions, and shared declarations
 */

#ifndef SONGBIRD_RECORDER_H
#define SONGBIRD_RECORDER_H

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Hardware pin definitions

// Headphone amplifier pins
#define HPAMP_VOL_CLK    52
#define HPAMP_VOL_UD     5
#define HPAMP_SHUTDOWN   45

// Buttons
#define BTN_RIGHT_PIN    30    // Next file
#define BTN_DOWN_PIN     29    // Play/Pause
#define BTN_UP_PIN       3     // Start/Stop recording
#define BTN_LEFT_PIN     28    // Previous file

// SD card
#define SDCARD_DETECT_PIN 9
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  11
#define SDCARD_MISO_PIN  12
#define SDCARD_SCK_PIN   13

// LED
#define LED_1_PIN        35   // Recording indicator (Blue)
#define LED_2_PIN        31   // Playback indicator (Pink)

// Display
#define OLED_SCL_PIN     16
#define OLED_SDA_PIN     17

// Display constants
#define SCREEN_WIDTH      128
#define SCREEN_HEIGHT     32
#define OLED_RESET        -1
#define OLED_ADDRESS      0x3C

// Audio constants
#define AUDIO_MEMORY_BLOCKS     60
#define RECORDING_SAMPLE_RATE   44100
#define AUDIO_BITS_PER_SAMPLE   16
#define AUDIO_CHANNELS          1  // Mono recording
#define BEEP_FREQUENCY 1000        // 1kHz beep tone
#define BEEP_AMPLITUDE 0.3         // Beep volume (0.0 to 1.0)
#define BEEP_DURATION_MS 200       // Length of beep in milliseconds

// Recording constants
#define MAX_RECORDING_TIME_MS   600000  // 10 minutes
#define RECORDING_BUFFER_SIZE   512
#define AUDIO_BLOCK_SIZE        256          // The Teensy Audio Library works with fixed-size audio blocks (128 samples × 2 bytes per sample)

#define CALLS_DIRECTORY         "CALLS"

// Button debounce
#define BUTTON_DEBOUNCE_MS      50

// Audio levels
#define PHONE_AUDIO_LEVEL       0.8f
#define HEADSET_AUDIO_LEVEL     0.5f
#define PLAYBACK_AUDIO_LEVEL    0.3f
#define RECORDING_MIX_LEVEL     0.5f

// System states
enum RecorderState {
  STATE_IDLE,
  STATE_RECORDING,
  STATE_PLAYBACK
};

// WAV file header structure
struct WAVHeader {
  char riff[4];           // "RIFF"
  uint32_t fileSize;      // File size - 8
  char wave[4];           // "WAVE"
  char fmt[4];            // "fmt "
  uint32_t fmtSize;       // Format chunk size (16)
  uint16_t audioFormat;   // Audio format (1 = PCM)
  uint16_t numChannels;   // Number of channels
  uint32_t sampleRate;    // Sample rate
  uint32_t byteRate;      // Byte rate
  uint16_t blockAlign;    // Block align
  uint16_t bitsPerSample; // Bits per sample
  char data[4];           // "data"
  uint32_t dataSize;      // Data size
};

// Global variables (extern declarations)
extern RecorderState currentState;
extern unsigned long recordStartTime;
extern int currentFileIndex;
extern int totalFiles;
extern String currentFilename;
extern bool sdCardReady;
extern File recordingFile;
extern uint32_t recordingBytesWritten;

// Audio objects (extern declarations)

// Input need to be declared before Outputs
extern AudioInputUSB inputFromPhone;
extern AudioInputI2S inputFromHeadset;

extern AudioOutputUSB outputToPhone;
extern AudioOutputI2S outputToHeadset;

extern AudioMixer4 phoneMixer;
extern AudioMixer4 phoneOutputMixer;
extern AudioMixer4 leftHeadphonesMixer;
extern AudioMixer4 rightHeadphonesMixer;

extern AudioRecordQueue recordQueue;
extern AudioPlaySdWav playWav;
extern AudioAnalyzeRMS inputLevel;
extern AudioControlSGTL5000 audioShield;
extern Adafruit_SSD1306 display;
extern AudioSynthWaveformSine recordBeep;

// Function declarations
// From audio_manager.ino
void setupAudioProcessing();
void startRecording();
void stopRecording();
void handleRecording();
void startPlayback();
void stopPlayback();
float getInputLevel();

// From sd_manager.ino
void initializeSDCard();
void scanForFiles();
void loadCurrentFilename();
void createWAVFile(String filename);
void writeWAVData(byte* buffer, int length);
void finalizeWAVFile();
void listFiles();
void deleteFile(String filename);
void deleteAll();

// From display_manager.ino
void initializeDisplay();
void updateDisplay();
void drawProgressBar(int x, int y, int width, int height, float level);

// From button_manager.ino
void handleButtons();
void handleUpButton();
void handleDownButton();
void handleLeftButton();
void handleRightButton();

// From serial_manager.ino
void handleSerialCommands();

#endif