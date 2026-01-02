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

// Volume control state
int currentVolume = 40;  // Start at reasonable level (0-100)
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
  
  // LEDs
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  digitalWrite(LED_1, LOW);
  digitalWrite(LED_2, HIGH);
  
  // Buttons
  pinMode(SW_LEFT_PIN,  INPUT_PULLUP);
  pinMode(SW_RIGHT_PIN, INPUT_PULLUP);
  pinMode(SW_UP_PIN,    INPUT_PULLUP);
  pinMode(SW_DOWN_PIN,  INPUT_PULLUP);
  
  // Display
  Wire1.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 failed");
  }
  display.setRotation(2);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 12);
  display.println("  ROADTRIP MP3");
  display.display();
  
  // Audio
  AudioMemory(20);
  
  if (!codec.enable()) {
    display.clearDisplay();
    display.setCursor(0, 12);
    display.println("CODEC ERROR!");
    display.display();
    while (1);
  }
  
  // Configure codec for line out to external headphone amp
  // This matches the working FieldRecorder configuration
  codec.inputSelect(AUDIO_INPUT_LINEIN);  // Select an input to avoid floating noise
  codec.lineOutLevel(13);                  // 13 = 3.16Vpp (loudest setting)
  codec.unmuteLineout();                   // Make sure line out is enabled
  
  // Initialize mixer gains - this controls our volume
  setVolume(currentVolume);
  
  // SD Card
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
  
  // Headphone amp
  setupHeadphoneAmp();
  
  // Scan for albums
  display.clearDisplay();
  display.setCursor(0, 12);
  display.println("Scanning albums...");
  display.display();
  
  scanAlbums();
  
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
  
  // Load first album
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
  
  // --- UP button: short = next album, long/hold = volume up ---
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
  
  // --- DOWN button: short = play/pause, long/hold = volume down ---
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
  
  // --- RIGHT button: next track ---
  if (btnRight.fallingEdge()) {
    nextTrack();
  }
  
  // --- LEFT button: restart/previous track ---
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
  
  // Check if track ended
  if (isPlaying && !isPaused && !mp3.isPlaying()) {
    nextTrack();
  }
  
  // Update display
  if (millis() - lastDisplayUpdate > 100) {
    lastDisplayUpdate = millis();
    updateDisplay();
    updateLEDs();
  }
}

// ============================================================
// Volume Control
// ============================================================
// Mixer gain range - constrained to safe listening levels
#define VOLUME_GAIN_MAX  0.15  // Max gain at 100% volume

void setVolume(int vol) {
  currentVolume = constrain(vol, 0, 100);
  
  // Use exponential curve for more usable volume range
  // This gives finer control at low volumes where ears are more sensitive
  // Formula: gain = MAX * (e^(k*x) - 1) / (e^k - 1)
  // where x is 0-1 and k controls the curve steepness
  
  float x = currentVolume / 100.0;
  float gain;
  
  if (currentVolume == 0) {
    gain = 0.0;
  } else {
    // Attempt exposure curve: k=4 gives good low-end detail
    // At 50% input, this yields roughly 5% of max output
    const float k = 4.0;
    gain = VOLUME_GAIN_MAX * (exp(k * x) - 1.0) / (exp(k) - 1.0);
  }
  
  mixerL.gain(0, gain);
  mixerR.gain(0, gain);
  
  volumeDisplayUntil = millis() + VOLUME_DISPLAY_MS;
  Serial.print("Volume: ");
  Serial.print(currentVolume);
  Serial.print("% (gain: ");
  Serial.print(gain, 4);
  Serial.println(")");
}

void volumeUp() {
  setVolume(currentVolume + 5);
}

void volumeDown() {
  setVolume(currentVolume - 5);
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
  
  // Sort albums alphabetically
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
  
  // Clear track name so old track doesn't show
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
  
  // Sort tracks alphabetically
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
  
  // Remove .mp3 extension
  if (name.endsWith(".mp3") || name.endsWith(".MP3")) {
    name = name.substring(0, name.length() - 4);
  }
  
  // Remove leading numbers and separators
  unsigned int start = 0;
  while (start < name.length() && (isDigit(name[start]) || name[start] == '_' || name[start] == '-' || name[start] == ' ')) {
    start++;
  }
  if (start > 0 && start < name.length()) {
    name = name.substring(start);
  }
  
  // Replace underscores with spaces
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
  
  // Volume bar at bottom
  display.drawRect(4, 26, 120, 6, SSD1306_WHITE);
  int barWidth = (currentVolume * 116) / 100;
  display.fillRect(6, 28, barWidth, 2, SSD1306_WHITE);
  
  display.setTextSize(1);
  if (currentVolume == 0) {
    display.setCursor(0, 10);
    display.print("X");
  }
}

void updateDisplay() {
  display.clearDisplay();
  
  // Show volume overlay if recently changed
  if (millis() < volumeDisplayUntil) {
    showVolumeOverlay();
    display.display();
    return;
  }
  
  // Line 1: Album name
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  String albumDisplay = String(currentAlbum + 1) + "/" + String(albumCount) + " ";
  String albumName = getDisplayName(albums[currentAlbum].name);
  albumDisplay += albumName;
  
  if (albumDisplay.length() > 21) {
    albumDisplay = albumDisplay.substring(0, 20) + "~";
  }
  display.println(albumDisplay);
  
  // Line 2: Track name
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
  
  // Line 3: Progress bar and time
  display.setCursor(0, 20);
  
  String trackInfo = String(currentTrack + 1) + "/" + String(trackCount);
  display.print(trackInfo);
  
  display.setCursor(40, 20);
  if (isPaused) {
    display.print("||");
  } else if (isPlaying) {
    display.print("> ");
  } else {
    display.print("[]");
  }
  
  if (isPlaying) {
    unsigned long ms = mp3.positionMillis();
    int secs = ms / 1000;
    int mins = secs / 60;
    secs = secs % 60;
    
    display.setCursor(56, 20);
    if (mins < 10) display.print("0");
    display.print(mins);
    display.print(":");
    if (secs < 10) display.print("0");
    display.print(secs);
  }
  
  // Progress animation
  display.drawRect(88, 20, 40, 8, SSD1306_WHITE);
  if (isPlaying && !isPaused) {
    int progress = (millis() / 100) % 36;
    display.fillRect(90, 22, progress, 4, SSD1306_WHITE);
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
// Headphone Amplifier
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
  
  // Volume up 4 steps (+12dB)
  digitalWrite(HPAMP_VOL_UD, HIGH);
  for (int i = 0; i < 4; i++) {
    digitalWrite(HPAMP_VOL_CLK, LOW);
    delay(10);
    digitalWrite(HPAMP_VOL_CLK, HIGH);
    delay(10);
  }
  digitalWrite(HPAMP_VOL_CLK, LOW);
}