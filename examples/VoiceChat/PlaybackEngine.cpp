/*
 * PlaybackEngine.cpp - Opus playback implementation
 */

#include "PlaybackEngine.h"

PlaybackEngine::PlaybackEngine()
    : state(PLAYBACK_IDLE), currentChannel(0),
      fileList(nullptr), fileCount(0), currentFileIndex(0),
      fileStartTime(0), totalPackets(0), packetsPlayed(0),
      fileHeaderRead(false), outputBufferPos(0), outputBufferCount(0)
{
}

bool PlaybackEngine::begin()
{
    // Initialize Opus codec
    if (!codec.begin())
    {
        DEBUG_PRINTLN("PlaybackEngine: Opus init failed");
        return false;
    }

    DEBUG_PRINTLN("PlaybackEngine initialized");
    return true;
}

void PlaybackEngine::cleanupFileList()
{
    if (fileList)
    {
        delete[] fileList;
        fileList = nullptr;
    }
    fileCount = 0;
    currentFileIndex = 0;
}

bool PlaybackEngine::loadChannelQueue(uint8_t channel)
{
    cleanupFileList();
    currentChannel = channel;

    // Build path: /RX/CH1/, /RX/CH2/, etc.
    String dirPath = String(RX_DIR_PREFIX) + String(channel + 1);

    File dir = SD.open(dirPath.c_str());
    if (!dir)
    {
        DEBUG_PRINTF("Cannot open channel dir: %s\n", dirPath.c_str());
        return false;
    }

    // Count .opus files first
    String tempList[MAX_FILES_PER_CHANNEL];
    uint8_t count = 0;

    while (count < MAX_FILES_PER_CHANNEL)
    {
        File entry = dir.openNextFile();
        if (!entry) break;

        String name = entry.name();
        entry.close();

        if (name.endsWith(".opus"))
        {
            tempList[count] = dirPath + "/" + name;
            count++;
        }
    }
    dir.close();

    if (count == 0)
    {
        DEBUG_PRINTF("No messages in channel %d\n", channel + 1);
        return true;  // Not an error, just empty
    }

    // Sort by filename (they're numbered, so alphabetical = chronological)
    for (uint8_t i = 0; i < count - 1; i++)
    {
        for (uint8_t j = i + 1; j < count; j++)
        {
            if (tempList[j] < tempList[i])
            {
                String tmp = tempList[i];
                tempList[i] = tempList[j];
                tempList[j] = tmp;
            }
        }
    }

    // Allocate and copy
    fileList = new String[count];
    if (!fileList)
    {
        DEBUG_PRINTLN("Failed to allocate file list");
        return false;
    }

    for (uint8_t i = 0; i < count; i++)
    {
        fileList[i] = tempList[i];
    }
    fileCount = count;
    currentFileIndex = 0;

    DEBUG_PRINTF("Loaded %d messages for channel %d\n", count, channel + 1);
    return true;
}

bool PlaybackEngine::startPlayback(AudioPlayQueue* playQueue)
{
    if (!playQueue) return false;
    if (fileCount == 0) return false;

    if (!openNextFile())
    {
        return false;
    }

    state = PLAYBACK_PLAYING;
    fileStartTime = millis();

    DEBUG_PRINTF("Starting playback: %s\n", getCurrentFileName().c_str());
    return true;
}

bool PlaybackEngine::openNextFile()
{
    if (currentFileIndex >= fileCount)
    {
        return false;
    }

    // Close any existing file
    if (currentFile)
    {
        currentFile.close();
    }

    // Open next file
    currentFile = SD.open(fileList[currentFileIndex].c_str(), FILE_READ);
    if (!currentFile)
    {
        DEBUG_PRINTF("Failed to open: %s\n", fileList[currentFileIndex].c_str());
        return false;
    }

    // Read and validate header
    uint8_t header[6];
    if (currentFile.read(header, 6) != 6)
    {
        DEBUG_PRINTLN("Failed to read header");
        currentFile.close();
        return false;
    }

    if (header[0] != 'O' || header[1] != 'P' || header[2] != 'U' || header[3] != 'S')
    {
        DEBUG_PRINTLN("Invalid OPUS file header");
        currentFile.close();
        return false;
    }

    fileHeaderRead = true;
    packetsPlayed = 0;
    outputBufferPos = 0;
    outputBufferCount = 0;

    // Extract sender from filename
    currentSender = extractSender(fileList[currentFileIndex]);

    // Estimate total packets from file size
    // Header = 6 bytes, each packet ~40 bytes average + 2 byte size
    uint32_t dataSize = currentFile.size() - 6;
    totalPackets = dataSize / 42;  // Rough estimate

    return true;
}

String PlaybackEngine::extractSender(const String& filename)
{
    // Filename format: /RX/CHx/MSG_00001_from_Alice.opus
    // or just: /RX/CHx/MSG_00001.opus
    int lastSlash = filename.lastIndexOf('/');
    String name = (lastSlash >= 0) ? filename.substring(lastSlash + 1) : filename;

    int fromPos = name.indexOf("_from_");
    if (fromPos > 0)
    {
        int dotPos = name.lastIndexOf('.');
        if (dotPos > fromPos)
        {
            return name.substring(fromPos + 6, dotPos);
        }
    }

    return "Unknown";
}

