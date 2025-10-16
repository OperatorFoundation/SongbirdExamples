/*
 * UkuleleTuner.ino - Songbird Ukulele Tuner
 *
 * A chromatic tuner for ukulele using the Songbird hardware
 *
 * Features:
 * - Real-time pitch detection using AudioAnalyzeNoteFrequency
 * - Multiple tuning modes (Standard GCEA, Low G, Baritone)
 * - Visual feedback on OLED display
 * - LED indicators for signal level and tuning accuracy
 *
 * Button controls:
 * - LEFT: Previous string
 * - RIGHT: Next string
 * - UP: Change tuning mode
 * - DOWN: Adjust reference pitch (A=440Hz)
 *
 * Version: 1.0.0
 * Author: Operator Foundation
 * License: MIT
 */

#include <Arduino.h>
#include <Wire.h>
#include <Audio.h>

// Include modules we're keeping
#include "Config.h"
#include "DisplayManager.h"
#include "UIController.h"
#include "LEDControl.h"

const float STANDARD_TUNING[NUM_STRINGS] = {
    392.00,  // String 4 (leftmost): G4
    261.63,  // String 3: C4
    329.63,  // String 2: E4
    440.00   // String 1 (rightmost): A4
};

const char* STANDARD_NOTES[NUM_STRINGS] = {"G4", "C4", "E4", "A4"};

// Low G tuning (only string 4 changes)
const float LOW_G_TUNING[NUM_STRINGS] = {
    196.00,  // String 4: G3 (low G)
    261.63,  // String 3: C4
    329.63,  // String 2: E4
    440.00   // String 1: A4
};

// Baritone DGBE tuning
const float BARITONE_TUNING[NUM_STRINGS] = {
    73.42,   // String 4: D2
    98.00,   // String 3: G2
    123.47,  // String 2: B2
    164.81   // String 1: E3
};

const char* BARITONE_NOTES[NUM_STRINGS] = {"D2", "G2", "B2", "E3"};

// =============================================================================
// Audio Chain Setup
// =============================================================================

// Audio objects
AudioInputI2S            i2s_input;
AudioAnalyzeNoteFrequency noteDetector;
AudioAnalyzePeak         peakDetector;
AudioConnection          patchCord1(i2s_input, 0, noteDetector, 0);
AudioConnection          patchCord2(i2s_input, 0, peakDetector, 0);
AudioControlSGTL5000     audioShield;

// =============================================================================
// Global Objects
// =============================================================================

DisplayManager display;
UIController ui;
LEDControl leds;

// =============================================================================
// Tuner State
// =============================================================================

TuningMode currentMode = MODE_STANDARD_GCEA;
uint8_t currentString = 0;  // 0-3, which string we're tuning
float referencePitch = CONCERT_A;

// Detected note info
float detectedFreq = 0.0;
float probability = 0.0;
char detectedNoteName[4] = "";
float centsOffset = 0.0;
bool inTune = false;

// Signal level
float signalLevel = 0.0;

// Timing
elapsedMillis displayUpdateTimer = 0;

// =============================================================================
// Forward Declarations
// =============================================================================

void updateTuner();
void updateDisplay();
void handleButtons();
void calculateTuning();
float getTargetFrequency();
const char* getTargetNote();
void frequencyToNote(float freq, char* noteName, int& octave);
float calculateCentsOffset(float detected, float target);
uint8_t getTuningAccuracyLED();
uint8_t getSignalLevelLED();

// =============================================================================
// Setup
// =============================================================================

