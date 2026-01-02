Roadtrip MP3 Player
A simple, car-friendly MP3 player for the Songbird V3 hardware platform.
  
Features
	- Album/Playlist Organization - Each folder on the SD card is treated as an album or playlist
	- Simple 4-Button Control - Designed for eyes-on-the-road operation
	- OLED Display - Shows current album, track, playback status, and time
	- Volume Control - Long-press for volume adjustment with visual feedback
	- Auto-Advance - Automatically plays next album when current one finishes
	- Track Navigation - Skip, restart, or go to previous track with intuitive controls
Hardware Requirements
	- Songbird V3 PCB (or compatible hardware)
	- Teensy 4.1 microcontroller
	- SGTL5000 audio codec
	- LM4811 headphone amplifier
	- SSD1306 128x32 OLED display (I2C)
	- 4 navigation buttons
	- 2 status LEDs
	- MicroSD card
Pin Assignments
Function
Pin
LED 1
35
LED 2
31
Button UP
3
Button DOWN
29
Button LEFT
28
Button RIGHT
30
SD Card CS
10
HP Amp Shutdown
45
HP Amp Vol Clock
52
HP Amp Vol Up/Down
5
I2C1 SDA (Display)
17
I2C1 SCL (Display)
16
I2C0 SDA (Codec)
18
I2C0 SCL (Codec)
19
Controls
Button
Short Press
Long Press / Hold
UP
Next Album
Volume Up
DOWN
Play / Pause
Volume Down
LEFT
Restart Track*
- RIGHT
Next Track
- *If less than 3 seconds into the track, LEFT goes to previous track. Double-press LEFT also goes to previous track.
SD Card Setup
Format your SD card as FAT32 and organize your music into folders:
/
├── 01_Road_Trip_Mix/
│   ├── 01_Highway_Star.mp3
│   ├── 02_Born_To_Run.mp3
│   └── 03_Life_Is_A_Highway.mp3
├── 02_Classic_Rock/
│   ├── 01_Stairway_To_Heaven.mp3
│   ├── 02_Free_Bird.mp3
│   └── 03_Hotel_California.mp3
├── 03_Chill_Vibes/
│   ├── 01_Smooth_Operator.mp3
│   └── 02_Sunset_Drive.mp3
└── ...
Naming Tips
	- Prefix folders with numbers (e.g., 01_, 02_) to control album order
	- Prefix tracks with numbers to control track order within albums
	- Use underscores (_) instead of spaces in filenames
	- The display will automatically clean up names (removes numbers, underscores, extensions)
Display Layout
┌──────────────────────┐
│ 1/5 Road Trip Mix    │  <- Album number and name
│ Highway Star         │  <- Track name (scrolls if long)
│ 2/12 > 01:23 [████ ] │  <- Track#, status, time, progress
└──────────────────────┘
Volume Overlay
When adjusting volume, the display temporarily shows:
┌──────────────────────┐
│      VOLUME          │
│        70            │  <- Large volume number
│ [████████████      ] │  <- Volume bar
└──────────────────────┘
LED Indicators
State
LED 1
LED 2
Playing
Pulsing/Breathing
Off
Paused
Off
Solid On
Stopped
Off
Off
Installation
Required Libraries
Install via Arduino Library Manager or manually:
	1	Arduino-Teensy-Codec-lib (Frank Boesing)
	- Download: https://github.com/FrankBoesing/Arduino-Teensy-Codec-lib
	- Install: Sketch → Include Library → Add .ZIP Library
	2	Adafruit SSD1306
	- Install via Library Manager: "Adafruit SSD1306"
	3	Adafruit GFX Library
	- Install via Library Manager: "Adafruit GFX Library"
Arduino IDE Setup
	1	Select Tools → Board → Teensy 4.1
	2	Select Tools → USB Type → Serial (or "Audio" if you want USB audio passthrough)
	3	Upload the sketch
Configuration
Adjust these constants in the code if needed:
#define MAX_ALBUMS       32    // Maximum number of albums
#define MAX_TRACKS       64    // Maximum tracks per album
#define MAX_NAME_LEN     64    // Maximum filename length
#define LONG_PRESS_MS    400   // Hold time for volume mode
#define VOLUME_REPEAT_MS 150   // Volume change repeat rate
#define VOLUME_DISPLAY_MS 1500 // Volume overlay display time
#define RESTART_THRESHOLD_MS 3000 // Time before restart vs prev track
Troubleshooting
No sound
	- Check headphone connection to TRRS jack
	- Verify SD card is inserted and formatted as FAT32
	- Check that MP3 files are valid (try playing on computer first)
	- Ensure volume is not at 0
Display not working
	- Check I2C connections (Wire1: SDA=17, SCL=16)
	- Verify display I2C address is 0x3C
	- Try adjusting display rotation in code if upside down
SD card not detected
	- Use FAT32 format (not exFAT)
	- Try a different SD card
	- Check SPI connections
Tracks not found
	- Ensure files have .mp3 extension (case insensitive)
	- Check that files are directly in album folders (not nested)
	- Avoid special characters in filenames
Codec error on startup
	- Check I2C connections (Wire: SDA=18, SCL=19)
	- Verify SGTL5000 is powered and connected
License
MIT License - Feel free to use and modify for your own projects.
Credits
	- Hardware: Songbird V3 by Operator Foundation
	- MP3 Decoding: Arduino-Teensy-Codec-lib by Frank Boesing
	- Audio Framework: Teensy Audio Library by PJRC

