# Songbird

A portable audio platform for field recording and audio experimentation, built on the Teensy 4.1 microcontroller.

## What is Songbird?

Songbird combines professional audio processing with a compact, portable design. Use it for field recording, audio production, sound experimentation, or as a foundation for building custom audio applications.

## Key Features

- **High-quality audio** - Professional SGTL5000 codec with microphone and line inputs
- **Portable design** - Battery-ready with USB-C power and built-in display
- **MicroSD storage** - Record directly to standard WAV files
- **Headset support** - Works with TRRS headsets for monitoring and recording
- **Expandable** - Qwiic connector for easy integration with I2C sensors and peripherals
- **Open platform** - Full access to GPIO, USB, and audio connections for custom projects

## What's Included

- Teensy 4.1 microcontroller (600MHz ARM Cortex-M7)
- SGTL5000 audio codec
- 128x32 OLED display
- MicroSD card slot
- Four control buttons
- USB-C connectivity
- Headphone amplifier
- Qwiic I2C connector
- Development breakouts and test points

## Example Applications

This repository includes four example applications:

- **FieldRecorder** - Professional field recording with automatic gain control and wind filtering
- **Recorder** - Phone call and USB audio recording
- **LoopBack** - USB audio interface with real-time monitoring
- **Looper** - Real-time audio looping and layering (in development)

## Getting Started

1. Install [Arduino IDE](https://www.arduino.cc/en/software) and [Teensyduino](https://www.pjrc.com/teensy/td_download.html)
2. Select **Tools > Board > Teensy 4.1**
3. Select **Tools > USB Type > Serial**
4. Load an example sketch and upload

## Hardware Requirements

- Songbird platform
- MicroSD card (FAT32 formatted)
- USB-C cable
- Headset with microphone (CTIA standard)

## Technical Documentation

Detailed technical specifications, pin assignments, and schematics are available in the `/docs` directory (in development).

## License

MIT License

## Developed By

Operator Foundation
