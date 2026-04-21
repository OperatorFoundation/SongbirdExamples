/*
 * PlaybackEngine.cpp - Opus playback implementation
 */

#include "PlaybackEngine.h"

PlaybackEngine::PlaybackEngine()
    : state(PLAYBACK_IDLE),
      fileList(nullptr), fileCount(0), currentFileIndex(0),
      playbackStartTime(0), totalPackets(0), packetsPlayed(0),
      fileHeaderRead(false), outputBufferPos(0), outputBufferCount(0)
{
    previousFile[0] = '\0';
    currentFileForRetention[0] = '\0';
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

bool PlaybackEngine::loadQueue()
{
    cleanupFileList();

    // Open /RX/ directory
    File dir = SD.open("/RX");
    if (!dir)
    {
        DEBUG_PRINTLN("Cannot open /RX/ directory");
        return false;
    }

    // Count .opus files first
    String tempList[MAX_FILES_TO_SCAN];
    uint8_t count = 0;

    while (count < MAX_FILES_TO_SCAN)
    {
        File entry = dir.openNextFile();
        if (!entry) break;

        String name = entry.name();
        entry.close();

        if (name.endsWith(".opus"))
        {
            tempList[count] = "/RX/" + name;
            count++;
        }
    }
    dir.close();

    if (count == 0)
    {
        DEBUG_PRINTLN("No messages in /RX/");
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

    DEBUG_PRINTF("Loaded %d messages\n", count);
    return true;
}

bool PlaybackEngine::startPlayback(AudioPlayQueue* playQueue)
{
    if (!playQueue) return false;
    if (fileCount == 0) return false;

    // Delete file from 2 slots ago (retention policy)
    if (previousFile[0] != '\0')
    {
        SD.remove(previousFile);
        DEBUG_PRINTF("Deleted old file: %s\n", previousFile);
        previousFile[0] = '\0';
    }

    // Shift current to previous
    if (currentFileForRetention[0] != '\0')
    {
        strcpy(previousFile, currentFileForRetention);
    }

    if (!openNextFile())
    {
        return false;
    }

    // Store current file for retention
    strncpy(currentFileForRetention, fileList[currentFileIndex].c_str(), sizeof(currentFileForRetention) - 1);
    currentFileForRetention[sizeof(currentFileForRetention) - 1] = '\0';

    state = PLAYBACK_PLAYING;
    playbackStartTime = millis();

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

    DEBUG_PRINTF("Opened file: %s (size=%lu bytes)\n", 
                 fileList[currentFileIndex].c_str(), currentFile.size());

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

    // Count actual packets in file for accurate duration
    uint32_t filePos = currentFile.position();  // Save position after header
    totalPackets = 0;
    
    while (currentFile.available())
    {
        uint16_t packetSize;
        if (currentFile.read((uint8_t*)&packetSize, 2) != 2)
        {
            DEBUG_PRINTLN("  Failed to read packet size");
            break;
        }
        
        if (packetSize == 0 || packetSize > OPUS_MAX_PACKET_SIZE)
        {
            DEBUG_PRINTF("  Invalid packet size: %u\n", packetSize);
            break;
        }
        
        // Skip packet data
        if (!currentFile.seek(currentFile.position() + packetSize))
        {
            DEBUG_PRINTLN("  Seek failed");
            break;
        }
        
        totalPackets++;
    }
    
    // Return to start of data
    currentFile.seek(filePos);

    DEBUG_PRINTF("File info:\n");
    DEBUG_PRINTF("  Total packets: %lu\n", totalPackets);
    DEBUG_PRINTF("  Duration: %lu ms\n", totalPackets * OPUS_FRAME_MS);

    return true;
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
    (void)playQueue;  // Unused for now
    if (state == PLAYBACK_PAUSED)
    {
        state = PLAYBACK_PLAYING;
        playbackStartTime = millis() - getPlaybackPosition();  // Adjust start time
        DEBUG_PRINTLN("Playback resumed");
        return true;
    }
    return false;
}

bool PlaybackEngine::skipToNext()
{
    DEBUG_PRINTLN("Skipping to next message");

    // Move to next
    currentFileIndex++;

    if (currentFileIndex >= fileCount)
    {
        // No more files
        DEBUG_PRINTLN("No more files in queue");
        stopPlayback();
        return false;
    }

    // Delete file from 2 slots ago (retention policy)
    if (previousFile[0] != '\0')
    {
        SD.remove(previousFile);
        DEBUG_PRINTF("Deleted old file: %s\n", previousFile);
        previousFile[0] = '\0';
    }

    // Shift current to previous
    if (currentFileForRetention[0] != '\0')
    {
        strcpy(previousFile, currentFileForRetention);
    }

    // Open next file
    if (!openNextFile())
    {
        DEBUG_PRINTLN("Failed to open next file");
        stopPlayback();
        return false;
    }

    // Store current file for retention
    strncpy(currentFileForRetention, fileList[currentFileIndex].c_str(), sizeof(currentFileForRetention) - 1);
    currentFileForRetention[sizeof(currentFileForRetention) - 1] = '\0';

    playbackStartTime = millis();
    DEBUG_PRINTF("Now playing: %s\n", getCurrentFileName().c_str());
    return true;
}

void PlaybackEngine::deleteFile(const char* filepath)
{
    SD.remove(filepath);
    DEBUG_PRINTF("Deleted: %s\n", filepath);
}

bool PlaybackEngine::processPlayback(AudioPlayQueue* playQueue)
{
    if (state != PLAYBACK_PLAYING || !playQueue)
    {
        return false;
    }

    // Feed audio to the play queue when it's ready for more data
    int loopCount = 0;
    while (playQueue->available() && loopCount < 10)  // Limit iterations to prevent lockup
    {
        loopCount++;
        
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
            // End of file - try to move to next message
            DEBUG_PRINTLN("File complete, checking for next message");
            
            if (!skipToNext())
            {
                // No more messages
                DEBUG_PRINTLN("No more messages in queue");
                return false;  // Signal that playback is done
            }
            
            // Successfully moved to next file, continue playback
            continue;
        }
    }

    return true;  // Still playing
}

bool PlaybackEngine::decodeAndBuffer()
{
    uint8_t packet[OPUS_MAX_PACKET_SIZE];
    size_t packetSize;

    if (!readPacket(packet, &packetSize))
    {
        DEBUG_PRINTF("readPacket failed, packetsPlayed=%lu, totalPackets=%lu\n", 
                     packetsPlayed, totalPackets);
        return false;  // End of file or error
    }

    // Decode packet - output is upsampled to 44.1kHz
    int samples = codec.decode(packet, packetSize, outputBuffer, RESAMPLE_OUTPUT_SAMPLES);

    if (samples <= 0)
    {
        DEBUG_PRINTF("Decode error: %d\n", samples);
        return false;
    }

    outputBufferCount = samples;
    outputBufferPos = 0;
    packetsPlayed++;

    // Check if we've reached the end
    if (packetsPlayed >= totalPackets)
    {
        DEBUG_PRINTF("Reached end: packetsPlayed=%lu, totalPackets=%lu\n", 
                     packetsPlayed, totalPackets);
    }

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
    if (state == PLAYBACK_PLAYING)
    {
        // Calculate position based on packets played, not wall clock time
        uint32_t position = packetsPlayed * OPUS_FRAME_MS;
        return position;
    }
    else if (state == PLAYBACK_PAUSED)
    {
        // Use packet count when paused
        return packetsPlayed * OPUS_FRAME_MS;
    }
    
    return 0;
}

uint32_t PlaybackEngine::getFileDuration() const
{
    // Estimate from total packets
    return totalPackets * OPUS_FRAME_MS;
}
