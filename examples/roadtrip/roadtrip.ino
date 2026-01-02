/**
 * Roadtrip - Car MP3 Player for Songbird V3
 * 
 * A simple, car-friendly MP3 player with album/playlist folders.
 * 
 * Hardware: Teensy 4.1 + SGTL5000 + LM4811 HP amp + SSD1306 OLED
 * 
 * SD Card Structure:
 *   /01_RoadTrip_Mix/
 *     01_song.mp3
 *     02_song.mp3
 *   /02_Classic_Rock/
 *     01_song.mp3
 *   ...
 * 
 * Controls:
 *   UP     - Short: Next album | Long/Hold: Volume up
 *   DOWN   - Short: Play/Pause | Long/Hold: Volume down
 *   LEFT   - Restart track / Previous track (double-press)
 *   RIGHT  - Next track
 *   
 * LEDs:
 *   LED1 - Pulses while playing
 *   LED2 - Solid when paused, off when playing
 * 
 * Required Libraries:
 *   - Arduino-Teensy-Codec-lib (Frank Boesing)
 *   - Adafruit_SSD1306
 *   - Adafruit_GFX
 */

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <play_sd_mp3.h>
#include <Bounce.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "splash.h"

// ============================================================
// Hardware Pin Definitions
// ============================================================
#define LED_1   35
#define LED_2   31

#define SW_LEFT_PIN   28
#define SW_DOWN_PIN   29
#define SW_UP_PIN      3
#define SW_RIGHT_PIN  30

#define HPAMP_VOL_CLK   52
#define HPAMP_VOL_UD     5
#define HPAMP_SHUTDOWN  45

#define SDCARD_CS_PIN   10

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT    32
#define SCREEN_ADDRESS 0x3C
#define OLED_RESET      -1

// ============================================================
// Configuration
// ============================================================
#define MAX_ALBUMS       32
#define MAX_TRACKS       64
#define MAX_NAME_LEN     64
#define DEBOUNCE_MS      15
#define DOUBLE_PRESS_MS 400
#define RESTART_THRESHOLD_MS 3000
#define LONG_PRESS_MS    400
#define VOLUME_REPEAT_MS 150
#define VOLUME_DISPLAY_MS 1500

// ============================================================
// Audio Objects
// ============================================================
AudioPlaySdMp3       mp3;
AudioMixer4          mixerL;
AudioMixer4          mixerR;
AudioOutputI2S       i2s_out;
AudioConnection      patchCord1(mp3, 0, mixerL, 0);
AudioConnection      patchCord2(mp3, 1, mixerR, 0);
AudioConnection      patchCord3(mixerL, 0, i2s_out, 0);
AudioConnection      patchCord4(mixerR, 0, i2s_out, 1);
AudioControlSGTL5000 codec;

// ============================================================
// Display
// ============================================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);

// ============================================================
// Buttons
// ============================================================
Bounce btnLeft  = Bounce(SW_LEFT_PIN,  DEBOUNCE_MS);
Bounce btnRight = Bounce(SW_RIGHT_PIN, DEBOUNCE_MS);
Bounce btnUp    = Bounce(SW_UP_PIN,    DEBOUNCE_MS);
Bounce btnDown  = Bounce(SW_DOWN_PIN,  DEBOUNCE_MS);

// ============================================================
// Data Structures
// ============================================================
struct Album {
  char name[MAX_NAME_LEN];
};

struct Track {
  char name[MAX_NAME_LEN];
};

// ============================================================
// Player State
// ============================================================
Album albums[MAX_ALBUMS];
Track tracks[MAX_TRACKS];
int albumCount = 0;
int trackCount = 0;
int currentAlbum = 0;
int currentTrack = 0;
bool isPlaying = false;
bool isPaused = false;
unsigned long lastLeftPress = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long playStartTime = 0;
int scrollOffset = 0;
unsigned long lastScrollTime = 0;
unsigned long currentTrackLengthMs = 0;

