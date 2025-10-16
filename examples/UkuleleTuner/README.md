# UkuleleTuner

**A chromatic tuner for ukulele built on the Songbird platform.**

UkuleleTuner transforms the Songbird hardware into a precision chromatic tuner specifically designed for ukulele. With visual feedback on the OLED display and intuitive LED indicators, it makes tuning quick and accurate whether you're playing standard GCEA tuning, Low G, or baritone configurations.

## Key Features

- **Chromatic tuning:** Accurately detects pitch across the full ukulele range (and beyond)
- **Multiple tuning presets:** Standard GCEA, Low G, Baritone (DGBE), and custom tunings
- **Real-time visual feedback:** OLED display shows detected note, target note, and tuning offset
- **LED indicators:** 
  - Pink LED brightness indicates tuning accuracy (brighter = more in tune)
  - Blue LED brightness shows input signal level
- **Simple navigation:** Left/Right buttons cycle through strings
- **Professional accuracy:** Uses FFT-based pitch detection for reliable note identification

## Hardware Requirements

- Songbird platform (Teensy 4.1 based)
- TRRS microphone
- Optional: Piezo pickup for acoustic instruments

## Tuning Modes

**Standard GCEA (High G):**
- String 1 (rightmost): A4 (440 Hz)
- String 2: E4 (329.63 Hz)
- String 3: C4 (261.63 Hz)
- String 4 (leftmost): G4 (392.00 Hz)

**Low G:**
- String 4: G3 (196.00 Hz)

**Baritone DGBE:**
- String 1: E3 (164.81 Hz)
- String 2: B2 (123.47 Hz)
- String 3: G2 (98.00 Hz)
- String 4: D2 (73.42 Hz)

## Installation

### Arduino IDE
1. Download this repository as a ZIP file
2. In Arduino IDE: Sketch → Include Library → Add .ZIP Library
3. Select the downloaded ZIP file
4. Ensure Teensy Audio Library is installed

### Manual Installation
1. Clone or download this repository
2. Copy the folder to your Arduino libraries directory
3. Restart Arduino IDE

## Usage

### Basic Operation
1. Connect your microphone or pickup
2. Play a string on your ukulele
3. The OLED displays:
   - Detected note (e.g., "G4")
   - Target note for current string
   - Tuning offset in cents (+/- 50 cents)
   - Visual arrow indicating sharp (↑) or flat (↓)
4. Adjust string tension until the pink LED is brightest and display shows "IN TUNE"

### Button Controls

**LEFT / RIGHT:** Navigate between strings (cycles through string 1-4)

**UP:** Switch tuning mode (Standard GCEA → Low G → Baritone → Chromatic)

**DOWN:** Recalibrate reference pitch (adjusts A4 frequency, default 440 Hz)

### LED Indicators

**Pink LED:**
- OFF: More than 20 cents out of tune
- DIM: 10-20 cents out of tune
- MEDIUM: 5-10 cents out of tune
- BRIGHT: Within ±5 cents (in tune!)
- FULL BRIGHTNESS: Within ±2 cents (perfectly in tune)

**Blue LED:**
- Brightness indicates microphone input level
- Use to ensure adequate signal strength
- Too dim: move closer to microphone or increase picking volume
- Too bright with distortion: reduce input gain

### Tips for Best Results

- **Pluck strings cleanly:** Avoid touching adjacent strings
- **Allow strings to ring:** Give each note time to stabilize
- **Tune in quiet environment:** Background noise can interfere with detection
- **Check intonation:** After tuning, play some chords to verify
- **Stretch new strings:** New strings may need several tuning passes

## Technical Details

- **Sampling rate:** 44.1 kHz, 16-bit
- **FFT size:** 1024 bins (configurable)
- **Frequency resolution:** ~43 Hz per bin
- **Update rate:** ~20 Hz
- **Tuning accuracy:** ±2 cents
- **Detection range:** 65 Hz (C2) to 2 kHz

## Troubleshooting

**"NO SIGNAL" displayed:**
- Check microphone connection
- Increase playing volume
- Ensure microphone is enabled in audio settings

**Erratic readings:**
- Reduce background noise
- Pluck one string at a time
- Check for vibrations affecting the instrument

**Display flickers between notes:**
- String may be between pitches (needs more tuning)
- Try muting other strings
- Ensure clean string contact

MIT License - see LICENSE file for details

## Author

Dr. Brandon Wiley, Operator Foundation
