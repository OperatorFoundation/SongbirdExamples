# Songbird Recorder

An audio recorder for the Songbird hardware platform that can record audio from computers, smart phones, and other USB audio devices.

## What it does

This device sits between your audio source and headset, recording both the USB audio input and headset microphone to an SD card while allowing normal audio playback and conversation.

## Built for Songbird Hardware v3

- **Microcontroller**: Teensy 4.1
- **Audio Processing**: SGTL5000 codec
- **Storage**: SD card (FAT32 formatted)
- **Display**: 128x32 OLED
- **Controls**: 4 buttons
- **LEDs**: Blue (recording), Pink (playback)

## Setup

1. Insert SD card
2. Connect headset via 3.5mm TRRS jack (headphone jack is wired using CTIA standard)
3. Connect audio source via USB-C cable

## Uses

- **Computer audio**: Record computer audio output + your microphone input
- **Interviews**: Record remote interviews via video calls
- **Music/Podcasts**: Record audio from any USB audio source with commentary
- **Accessibility**: Create audio records for note-taking or hearing assistance
- **Documentation**: Record important conversations for reference

## Legal and Ethical Considerations

**⚠️ IMPORTANT: Recording audio may be subject to legal restrictions in your jurisdiction.**

### Recording Consent Requirements
- **Two-party consent states**: All parties must consent to recording (California, Florida, etc.)
- **One-party consent states**: Only the recorder needs to consent
- **International calls**: May be subject to stricter international laws
- **Workplace recording**: Often prohibited without employer consent

### User Responsibilities
- **Obtain proper consent** before recording conversations
- **Notify participants** when recording is active
- **Comply with local laws** regarding audio recording
- **Protect recorded data** from unauthorized access
- **Implement retention policies** - don't keep recordings longer than necessary

### Data Security Best Practices When Creating Production Applications
- **Encrypt SD cards** when possible
- **Use strong passwords** for file access
- **Limit access** to recordings on a need-to-know basis
- **Delete recordings** when no longer needed
- **Backup responsibly** - consider encryption for cloud storage

**This software is provided for education and demonstration purposes only. 
The developers of this project are not responsible for misuse of this technology. 
Users must ensure compliance with all applicable laws and ethical standards.**

## Installation

1. Install Arduino IDE and Teensyduino
2. Install required libraries:
   - Audio Library
   - Adafruit GFX Library
   - Adafruit SSD1306 Library
3. Open `SongbirdRecorder.ino` in Arduino IDE
4. Select Board: Teensy 4.1
5. Upload to device

## Usage

### Controls
- **UP**: Start/Stop recording
- **DOWN**: Play/Pause current recording  
- **LEFT**: Previous recording
- **RIGHT**: Next recording

### Display
- Line 1: Current state and timing
- Line 2: Current filename
- Line 3: Button controls

### Recording
1. Press UP to start recording
2. Blue LED shows recording level
3. Press UP again to stop
4. Files saved as `CALL_timestamp.WAV`

### Playback
1. Use LEFT/RIGHT to browse files
2. Press DOWN to play current file
3. Pink LED shows playback status

## Serial Commands

Connect to serial monitor (9600 baud) for file management:

- `LIST` - Show all recordings
- `DELETE filename.wav` - Delete a recording
- `DELETEALL` - Delete all recordings
- `STATUS` - Show system status
- `HELP` - Show available commands

## File Format

Recordings are saved as WAV files:
- 44.1 kHz, 16-bit, mono
- Stored in `/CALLS/` directory on SD card
- Compatible with standard audio software

## Code Structure

- `SongbirdRecorder.ino` - Main application
- `SongbirdRecorder.h` - Constants and definitions
- `audio_manager.ino` - Audio processing
- `sd_manager.ino` - SD card and WAV file operations
- `display_manager.ino` - OLED display
- `button_manager.ino` - Button handling
- `serial_manager.ino` - Serial commands

## License

This project is licensed under the MIT License - see below for details:

```
MIT License

Copyright (c) 2025 Songbird Recorder

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, provided that the following conditions are met:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

**Note**: This is a technology is provided for legitimate purposes such as accessibility, documentation, and content creation. 
Users are solely responsible for ensuring legal compliance and obtaining proper consent when recording conversations. 
Check local laws regarding audio recording before use.