// Volume control state
int currentVolume = 50;
int currentHpAmpStep = 0;
unsigned long upPressTime = 0;
unsigned long downPressTime = 0;
unsigned long lastVolumeChange = 0;
unsigned long volumeDisplayUntil = 0;
bool upHeld = false;
bool downHeld = false;

// ============================================================
// Forward Declarations
// ============================================================
void scanAlbums();
void loadAlbumTracks(int albumIndex);
void playTrack(int trackIndex);
void nextTrack();
void prevTrack();
void restartTrack();
void nextAlbum();
void prevAlbum();
void togglePause();
void updateDisplay();
void updateLEDs();
void setupHeadphoneAmp();
String getDisplayName(const char* filename);
void volumeUp();
void volumeDown();
void setVolume(int vol);
void showVolumeOverlay();

// ============================================================
// Setup
// ============================================================
void setup() {
  Serial.begin(115200);
  
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  digitalWrite(LED_1, LOW);
  digitalWrite(LED_2, HIGH);
  
  pinMode(SW_LEFT_PIN,  INPUT_PULLUP);
  pinMode(SW_RIGHT_PIN, INPUT_PULLUP);
  pinMode(SW_UP_PIN,    INPUT_PULLUP);
  pinMode(SW_DOWN_PIN,  INPUT_PULLUP);
  
  Wire1.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 failed");
  }
  display.setRotation(2);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Play cinematic splash screen
  playSplashScreen(display);
  
  AudioMemory(20);
  
  if (!codec.enable()) {
    display.clearDisplay();
    display.setCursor(0, 12);
    display.println("CODEC ERROR!");
    display.display();
    while (1);
  }
  
  codec.inputSelect(AUDIO_INPUT_LINEIN);
  codec.lineOutLevel(13);
  codec.unmuteLineout();
  
  mixerL.gain(0, 0.0);
  mixerR.gain(0, 0.0);
  
  SPI.setMOSI(11);
  SPI.setMISO(12);
  SPI.setSCK(13);
  
  if (!SD.begin(SDCARD_CS_PIN)) {
    display.clearDisplay();
    display.setCursor(0, 8);
    display.println("SD CARD ERROR!");
    display.setCursor(0, 20);
    display.println("Insert card & reset");
    display.display();
    while (1);
  }
  
  setupHeadphoneAmp();
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 12);
  display.println("Scanning...");
  display.display();
  
  scanAlbums();
  
  // Ensure scanning message is visible for at least 1 second
  delay(1000);
  
  if (albumCount == 0) {
    display.clearDisplay();
    display.setCursor(0, 4);
    display.println("No albums found!");
    display.setCursor(0, 16);
    display.println("Add folders with");
    display.setCursor(0, 26);
    display.println("MP3 files to SD");
    display.display();
    while (1);
  }
  
  loadAlbumTracks(0);
  
  if (trackCount > 0) {
    playTrack(0);
  }
  
  updateDisplay();
}

