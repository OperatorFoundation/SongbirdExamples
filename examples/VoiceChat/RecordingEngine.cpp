/*
 * RecordingEngine.cpp - Opus-compressed recording implementation
 */

#include "RecordingEngine.h"

RecordingEngine::RecordingEngine()
    : recording(false), sdCardPresent(false), lastError(ERROR_NONE),
      recordingStartTime(0), bytesWritten(0), packetCount(0),
      nextSequenceNumber(1)
{
}

bool RecordingEngine::begin()
{
    // Initialize SD card
    if (!checkSDCard())
    {
        lastError = ERROR_NO_SD_CARD;
        return false;
    }

    // Create directory structure
    if (!createDirectories())
    {
        DEBUG_PRINTLN("Warning: Could not create directories");
    }

    // Initialize Opus codec
    if (!codec.begin())
    {
        DEBUG_PRINTLN("Failed to initialize Opus codec");
        return false;
    }

    // Scan for highest sequence number in TX directory
    File txDir = SD.open(TX_DIR);
    if (txDir)
    {
        uint32_t highest = 0;
        while (true)
        {
            File entry = txDir.openNextFile();
            if (!entry) break;

            String name = entry.name();
            entry.close();

            // Parse MSG_NNNNN.opus
            if (name.startsWith("MSG_") && name.endsWith(".opus"))
            {
                uint32_t num = name.substring(4, 9).toInt();
                if (num > highest) highest = num;
            }
        }
        txDir.close();
        nextSequenceNumber = highest + 1;
    }

    DEBUG_PRINTF("RecordingEngine initialized, next seq: %d\n", nextSequenceNumber);
    return true;
}

bool RecordingEngine::checkSDCard()
{
    if (digitalRead(SDCARD_DETECT_PIN) == HIGH)
    {
        sdCardPresent = false;
        return false;
    }

    if (!SD.begin(SDCARD_CS_PIN))
    {
        sdCardPresent = false;
        return false;
    }

    sdCardPresent = true;
    return true;
}

bool RecordingEngine::createDirectories()
{
    if (!sdCardPresent) return false;

    // Create TX directory
    if (!SD.exists(TX_DIR))
    {
        if (!SD.mkdir(TX_DIR))
        {
            DEBUG_PRINTLN("Failed to create TX directory");
            return false;
        }
    }

    // Create RX channel directories
    for (uint8_t i = 0; i < NUM_CHANNELS; i++)
    {
        String dirPath = String(RX_DIR_PREFIX) + String(i + 1);
        if (!SD.exists(dirPath.c_str()))
        {
            SD.mkdir(dirPath.c_str());
        }
    }

    return true;
}

String RecordingEngine::generateFilename(uint8_t channel)
{
    // Format: /TX/MSG_NNNNN_CHx.opus
    char filename[32];
    snprintf(filename, sizeof(filename), "%s/MSG_%05lu_CH%d.opus",
             TX_DIR, nextSequenceNumber, channel + 1);
    return String(filename);
}

bool RecordingEngine::startRecording(uint8_t channel)
{
    if (recording)
    {
        DEBUG_PRINTLN("Already recording");
        return false;
    }

    if (!checkSDCard())
    {
        lastError = ERROR_NO_SD_CARD;
        return false;
    }

    // Generate filename
    currentFileName = generateFilename(channel);
    DEBUG_PRINTF("Starting recording: %s\n", currentFileName.c_str());

    // Open file
    currentFile = SD.open(currentFileName.c_str(), FILE_WRITE);
    if (!currentFile)
    {
        DEBUG_PRINTLN("Failed to create file");
        lastError = ERROR_FILE_CREATE_FAILED;
        return false;
    }

    // Write file header (simple magic + version)
    const uint8_t header[] = {'O', 'P', 'U', 'S', 0x01, 0x00};  // "OPUS" + version 1.0
    currentFile.write(header, sizeof(header));
    bytesWritten = sizeof(header);

    // Reset codec state
    codec.resetEncoder();

    // Reset counters
    recordingStartTime = millis();
    packetCount = 0;
    recording = true;

    return true;
}

bool RecordingEngine::processRecording(AudioRecordQueue* queue)
{
    if (!recording || !queue) return false;

    bool dataProcessed = false;

    while (queue->available() > 0)
    {
        int16_t* buffer = queue->readBuffer();

        // Feed samples to Opus encoder
        int result = codec.addSamples(buffer, AUDIO_BLOCK_SAMPLES);

        if (result < 0)
        {
            DEBUG_PRINTLN("Opus encoding error");
            lastError = ERROR_WRITE_FAILED;
            queue->freeBuffer();
            stopRecording();
            return false;
        }

        // Write any encoded packets
        while (codec.hasEncodedPacket())
        {
            uint8_t packet[OPUS_MAX_PACKET_SIZE];
            int packetSize = codec.getEncodedPacket(packet, sizeof(packet));

            if (packetSize > 0)
            {
                if (!writePacket(packet, packetSize))
                {
                    lastError = ERROR_WRITE_FAILED;
                    queue->freeBuffer();
                    stopRecording();
                    return false;
                }
                dataProcessed = true;
            }
        }

        queue->freeBuffer();
    }

    return dataProcessed;
}

bool RecordingEngine::writePacket(const uint8_t* packet, size_t size)
{
    if (!currentFile) return false;

    // Write packet size (2 bytes, little-endian)
    uint16_t packetSize = (uint16_t)size;
    if (currentFile.write((uint8_t*)&packetSize, 2) != 2)
    {
        DEBUG_PRINTLN("Failed to write packet size");
        return false;
    }

    // Write packet data
    if (currentFile.write(packet, size) != size)
    {
        DEBUG_PRINTLN("Failed to write packet data");
        return false;
    }

    bytesWritten += 2 + size;
    packetCount++;

    // Flush periodically
    if (packetCount % 50 == 0)
    {
        currentFile.flush();
    }

    return true;
}

bool RecordingEngine::stopRecording()
{
    if (!recording) return false;

    recording = false;

    // Close file
    if (currentFile)
    {
        currentFile.flush();
        currentFile.close();
    }

    nextSequenceNumber++;

    DEBUG_PRINTF("Recording saved: %s\n", currentFileName.c_str());
    DEBUG_PRINTF("  Duration: %lu ms\n", getRecordingDuration());
    DEBUG_PRINTF("  Size: %lu bytes\n", bytesWritten);
    DEBUG_PRINTF("  Packets: %lu\n", packetCount);

    return true;
}

uint32_t RecordingEngine::getRecordingDuration() const
{
    if (recording)
    {
        return millis() - recordingStartTime;
    }
    // Estimate from packet count (each packet = 20ms)
    return packetCount * OPUS_FRAME_MS;
}

uint32_t RecordingEngine::getRecordingSize() const
{
    return bytesWritten;
}