void setup() {
    #ifdef DEBUG_MODE
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {}
    
    Serial.println("\n========================================");
    Serial.println("Songbird Ukulele Tuner");
    Serial.println("Version: " FIRMWARE_VERSION);
    Serial.println("========================================\n");
    #endif

    // Initialize I2C for display
    Wire1.begin();
    Wire1.setSDA(OLED_SDA_PIN);
    Wire1.setSCL(OLED_SCL_PIN);

    // Initialize display
    if (!display.begin()) {
        DEBUG_PRINTLN("Display initialization failed");
    }

    display.showStartupScreen();
    display.update();
    delay(2000);

    // Initialize UI
    ui.begin();
    ui.update();

    // Initialize LEDs
    leds.begin();

    // Initialize audio system
    AudioMemory(30);
    
    audioShield.enable();
    audioShield.inputSelect(AUDIO_INPUT_LINEIN);  // Using line in for mic
    audioShield.micGain(DEFAULT_MIC_GAIN);
    audioShield.volume(0.0);  // No playback needed
    
    // Configure AudioAnalyzeNoteFrequency
    // threshold: minimum probability to consider valid (0.0-1.0)
    noteDetector.begin(0.15);  // 15% confidence threshold
    
    DEBUG_PRINTLN("Audio system initialized");
    DEBUG_PRINTLN("Ready to tune!\n");
}

// =============================================================================
// Main Loop
// =============================================================================

void loop() {
    // Update UI
    ui.update();
    
    // Update LED animations
    leds.update();
    
    // Handle button presses
    handleButtons();
    
    // Update tuner analysis
    updateTuner();
    
    // Update display
    if (displayUpdateTimer >= DISPLAY_UPDATE_MS) {
        displayUpdateTimer = 0;
        updateDisplay();
    }
}

// =============================================================================
// Tuner Processing
// =============================================================================

void updateTuner() {
    // Check for new note detection
    if (noteDetector.available()) {
        detectedFreq = noteDetector.read();
        probability = noteDetector.probability();
        
        DEBUG_PRINTF("Freq: %.2f Hz, Probability: %.2f\n", detectedFreq, probability);
        
        // Only process if we have decent confidence
        if (probability > 0.9 && detectedFreq >= DETECTION_MIN_FREQ && detectedFreq <= DETECTION_MAX_FREQ) {
            calculateTuning();
        } else {
            // No valid signal
            detectedNoteName[0] = '\0';
            centsOffset = 0.0;
            inTune = false;
        }
    }
    
    // Update signal level
    if (peakDetector.available()) {
        signalLevel = peakDetector.read();
    }
    
    // Update LEDs
    leds.setPinkLED(getTuningAccuracyLED());
    leds.setBlueLED(getSignalLevelLED() > 0);  // Just on/off for now
}

void calculateTuning() {
    // Convert frequency to note name
    int octave;
    char noteName[3];
    frequencyToNote(detectedFreq, noteName, octave);
    snprintf(detectedNoteName, sizeof(detectedNoteName), "%s%d", noteName, octave);
    
    // Get target frequency for current string
    float targetFreq = getTargetFrequency();
    
    // Calculate cents offset
    centsOffset = calculateCentsOffset(detectedFreq, targetFreq);
    
    // Check if in tune
    inTune = (fabs(centsOffset) <= TUNING_TOLERANCE_CENTS);
    
    DEBUG_PRINTF("Detected: %s (%.2f Hz), Target: %s (%.2f Hz), Cents: %.1f\n",
                 detectedNoteName, detectedFreq, getTargetNote(), targetFreq, centsOffset);
}

// =============================================================================
// Helper Functions
// =============================================================================

float getTargetFrequency() {
    const float* tuning;
    
    switch (currentMode) {
        case MODE_STANDARD_GCEA:
            tuning = STANDARD_TUNING;
            break;
        case MODE_LOW_G:
            tuning = LOW_G_TUNING;
            break;
        case MODE_BARITONE:
            tuning = BARITONE_TUNING;
            break;
        case MODE_CHROMATIC:
            // In chromatic mode, snap to nearest semitone
            {
                float semitones = 12.0 * log2(detectedFreq / referencePitch);
                int nearestSemitone = round(semitones);
                return referencePitch * pow(2.0, nearestSemitone / 12.0);
            }
        default:
            tuning = STANDARD_TUNING;
    }
    
    return tuning[currentString];
}