// ============================================================
// Main Loop
// ============================================================
void loop() {
  btnLeft.update();
  btnRight.update();
  btnUp.update();
  btnDown.update();
  
  unsigned long now = millis();
  
  if (btnUp.fallingEdge()) {
    upPressTime = now;
    upHeld = false;
  }
  
  if (btnUp.read() == LOW) {
    if (now - upPressTime >= LONG_PRESS_MS) {
      if (!upHeld || (now - lastVolumeChange >= VOLUME_REPEAT_MS)) {
        volumeUp();
        lastVolumeChange = now;
      }
      upHeld = true;
    }
  }
  
  if (btnUp.risingEdge()) {
    if (!upHeld && (now - upPressTime < LONG_PRESS_MS)) {
      nextAlbum();
    }
    upHeld = false;
  }
  
  if (btnDown.fallingEdge()) {
    downPressTime = now;
    downHeld = false;
  }
  
  if (btnDown.read() == LOW) {
    if (now - downPressTime >= LONG_PRESS_MS) {
      if (!downHeld || (now - lastVolumeChange >= VOLUME_REPEAT_MS)) {
        volumeDown();
        lastVolumeChange = now;
      }
      downHeld = true;
    }
  }
  
  if (btnDown.risingEdge()) {
    if (!downHeld && (now - downPressTime < LONG_PRESS_MS)) {
      togglePause();
    }
    downHeld = false;
  }
  
  if (btnRight.fallingEdge()) {
    nextTrack();
  }
  
  if (btnLeft.fallingEdge()) {
    unsigned long playTime = now - playStartTime;
    
    if (now - lastLeftPress < DOUBLE_PRESS_MS) {
      prevTrack();
      lastLeftPress = 0;
    } else if (playTime < RESTART_THRESHOLD_MS) {
      prevTrack();
    } else {
      restartTrack();
    }
    lastLeftPress = now;
  }
  
  if (isPlaying && !isPaused && !mp3.isPlaying()) {
    nextTrack();
  }
  
  if (millis() - lastDisplayUpdate > 100) {
    lastDisplayUpdate = millis();
    updateDisplay();
    updateLEDs();
  }
}

// ============================================================
// Volume Control
// ============================================================
#define VOLUME_DISPLAY_MIN 0
#define VOLUME_DISPLAY_MAX 100
#define HPAMP_STEP_AT_100  2
#define HPAMP_STEP_AT_75   0
#define HPAMP_STEP_AT_50  -5
#define HPAMP_STEP_AT_25  -11
#define LINEOUT_LOUDEST  13
#define LINEOUT_QUIETEST 31

void setVolume(int vol) {
  currentVolume = constrain(vol, VOLUME_DISPLAY_MIN, VOLUME_DISPLAY_MAX);
  
  if (currentVolume == 0) {
    mixerL.gain(0, 0.0);
    mixerR.gain(0, 0.0);
    Serial.println("Volume: 0% (muted)");
  } else {
    int targetHpStep;
    int lineOutLevel = LINEOUT_LOUDEST;
    float mixerGain = 1.0;
    
    if (currentVolume >= 75) {
      targetHpStep = map(currentVolume, 75, 100, HPAMP_STEP_AT_75, HPAMP_STEP_AT_100);
      lineOutLevel = LINEOUT_LOUDEST;
      mixerGain = 1.0;
    } else if (currentVolume >= 50) {
      targetHpStep = map(currentVolume, 50, 75, HPAMP_STEP_AT_50, HPAMP_STEP_AT_75);
      lineOutLevel = LINEOUT_LOUDEST;
      mixerGain = 1.0;
    } else if (currentVolume >= 25) {
      targetHpStep = map(currentVolume, 25, 50, HPAMP_STEP_AT_25, HPAMP_STEP_AT_50);
      lineOutLevel = LINEOUT_LOUDEST;
      mixerGain = 1.0;
    } else if (currentVolume >= 10) {
      targetHpStep = HPAMP_STEP_AT_25;
      lineOutLevel = map(currentVolume, 10, 24, LINEOUT_LOUDEST, LINEOUT_QUIETEST);
      lineOutLevel = constrain(lineOutLevel, LINEOUT_LOUDEST, LINEOUT_QUIETEST);
      mixerGain = 1.0;
    } else {
      targetHpStep = HPAMP_STEP_AT_25;
      lineOutLevel = LINEOUT_QUIETEST;
      mixerGain = currentVolume / 10.0;
    }
    
    mixerL.gain(0, mixerGain);
    mixerR.gain(0, mixerGain);
    setHpAmpToStep(targetHpStep);
    codec.lineOutLevel(lineOutLevel);
    
    Serial.print("Volume: ");
    Serial.print(currentVolume);
    Serial.print("% (HP step: ");
    Serial.print(currentHpAmpStep);
    Serial.print(", lineOut: ");
    Serial.print(lineOutLevel);
    Serial.print(", mixer: ");
    Serial.print(mixerGain, 2);
    Serial.println(")");
  }
  
  volumeDisplayUntil = millis() + VOLUME_DISPLAY_MS;
}

