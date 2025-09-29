# FieldRecorder

**A secure, portable audio recording solution for journalists and reporters.**

FieldRecorder is a Songbird example application that transforms the Songbird platform into a professional field recording device. Designed specifically with reporters in mind, it prioritizes security and simplicity—all recordings are stored exclusively on the microSD card with no cloud storage, persistent device memory, or wireless connectivity.

## Key Features

- **Privacy-focused design:** Audio never leaves the microSD card—no cloud uploads, no device storage, no wireless transmission
- **One-handed operation:** Simple button interface for starting/stopping recordings in the field
- **Professional audio quality:** 44.1kHz, 16-bit WAV files with configurable wind-cut filter (100Hz high-pass)
- **Headset microphone support:** Works with standard TRRS headsets via I2S audio input
- **Organized file management:** Automatic date-based naming (REC_2025-08-21_001.WAV) with sequential numbering
- **Visual feedback:** OLED display shows recording status and audio levels
- **Reliable operation:** Large write buffers and error recovery ensure recordings are never lost

## Hardware Requirements

- Songbird platform
- MicroSD card
- TRRS microphone or headset with microphone

Perfect for investigative journalism, interviews, and any field recording scenario where data security and ease of use are paramount.

## Installation

### Arduino IDE
1. Download this repository as a ZIP file
2. In Arduino IDE: Sketch → Include Library → Add .ZIP Library
3. Select the downloaded ZIP file

### Manual Installation
1. Clone or download this repository
2. Copy the folder to your Arduino libraries directory
3. Restart Arduino IDE

## Usage

### Starting a Recording
1. Press **UP** button
2. Wait for 3-second countdown
3. Recording begins automatically

### Stopping a Recording
Hold **UP** button for 2 seconds while recording

### Adjusting Audio During Recording

**When AGC is enabled:**
- Press **LEFT** to toggle wind-cut filter on/off (100Hz high-pass filter for outdoor recording)

**When using manual gain:**
- Press **LEFT** to decrease microphone gain
- Press **RIGHT** to increase microphone gain

Manual gain adjustments disable AGC automatically and persist between recordings.

### Enabling AGC
When idle, hold **LEFT** + **RIGHT** together for 2 seconds to re-enable automatic gain control

### Accessing Recordings
All recordings are saved as standard WAV files in the `/RECORDINGS/` directory on the microSD card. These files can be transferred to a computer and played using any standard audio software, providing immediate access to your recordings while on-device playback features continue development.

### Playback (Experimental)
On-device playback is under active development as the WAVMaker library and playback functionality are refined.

**When idle:**
- Press **LEFT** or **RIGHT** to browse recordings
- Press **DOWN** to play selected recording

**During playback:**
- Press **LEFT** or **RIGHT** to adjust headphone volume
- Press **DOWN** to skip to next recording
- Press **UP** to stop

## License

MIT License - see LICENSE file for details

## Author

Operator Foundation - 2025
