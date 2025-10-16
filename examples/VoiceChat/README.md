# VoiceChat

**A push-to-talk voice chat application for Songbird with end-to-end encryption.**

VoiceChat transforms the Songbird platform into a secure, push-to-talk communication device. Designed for non-realtime voice messaging over various transport layers (Internet, LoRa, etc.), it features end-to-end encryption and optional compression using Codec2 for efficient bandwidth usage.

## Key Features

- **Push-to-talk operation:** Simple one-button recording - press and hold to transmit
- **Queue-based playback:** Non-realtime message delivery with automatic queuing
- **Multi-channel support:** Switch between different chat channels/rooms
- **End-to-end encryption:** Audio encrypted with AES-GCM-128, only endpoints can decrypt
- **Efficient compression:** Optional Codec2 Mode 700C reduces bandwidth

## Hardware Requirements

- Songbird
- microSD card (for message queue storage)
- TRRS headset 
- USB connection to host device for power and network

## Usage

### Basic Operation

**Transmitting:**
1. Press and hold **TOP** button to record
2. Speak your message
4. Release **TOP** button to send
5. Message is encrypted and queued for transmission

**Receiving:**
- Incoming messages play automatically when you're not transmitting
- Queue indicator shows pending messages
- Messages play in order received

### Channel Management

**Switching channels:**
- Press **LEFT** to go to previous channel
- Press **RIGHT** to go to next channel
- Current audio stops, beep plays, new channel audio begins

### Playback Control

**Skip current message:**
- Press **CENTER** button once

**Mute (stop receiving):**
- Hold **CENTER** button for 2 seconds
- Message playback will stop 
- No new messages will be queued until unmuted
- Hold **CENTER** again to unmute

### LED Indicators

**Blue LED:**
- Solid: Connected and idle
- Pulsing: Playing back audio
- Off: No serial connection

**Pink LED:**
- Off: Idle
- Solid: Recording (PTT active)
- Flashing: New message(s) in queue

## Display States

The OLED shows current status:

**Idle/Listening:**
```
Channel: #general
Status: IDLE
Connected: Yes
Queue: 0 msgs
```

**Recording:**
```
Channel: #general
RECORDING [00:03]
████████░░
```

**Playing:**
```
Channel: #general
PLAYING [00:05/00:12]
From: Alice
```

**No Connection:**
```
Channel: #general
NO CONNECTION
Check USB cable
```

## Technical Details

**Audio Format:**
- Input: 44.1kHz, 16-bit mono (Teensy Audio Library native format)
- Compressed: Codec2 Mode 700C (~700 bits/s, 0.52s per packet)
- Encrypted: AES-GCM-128

## Future Enhancements

- LoRa transport layer support
- Updated Codec2 modes for better efficiency
- Group channel support

## License

MIT License - see LICENSE file for details

## Author

Dr. Brandon Wiley, Operator Foundation