void setHpAmpToStep(int targetStep) {
  targetStep = constrain(targetStep, -11, 4);
  
  while (currentHpAmpStep < targetStep) {
    hpAmpStepUp();
    currentHpAmpStep++;
  }
  while (currentHpAmpStep > targetStep) {
    hpAmpStepDown();
    currentHpAmpStep--;
  }
}

void hpAmpStepUp() {
  digitalWrite(HPAMP_VOL_UD, HIGH);
  digitalWrite(HPAMP_VOL_CLK, LOW);
  delayMicroseconds(100);
  digitalWrite(HPAMP_VOL_CLK, HIGH);
  delayMicroseconds(100);
  digitalWrite(HPAMP_VOL_CLK, LOW);
}

void hpAmpStepDown() {
  digitalWrite(HPAMP_VOL_UD, LOW);
  digitalWrite(HPAMP_VOL_CLK, LOW);
  delayMicroseconds(100);
  digitalWrite(HPAMP_VOL_CLK, HIGH);
  delayMicroseconds(100);
  digitalWrite(HPAMP_VOL_CLK, LOW);
}

void volumeUp() {
  if (currentVolume < VOLUME_DISPLAY_MAX) {
    int step = (currentVolume < 25) ? 1 : 5;
    setVolume(currentVolume + step);
  }
}

void volumeDown() {
  if (currentVolume > VOLUME_DISPLAY_MIN) {
    int step = (currentVolume <= 25) ? 1 : 5;
    setVolume(currentVolume - step);
  }
}

// ============================================================
// Album/Track Management
// ============================================================
void scanAlbums() {
  albumCount = 0;
  File root = SD.open("/");
  
  while (albumCount < MAX_ALBUMS) {
    File entry = root.openNextFile();
    if (!entry) break;
    
    if (entry.isDirectory()) {
      const char* name = entry.name();
      if (name[0] != '.') {
        strncpy(albums[albumCount].name, name, MAX_NAME_LEN - 1);
        albums[albumCount].name[MAX_NAME_LEN - 1] = '\0';
        albumCount++;
        Serial.print("Found album: ");
        Serial.println(name);
      }
    }
    entry.close();
  }
  root.close();
  
  for (int i = 0; i < albumCount - 1; i++) {
    for (int j = i + 1; j < albumCount; j++) {
      if (strcasecmp(albums[i].name, albums[j].name) > 0) {
        Album temp = albums[i];
        albums[i] = albums[j];
        albums[j] = temp;
      }
    }
  }
  
  Serial.print("Total albums: ");
  Serial.println(albumCount);
}

void loadAlbumTracks(int albumIndex) {
  trackCount = 0;
  currentAlbum = albumIndex;
  currentTrack = 0;
  tracks[0].name[0] = '\0';
  
  char path[MAX_NAME_LEN + 2];
  snprintf(path, sizeof(path), "/%s", albums[albumIndex].name);
  
  File dir = SD.open(path);
  if (!dir) {
    Serial.print("Failed to open: ");
    Serial.println(path);
    return;
  }
  
  while (trackCount < MAX_TRACKS) {
    File entry = dir.openNextFile();
    if (!entry) break;
    
    if (!entry.isDirectory()) {
      const char* name = entry.name();
      int len = strlen(name);
      
      if (len > 4 && strcasecmp(name + len - 4, ".mp3") == 0) {
        strncpy(tracks[trackCount].name, name, MAX_NAME_LEN - 1);
        tracks[trackCount].name[MAX_NAME_LEN - 1] = '\0';
        trackCount++;
      }
    }
    entry.close();
  }
  dir.close();
  
  for (int i = 0; i < trackCount - 1; i++) {
    for (int j = i + 1; j < trackCount; j++) {
      if (strcasecmp(tracks[i].name, tracks[j].name) > 0) {
        Track temp = tracks[i];
        tracks[i] = tracks[j];
        tracks[j] = temp;
      }
    }
  }
  
  Serial.print("Loaded ");
  Serial.print(trackCount);
  Serial.print(" tracks from ");
  Serial.println(albums[albumIndex].name);
}

