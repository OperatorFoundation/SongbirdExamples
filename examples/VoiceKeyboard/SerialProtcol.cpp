/*
 * SerialProtocol.cpp - Simplified file transfer implementation
 *
 * TX: [SYNC:2][LENGTH:2][opus_file_data...]
 * RX: [SYNC:2][LENGTH:2][opus_file_data...]
 * LOG: [SYNC:2][LENGTH:2][log_string...]
 */

#include "SerialProtocol.h"
#include <stdarg.h>

SerialProtocol::SerialProtocol()
    : serial(nullptr), rxState(RX_WAIT_SYNC1), rxFileLength(0),
      rxBytesReceived(0), rxLengthPos(0),
      rxFileReady(false), rxSequence(0), lastActivityTime(0)
{
    rxFilePath[0] = '\0';
}

bool SerialProtocol::begin(Stream* serialPort)
{
    if (!serialPort) return false;
    serial = serialPort;
    resetRxState();
    lastActivityTime = millis();
    
    // Give a moment for serial to stabilize
    delay(100);
    
    sendLog("SerialProtocol initialized");
    return true;
}

void SerialProtocol::resetRxState()
{
    rxState = RX_WAIT_SYNC1;
    rxFileLength = 0;
    rxBytesReceived = 0;
    rxLengthPos = 0;
    if (rxFile) {
        rxFile.close();
    }
}

bool SerialProtocol::sendFile(const char* filepath)
{
    if (!serial) return false;

    File file = SD.open(filepath, FILE_READ);
    if (!file) {
        sendLogf("Failed to open file: %s", filepath);
        return false;
    }

    uint16_t fileSize = file.size();
    if (fileSize > MAX_FILE_SIZE) {
        sendLogf("File too large: %u", fileSize);
        file.close();
        return false;
    }

    // Send header: sync + length
    uint8_t header[HEADER_SIZE];
    header[0] = SYNC_BYTE_1;
    header[1] = SYNC_BYTE_2;
    header[2] = fileSize & 0xFF;
    header[3] = (fileSize >> 8) & 0xFF;

    serial->write(header, HEADER_SIZE);

    // Send file data in chunks
    uint8_t buffer[256];
    while (file.available()) {
        int bytesRead = file.read(buffer, sizeof(buffer));
        if (bytesRead > 0) {
            serial->write(buffer, bytesRead);
        }
    }

    file.close();
    sendLogf("Sent file: %s (%u bytes)", filepath, fileSize);
    return true;
}

void SerialProtocol::sendLog(const char* message)
{
    if (!serial) return;
    
    size_t len = strlen(message);
    if (len > MAX_LOG_SIZE) len = MAX_LOG_SIZE;
    
    // Header: sync(2) + length(2)
    uint8_t header[HEADER_SIZE];
    header[0] = SYNC_BYTE_1;
    header[1] = SYNC_BYTE_2;
    header[2] = len & 0xFF;
    header[3] = (len >> 8) & 0xFF;
    
    serial->write(header, HEADER_SIZE);
    serial->write((const uint8_t*)message, len);
}

void SerialProtocol::sendLogf(const char* format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    sendLog(buffer);
}

bool SerialProtocol::openRxFile()
{
    snprintf(rxFilePath, sizeof(rxFilePath), "/RX/MSG_%05u.opus", rxSequence);
    
    sendLogf("Creating RX file: %s", rxFilePath);
    
    rxFile = SD.open(rxFilePath, FILE_WRITE);
    if (!rxFile) {
        sendLogf("Failed to create file: %s", rxFilePath);
        return false;
    }
    
    rxSequence++;
    return true;
}

bool SerialProtocol::processIncoming()
{
    if (!serial) return false;

    while (serial->available()) {
        uint8_t byte = serial->read();
        lastActivityTime = millis();

        switch (rxState) {
            case RX_WAIT_SYNC1:
                if (byte == SYNC_BYTE_1) {
                    rxState = RX_WAIT_SYNC2;
                }
                break;

            case RX_WAIT_SYNC2:
                if (byte == SYNC_BYTE_2) {
                    rxState = RX_READ_LENGTH;
                    rxLengthPos = 0;
                } else if (byte == SYNC_BYTE_1) {
                    // Stay in SYNC2 state
                } else {
                    rxState = RX_WAIT_SYNC1;
                }
                break;

            case RX_READ_LENGTH:
                rxLengthBytes[rxLengthPos++] = byte;
                if (rxLengthPos >= 2) {
                    rxFileLength = rxLengthBytes[0] |
                                   (rxLengthBytes[1] << 8);
                    
                    sendLogf("RX length: %u", rxFileLength);
                    
                    if (rxFileLength > MAX_FILE_SIZE) {
                        sendLog("Length too large, resetting");
                        resetRxState();
                    } else if (rxFileLength == 0) {
                        sendLog("Invalid: zero length");
                        resetRxState();
                    } else {
                        rxBytesReceived = 0;
                        if (!openRxFile()) {
                            sendLog("Will discard incoming data");
                            rxState = RX_DISCARD_DATA;
                        } else {
                            rxState = RX_READ_DATA;
                        }
                    }
                }
                break;

            case RX_READ_DATA:
                rxFile.write(byte);
                rxBytesReceived++;
                
                if (rxBytesReceived >= rxFileLength) {
                    // File complete
                    rxFile.close();
                    rxFileReady = true;
                    sendLogf("File complete: %u bytes", rxBytesReceived);
                    resetRxState();
                    return true;
                }
                break;

            case RX_DISCARD_DATA:
                // Discard byte but keep counting
                rxBytesReceived++;
                
                if (rxBytesReceived >= rxFileLength) {
                    sendLogf("Discarded %u bytes", rxBytesReceived);
                    resetRxState();
                }
                break;
        }
    }

    return false;
}
