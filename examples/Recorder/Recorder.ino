/*
 * Recorder.ino
 * 
 * Songbird Phone Call Recorder - Main Application File
 * 
 * Simple phone call recorder demonstrating:
 * - Recording phone calls to SD card in WAV format
 * - Playing back recorded calls through headset
 * - Basic file management and navigation
 * - Real-time audio level monitoring
 * 
 * Hardware Setup:
 * - Phone connects via USB-C (provides power and audio interface)
 * - Headset connects via 3.5mm jack (microphone and headphones)
 * - SD card for storage
 * - OLED display for status
 * - 4 buttons for control
 */

 #include "Recorder.h"

// Global state variables
RecorderState currentState = STATE_IDLE;
unsigned long recordStartTime = 0;
int currentFileIndex = 0;
int totalFiles = 0;
String currentFilename = "";
bool sdCardReady = false;
File recordingFile;
uint32_t recordingBytesWritten = 0;

// Audio objects
AudioInputUSB inputFromPhone;
AudioInputI2S inputFromHeadset;
AudioOutputUSB outputToPhone;
AudioOutputI2S outputToHeadset;
AudioMixer4 phoneMixer;
AudioMixer4 phoneOutputMixer;
AudioMixer4 leftHeadphonesMixer;
AudioMixer4 rightHeadphonesMixer;
AudioRecordQueue recordQueue;
AudioPlaySdWav playWav;
AudioAnalyzeRMS inputLevel;
AudioControlSGTL5000 audioShield;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);
AudioSynthWaveformSine recordBeep;


void setup() 
{
  Serial.begin(9600);
  Serial.println("Songbird Phone Call Recorder Starting...");

  // Initialize hardware pins

  // LEDs
  pinMode(LED_1_PIN, OUTPUT);
  pinMode(LED_2_PIN, OUTPUT);

  // Buttons
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_LEFT_PIN, INPUT_PULLUP);
  pinMode(BTN_RIGHT_PIN, INPUT_PULLUP);

  // Turn off LEDs initially
  digitalWrite(LED_1_PIN, LOW);
  digitalWrite(LED_2_PIN, LOW);

  // Initialize I2C for display
  Wire1.begin();
  Wire1.setSDA(OLED_SDA_PIN);
  Wire1.setSCL(OLED_SCL_PIN);

  setupAudioProcessing();
  initializeSDCard();
  initializeDisplay();
  updateDisplay();
  
  Serial.println("Songbird Recorder is ready.");
  Serial.println("Commands: LIST, DELETE filename.wav, DELETEALL, HELP");
}

void loop() 
{
  // Handle button presses
  handleButtons();

  // Handle active recording
  if (currentState == STATE_RECORDING) 
  {
    // static unsigned long lastReminderBeep = 0;
    // if (millis() - lastReminderBeep > 30000) {  // Every 30 seconds
    //   playRecordingBeep();
    //   lastReminderBeep = millis();
    // }

    handleRecording();
  }

  // Check if playback is finished
  if (currentState == STATE_PLAYBACK && !playWav.isPlaying()) 
  {
    stopPlayback();
  }

  // Update the display periodically
  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate > 250) 
  {
    // Updates 4 times per second
    updateDisplay();
    lastDisplayUpdate = millis();
  }

  // Check SD card status periodically
  static unsigned long lastSDCheck = 0;
  if (millis() - lastSDCheck > 2000)  // Check every 2 seconds
  {
    checkSDCardStatus();
    lastSDCheck = millis();
  }

  handleSerialCommands();

  // Update LED brightness based on audio level
  if (currentState == STATE_RECORDING) 
  {
    float level = getInputLevel();
    analogWrite(LED_1_PIN, (int)(level * 255));
  }
  else 
  {
    digitalWrite(LED_1_PIN, LOW);
  }

  // Playback LED
  if (currentState == STATE_PLAYBACK) 
  {
    digitalWrite(LED_2_PIN, HIGH);
  }
  else 
  {
    digitalWrite(LED_2_PIN, LOW);
  }
}