// ============================================================
// Playback Control
// ============================================================
void playTrack(int trackIndex) {
  if (trackIndex < 0 || trackIndex >= trackCount) return;
  
  currentTrack = trackIndex;
  scrollOffset = 0;
  lastScrollTime = millis();
  
  char filepath[MAX_NAME_LEN * 2 + 4];
  snprintf(filepath, sizeof(filepath), "/%s/%s", 
           albums[currentAlbum].name, tracks[currentTrack].name);
  
  Serial.print("Playing: ");
  Serial.println(filepath);
  
  File f = SD.open(filepath);
  if (f) {
    unsigned long fileSize = f.size();
    f.close();
    currentTrackLengthMs = (fileSize * 1000UL) / 24000UL;
    Serial.print("File size: ");
    Serial.print(fileSize);
    Serial.print(" bytes, estimated length: ");
    Serial.print(currentTrackLengthMs / 1000);
    Serial.println(" seconds");
  } else {
    currentTrackLengthMs = 0;
  }
  
  mp3.stop();
  delay(10);
  mp3.play(filepath);
  
  isPlaying = true;
  isPaused = false;
  playStartTime = millis();
  
  updateDisplay();
}

void nextTrack() {
  if (trackCount == 0) return;
  
  int next = currentTrack + 1;
  if (next >= trackCount) {
    int nextAlbumIdx = (currentAlbum + 1) % albumCount;
    loadAlbumTracks(nextAlbumIdx);
    next = 0;
  }
  playTrack(next);
}

void prevTrack() {
  if (trackCount == 0) return;
  
  int prev = currentTrack - 1;
  if (prev < 0) {
    int prevAlbumIdx = (currentAlbum - 1 + albumCount) % albumCount;
    loadAlbumTracks(prevAlbumIdx);
    prev = trackCount - 1;
    if (prev < 0) prev = 0;
  }
  playTrack(prev);
}

void restartTrack() {
  playTrack(currentTrack);
}

void nextAlbum() {
  if (albumCount == 0) return;
  
  mp3.stop();
  int next = (currentAlbum + 1) % albumCount;
  loadAlbumTracks(next);
  
  if (trackCount > 0) {
    playTrack(0);
  } else {
    isPlaying = false;
    updateDisplay();
  }
}

void prevAlbum() {
  if (albumCount == 0) return;
  
  mp3.stop();
  int prev = (currentAlbum - 1 + albumCount) % albumCount;
  loadAlbumTracks(prev);
  
  if (trackCount > 0) {
    playTrack(0);
  } else {
    isPlaying = false;
    updateDisplay();
  }
}

void togglePause() {
  if (!isPlaying) {
    if (trackCount > 0) {
      playTrack(currentTrack);
    }
    return;
  }
  
  isPaused = !isPaused;
  
  if (isPaused) {
    mp3.pause(true);
    Serial.println("Paused");
  } else {
    mp3.pause(false);
    Serial.println("Resumed");
  }
  
  updateDisplay();
}

// ============================================================
// Display
// ============================================================
String getDisplayName(const char* filename) {
  String name = String(filename);
  
  if (name.endsWith(".mp3") || name.endsWith(".MP3")) {
    name = name.substring(0, name.length() - 4);
  }
  
  unsigned int start = 0;
  while (start < name.length() && (isDigit(name[start]) || name[start] == '_' || name[start] == '-' || name[start] == ' ')) {
    start++;
  }
  if (start > 0 && start < name.length()) {
    name = name.substring(start);
  }
  
  name.replace('_', ' ');
  
  return name;
}