bool PlaybackEngine::stopPlayback()
{
    if (currentFile)
    {
        currentFile.close();
    }

    state = PLAYBACK_IDLE;
    outputBufferPos = 0;
    outputBufferCount = 0;

    DEBUG_PRINTLN("Playback stopped");
    return true;
}

bool PlaybackEngine::pausePlayback()
{
    if (state == PLAYBACK_PLAYING)
    {
        state = PLAYBACK_PAUSED;
        DEBUG_PRINTLN("Playback paused");
        return true;
    }
    return false;
}

bool PlaybackEngine::resumePlayback(AudioPlayQueue* playQueue)
{
    if (state == PLAYBACK_PAUSED)
    {
        state = PLAYBACK_PLAYING;
        DEBUG_PRINTLN("Playback resumed");
        return true;
    }
    return false;
}

bool PlaybackEngine::skipToNext()
{
    DEBUG_PRINTLN("Skipping to next message");

    // Delete current file after playing
    deleteCurrentFile();

    // Move to next
    currentFileIndex++;

    if (currentFileIndex >= fileCount)
    {
        // No more files
        stopPlayback();
        return false;
    }

    // Open next file
    if (!openNextFile())
    {
        stopPlayback();
        return false;
    }

    fileStartTime = millis();
    return true;
}

void PlaybackEngine::deleteCurrentFile()
{
    if (currentFileIndex < fileCount && fileList)
    {
        String path = fileList[currentFileIndex];
        if (currentFile)
        {
            currentFile.close();
        }
        SD.remove(path.c_str());
        DEBUG_PRINTF("Deleted: %s\n", path.c_str());
    }
}

bool PlaybackEngine::processPlayback(AudioPlayQueue* playQueue)
{
    if (state != PLAYBACK_PLAYING || !playQueue)
    {
        return false;
    }

    // Feed audio to the play queue
    while (playQueue->available())
    {
        // If we have buffered samples, send them
        if (outputBufferPos < outputBufferCount)
        {
            int16_t* dest = playQueue->getBuffer();
            size_t toCopy = min((size_t)AUDIO_BLOCK_SAMPLES,
                               outputBufferCount - outputBufferPos);

            memcpy(dest, &outputBuffer[outputBufferPos], toCopy * sizeof(int16_t));

            // Pad with zeros if needed
            if (toCopy < AUDIO_BLOCK_SAMPLES)
            {
                memset(&dest[toCopy], 0, (AUDIO_BLOCK_SAMPLES - toCopy) * sizeof(int16_t));
            }

            playQueue->playBuffer();
            outputBufferPos += toCopy;
            continue;
        }

        // Need more samples - decode next packet
        if (!decodeAndBuffer())
        {
            // End of file or error
            if (!skipToNext())
            {
                // No more messages
                return false;
            }
        }
    }

    return true;
}

bool PlaybackEngine::decodeAndBuffer()
{
    uint8_t packet[OPUS_MAX_PACKET_SIZE];
    size_t packetSize;

    if (!readPacket(packet, &packetSize))
    {
        return false;  // End of file or error
    }

    // Decode packet to 16kHz, then upsample to 44.1kHz
    int samples = codec.decode(packet, packetSize, outputBuffer, RESAMPLE_OUTPUT_SAMPLES);

    if (samples <= 0)
    {
        DEBUG_PRINTF("Decode error: %d\n", samples);
        return false;
    }

    outputBufferCount = samples;
    outputBufferPos = 0;
    packetsPlayed++;

    return true;
}

bool PlaybackEngine::readPacket(uint8_t* packet, size_t* packetSize)
{
    if (!currentFile || !currentFile.available())
    {
        return false;
    }

    // Read packet size (2 bytes, little-endian)
    uint16_t size;
    if (currentFile.read((uint8_t*)&size, 2) != 2)
    {
        return false;
    }

    if (size == 0 || size > OPUS_MAX_PACKET_SIZE)
    {
        DEBUG_PRINTF("Invalid packet size: %d\n", size);
        return false;
    }

    // Read packet data
    if (currentFile.read(packet, size) != size)
    {
        DEBUG_PRINTLN("Failed to read packet data");
        return false;
    }

    *packetSize = size;
    return true;
}

String PlaybackEngine::getCurrentFileName() const
{
    if (fileCount == 0 || currentFileIndex >= fileCount)
    {
        return "";
    }

    String path = fileList[currentFileIndex];
    int lastSlash = path.lastIndexOf('/');
    return (lastSlash >= 0) ? path.substring(lastSlash + 1) : path;
}

uint32_t PlaybackEngine::getPlaybackPosition() const
{
    // Each packet = 20ms of audio
    return packetsPlayed * OPUS_FRAME_MS;
}

uint32_t PlaybackEngine::getFileDuration() const
{
    // Estimate from total packets
    return totalPackets * OPUS_FRAME_MS;
}