const char* getTargetNote() {
    if (currentMode == MODE_CHROMATIC) {
        return detectedNoteName;
    }
    
    if (currentMode == MODE_BARITONE) {
        return BARITONE_NOTES[currentString];
    }
    
    return STANDARD_NOTES[currentString];
}

void frequencyToNote(float freq, char* noteName, int& octave) {
    const char* NOTE_NAMES[12] = {
        "C", "C#", "D", "D#", "E", "F", 
        "F#", "G", "G#", "A", "A#", "B"
    };
    
    // Calculate semitones from A4 (440 Hz)
    float semitones = 12.0 * log2(freq / referencePitch);
    int semitonesFromA4 = round(semitones);
    
    // Calculate note index (0-11 where 0=C, 9=A)
    int noteIndex = (semitonesFromA4 + 9) % 12;
    if (noteIndex < 0) noteIndex += 12;
    
    // Calculate octave
    octave = 4 + (semitonesFromA4 + 9) / 12;
    if (semitonesFromA4 + 9 < 0 && (semitonesFromA4 + 9) % 12 != 0) {
        octave--;
    }
    
    strcpy(noteName, NOTE_NAMES[noteIndex]);
}

float calculateCentsOffset(float detected, float target) {
    return 1200.0 * log2(detected / target);
}

uint8_t getTuningAccuracyLED() {
    if (detectedNoteName[0] == '\0') {
        return LED_OFF;
    }
    
    float absCents = fabs(centsOffset);
    
    if (absCents > 20.0) return LED_OFF;
    if (absCents > 10.0) return LED_DIM;
    if (absCents > 5.0) return LED_MEDIUM;
    if (absCents > 2.0) return LED_BRIGHT;
    
    return LED_MAX;  // In tune!
}

uint8_t getSignalLevelLED() {
    // Map signal level (0.0-1.0) to LED brightness (0-255)
    return (uint8_t)(signalLevel * 255.0);
}

// =============================================================================
// Button Handling
// =============================================================================

void handleButtons() {
    // LEFT - Previous string
    if (ui.wasJustPressed(BTN_LEFT)) {
        currentString = (currentString == 0) ? (NUM_STRINGS - 1) : (currentString - 1);
        DEBUG_PRINTF("String: %d\n", currentString);
    }
    
    // RIGHT - Next string
    if (ui.wasJustPressed(BTN_RIGHT)) {
        currentString = (currentString + 1) % NUM_STRINGS;
        DEBUG_PRINTF("String: %d\n", currentString);
    }
    
    // UP - Change tuning mode
    if (ui.wasJustPressed(BTN_UP)) {
        currentMode = (TuningMode)((currentMode + 1) % MODE_COUNT);
        DEBUG_PRINTF("Mode: %d\n", currentMode);
    }
    
    // DOWN - Adjust reference pitch (cycles through common values)
    if (ui.wasJustPressed(BTN_DOWN)) {
        // Cycle: 440 -> 442 -> 438 -> 440
        if (referencePitch == 440.0) referencePitch = 442.0;
        else if (referencePitch == 442.0) referencePitch = 438.0;
        else referencePitch = 440.0;
        
        DEBUG_PRINTF("Reference pitch: %.1f Hz\n", referencePitch);
    }
}

// =============================================================================
// Display Update
// =============================================================================

void updateDisplay() {
    const char* modeNames[] = {"STANDARD", "LOW G", "BARITONE", "CHROMATIC"};
    
    bool hasSignal = (detectedNoteName[0] != '\0');
    
    display.showTunerScreen(
        modeNames[currentMode],
        currentString + 1,
        detectedNoteName,
        getTargetNote(),
        centsOffset,
        inTune,
        hasSignal
    );
    
    display.update();
}