void showVolumeOverlay() {
  display.setTextSize(1);
  display.setCursor(40, 0);
  display.print("VOLUME");
  
  display.setTextSize(2);
  int xPos = (currentVolume < 10) ? 52 : (currentVolume < 100) ? 46 : 40;
  display.setCursor(xPos, 10);
  display.print(currentVolume);
  
  display.drawRect(4, 26, 120, 6, SSD1306_WHITE);
  int barWidth = (currentVolume * 116) / 100;
  if (barWidth > 0) {
    display.fillRect(6, 28, barWidth, 2, SSD1306_WHITE);
  }
  
  display.setTextSize(1);
  if (currentVolume == 0) {
    display.setCursor(0, 10);
    display.print("X");
  }
}

void updateDisplay() {
  display.clearDisplay();
  
  if (millis() < volumeDisplayUntil) {
    showVolumeOverlay();
    display.display();
    return;
  }
  
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  String albumDisplay = String(currentAlbum + 1) + "/" + String(albumCount) + " ";
  String albumName = getDisplayName(albums[currentAlbum].name);
  albumDisplay += albumName;
  
  if (albumDisplay.length() > 21) {
    albumDisplay = albumDisplay.substring(0, 20) + "~";
  }
  display.println(albumDisplay);
  
  display.setCursor(0, 10);
  
  if (trackCount == 0 || tracks[currentTrack].name[0] == '\0') {
    display.println("Loading...");
  } else {
    String trackName = getDisplayName(tracks[currentTrack].name);
    
    if (trackName.length() > 21) {
      if (millis() - lastScrollTime > 300) {
        lastScrollTime = millis();
        scrollOffset++;
        if (scrollOffset > (int)trackName.length() - 18) {
          scrollOffset = -3;
        }
      }
      int offset = max(0, scrollOffset);
      trackName = trackName.substring(offset, offset + 21);
    }
    display.println(trackName);
  }
  
  display.setCursor(0, 20);
  
  String trackInfo = String(currentTrack + 1) + "/" + String(trackCount);
  display.print(trackInfo);
  
  display.setCursor(36, 20);
  if (isPaused) {
    display.print("||");
  } else if (isPlaying) {
    display.print("> ");
  } else {
    display.print("[]");
  }
  
  if (isPlaying) {
    unsigned long elapsedMs = millis() - playStartTime;
    int totalSecs = elapsedMs / 1000;
    int mins = totalSecs / 60;
    int secs = totalSecs % 60;
    
    display.setCursor(50, 20);
    if (mins < 10) display.print("0");
    display.print(mins);
    display.print(":");
    if (secs < 10) display.print("0");
    display.print(secs);
  } else {
    display.setCursor(50, 20);
    display.print("--:--");
  }
  
  // Joust-style bird animation
  if (isPlaying && !isPaused) {
    int cycleTime = 4000;
    int phase = (millis() % cycleTime);
    int xPos;
    float yPos;
    int yBase = 23;  // Centered on bottom line
    bool facingRight;
    
    if (phase < cycleTime / 2) {
      xPos = 90 + (phase * 28) / (cycleTime / 2);
      facingRight = true;
    } else {
      xPos = 118 - ((phase - cycleTime / 2) * 28) / (cycleTime / 2);
      facingRight = false;
    }
    
    // Smooth flying motion - gentle sine wave centered on baseline
    int flapCycle = 600;  // Time for one full flap cycle
    int flapPhase = (millis() % flapCycle);
    
    // Smooth sine wave motion, centered on yBase
    float angle = (flapPhase / (float)flapCycle) * 2.0 * 3.14159;
    float lift = sin(angle) * 2.0;  // +/- 2 pixels from center
    
    // Wing position based on vertical velocity (derivative of sine is cosine)
    bool wingUp = cos(angle) < 0;  // Wing up when moving down
    
    int yHop = yBase + (int)lift;
    
    if (facingRight) {
      // Body (smaller, 3x2)
      display.fillRect(xPos, yHop + 2, 3, 2, SSD1306_WHITE);
      // Head
      display.fillRect(xPos + 2, yHop + 1, 2, 2, SSD1306_WHITE);
      // Beak
      display.drawPixel(xPos + 4, yHop + 1, SSD1306_WHITE);
      // Eye
      display.drawPixel(xPos + 3, yHop + 1, SSD1306_BLACK);
      // Tail
      display.drawPixel(xPos - 1, yHop + 2, SSD1306_WHITE);
      // Wing (more dramatic flap)
      if (wingUp) {
        display.drawPixel(xPos + 1, yHop + 1, SSD1306_WHITE);
        display.drawPixel(xPos + 1, yHop, SSD1306_WHITE);
        display.drawPixel(xPos, yHop - 1, SSD1306_WHITE);
        display.drawPixel(xPos + 1, yHop - 1, SSD1306_WHITE);
      } else {
        display.drawPixel(xPos + 1, yHop + 4, SSD1306_WHITE);
        display.drawPixel(xPos, yHop + 5, SSD1306_WHITE);
        display.drawPixel(xPos + 1, yHop + 5, SSD1306_WHITE);
      }
      // Legs (tucked while flying)
      display.drawPixel(xPos + 1, yHop + 4, SSD1306_WHITE);
    } else {
      // Body (smaller, 3x2)
      display.fillRect(xPos, yHop + 2, 3, 2, SSD1306_WHITE);
      // Head
      display.fillRect(xPos - 1, yHop + 1, 2, 2, SSD1306_WHITE);
      // Beak
      display.drawPixel(xPos - 2, yHop + 1, SSD1306_WHITE);
      // Eye
      display.drawPixel(xPos - 1, yHop + 1, SSD1306_BLACK);
      // Tail
      display.drawPixel(xPos + 3, yHop + 2, SSD1306_WHITE);
      // Wing (more dramatic flap)
      if (wingUp) {
        display.drawPixel(xPos + 1, yHop + 1, SSD1306_WHITE);
        display.drawPixel(xPos + 1, yHop, SSD1306_WHITE);
        display.drawPixel(xPos + 2, yHop - 1, SSD1306_WHITE);
        display.drawPixel(xPos + 1, yHop - 1, SSD1306_WHITE);
      } else {
        display.drawPixel(xPos + 1, yHop + 4, SSD1306_WHITE);
        display.drawPixel(xPos + 2, yHop + 5, SSD1306_WHITE);
        display.drawPixel(xPos + 1, yHop + 5, SSD1306_WHITE);
      }
      // Legs (tucked while flying)
      display.drawPixel(xPos + 1, yHop + 4, SSD1306_WHITE);
    }
  }
  
  display.display();
}

