# VoiceChat Playback Flow

## Overview
The playback system receives Opus-compressed audio files from the serial protocol (which includes the sender's username), stores them to SD card, decodes them on-the-fly, and plays them through the headphone output.

## File Reception (SerialProtocol)

### Incoming File Format
```
[SYNC:2][LENGTH:4][CHANNEL:1][USERNAME_LEN:1][USERNAME:0-31][opus_file_data...]
```

- Bridge injects sender username into the header
- Files are saved to `/RX/CH1/`, `/RX/CH2/`, etc. (1-indexed)
- Filename format: `MSG_00001_from_Alice.opus` or `MSG_00001.opus`

### Reception Process
1. `SerialProtocol::processIncoming()` receives complete file with metadata
2. File is written to SD card with username in filename
3. Queue counter for that channel is incremented
4. If idle and on that channel, playback starts automatically

## Playback Initiation

### Trigger Conditions
- New message received while idle on that channel
- User switches to a channel with queued messages
- Previous message finishes and more are queued

### Startup Sequence
```cpp
startPlayback()
├─> player.loadChannelQueue(channel)      // Scan directory for .opus files
├─> player.startPlayback(audioQueue)      // Open first file
│   ├─> openNextFile()
│   │   ├─> Open file and validate OPUS header
│   │   ├─> Extract sender from filename
│   │   └─> Estimate duration from file size
│   └─> Set state to PLAYBACK_PLAYING
├─> currentSender = player.getSenderName()  // "Alice"
├─> Enable headphone amplifier
└─> Display shows "From: Alice"
```

## Continuous Playback Processing

### Main Loop (called from VoiceChat.ino loop)
```cpp
processPlaybackState()
└─> player.processPlayback(audioQueue)
    └─> while (audioQueue->available())
        ├─> If buffered samples exist
        │   ├─> Copy to audio queue buffer
        │   └─> Increment buffer position
        │
        └─> If need more samples
            └─> decodeAndBuffer()
                ├─> readPacket()  // Read packet size + data
                ├─> codec.decode() // Opus → 44.1kHz PCM
                ├─> Store in outputBuffer[882]
                └─> packetsPlayed++
```

### Audio Pipeline
1. **Read packet** from SD card (16-bit size + Opus packet data)
2. **Decode** with Opus codec (16kHz → 44.1kHz upsampling)
3. **Buffer** 882 samples (20ms at 44.1kHz)
4. **Feed** to AudioPlayQueue in 128-sample chunks
5. **Hardware** plays through I2S → SGTL5000 → headphone amp

## Display During Playback

### Information Shown
```
Channel: #2
From: Alice
PLAYING 00:03/00:12
[=========>      ]
```

- Channel number (1-indexed)
- Sender name (extracted from filename)
- Current position / Total duration
- Progress bar

### Duration Tracking
- **Position**: `millis() - playbackStartTime` (real elapsed time)
- **Total**: `totalPackets * 20ms` (estimated from file size)

## Message Completion & Queue Management

### Automatic Progression
```cpp
decodeAndBuffer()
└─> readPacket() returns false (end of file)
    └─> skipToNext()
        ├─> deleteCurrentFile()      // Remove from SD card
        ├─> currentFileIndex++
        ├─> openNextFile()           // Load next in queue
        │   └─> Extract new sender name
        └─> playbackStartTime = millis()  // Reset timer
```

### Manual Skip (DOWN button)
```cpp
Button pressed
└─> player.skipToNext()
    ├─> Delete current file
    ├─> Move to next file
    └─> Update currentSender display
```

### Queue Empty
- `processPlayback()` returns false
- `stopPlayback()` is called
- State returns to IDLE
- Queue count updated

## Pause/Resume Support

### Pause (PTT pressed during playback)
```cpp
startRecording()
├─> if (STATE_PLAYING)
│   ├─> wasPlayingBeforePTT = true
│   └─> player.pausePlayback()
│       └─> state = PLAYBACK_PAUSED
└─> Start recording...
```

### Resume (PTT released)
```cpp
stopRecording()
└─> if (wasPlayingBeforePTT)
    ├─> player.resumePlayback(audioQueue)
    │   ├─> state = PLAYBACK_PLAYING
    │   └─> Adjust playbackStartTime
    └─> currentState = STATE_PLAYING
```

## Key Features

### Username Display
- **Source**: Injected by bridge during reception
- **Storage**: Embedded in filename (`_from_Alice.opus`)
- **Extraction**: Parsed when file is opened
- **Display**: Shown continuously during playback
- **Fallback**: "Unknown" if no username in filename

### File Format
```
[O][P][U][S][0x01][0x00]           ← Header (6 bytes)
[size:16][packet_data...]          ← Packet 1
[size:16][packet_data...]          ← Packet 2
...
```

### Memory Efficiency
- Only current file is open
- Output buffer: 882 samples (1764 bytes)
- Files deleted immediately after playback
- No memory wasted on already-played messages

### Error Handling
- Invalid headers: skip file
- Decode errors: try next packet or skip file
- File I/O errors: log and move to next
- Empty queue: return to idle state

## Integration Points

### With SerialProtocol
- Receives files with username metadata
- Files saved with username in filename
- Queue counters updated per channel

### With AudioSystem
- Uses AudioPlayQueue for output
- Feeds 128-sample blocks continuously
- Enables/disables headphone amp

### With DisplayManager
- Shows channel, sender, time, progress
- Updates every 100ms during playback
- Sender name persists across screens

### With Main Loop
- `processPlayback()` called continuously
- Returns false when queue empty
- Integrates with PTT interruption