// ============================================================
// LEDs
// ============================================================
void updateLEDs() {
  if (isPaused) {
    digitalWrite(LED_1, LOW);
    digitalWrite(LED_2, HIGH);
  } else if (isPlaying) {
    int brightness = (millis() / 4) % 256;
    if (brightness > 127) brightness = 255 - brightness;
    analogWrite(LED_1, brightness);
    digitalWrite(LED_2, LOW);
  } else {
    digitalWrite(LED_1, LOW);
    digitalWrite(LED_2, LOW);
  }
}

// ============================================================
// Headphone Amplifier Setup
// ============================================================
void setupHeadphoneAmp() {
  pinMode(HPAMP_VOL_CLK, OUTPUT);
  pinMode(HPAMP_VOL_UD, OUTPUT);
  pinMode(HPAMP_SHUTDOWN, OUTPUT);
  
  digitalWrite(HPAMP_VOL_CLK, LOW);
  digitalWrite(HPAMP_VOL_UD, LOW);
  digitalWrite(HPAMP_SHUTDOWN, HIGH);
  delay(100);
  
  digitalWrite(HPAMP_SHUTDOWN, LOW);
  
  currentHpAmpStep = 0;
  
  setVolume(currentVolume);
  
  Serial.println("Headphone amp initialized");
}