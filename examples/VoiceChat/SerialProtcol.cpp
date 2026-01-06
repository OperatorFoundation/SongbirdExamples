/*
 * SerialProtocol.cpp - File transfer implementation
 *
 * TX: Simple header without username
 * RX: Header includes username from bridge
 */

#include "SerialProtocol.h"

SerialProtocol::SerialProtocol()
    : serial(nullptr), rxState(RX_WAIT_SYNC1), rxFileLength(0),
      rxBytesReceived(0), rxChannel(0), rxLengthPos(0),
      rxUsernameLen(0), rxUsernamePos(0),
      rxFileReady(false), rxSequence(0), lastActivityTime(0)
{
    rxUsername[0] = '\0';
    rxFilePath[0] = '\0';
}

bool SerialProtocol::begin(Stream* serialPort)
{
    if (!serialPort) return false;
    serial = serialPort;
    resetRxState();
    lastActivityTime = millis();
    return true;
}

void SerialProtocol::resetRxState()
{
    rxState = RX_WAIT_SYNC1;
    rxFileLength = 0;
    rxBytesReceived = 0;
    rxLengthPos = 0;
    rxUsernameLen = 0;
    rxUsernamePos = 0;
    rxUsername[0] = '\0';
    if (rxFile) {
        rxFile.close();
    }
}

bool SerialProtocol::isConnected() const
{
    return (millis() - lastActivityTime) < CONNECTION_TIMEOUT_MS;
}

bool SerialProtocol::sendFile(const char* filepath, uint8_t channel)
{
    if (!serial) return false;

    File file = SD.open(filepath, FILE_READ);
    if (!file) {
        return false;
    }

    uint32_t fileSize = file.size();
    if (fileSize > MAX_FILE_SIZE) {
        file.close();
        return false;
    }

    // Send header: sync + length + channel (no username on TX)
    uint8_t header[TX_HEADER_SIZE];
    header[0] = SYNC_BYTE_1;
    header[1] = SYNC_BYTE_2;
    header[2] = fileSize & 0xFF;
    header[3] = (fileSize >> 8) & 0xFF;
    header[4] = (fileSize >> 16) & 0xFF;
    header[5] = (fileSize >> 24) & 0xFF;
    header[6] = channel;

    serial->write(header, TX_HEADER_SIZE);

    // Send file data in chunks
    uint8_t buffer[256];
    while (file.available()) {
        int bytesRead = file.read(buffer, sizeof(buffer));
        if (bytesRead > 0) {
            serial->write(buffer, bytesRead);
        }
    }

    file.close();
    return true;
}

bool SerialProtocol::openRxFile()
{
    // Generate filename with username if present
    // Format: /RX/CHx/MSG_NNNNN_from_Username.opus
    // Or:     /RX/CHx/MSG_NNNNN.opus (if no username)
    
    if (rxUsernameLen > 0)
    {
        snprintf(rxFilePath, sizeof(rxFilePath), "%s%d/MSG_%05lu_from_%s.opus",
                 RX_DIR_PREFIX, rxChannel + 1, rxSequence, rxUsername);
    }
    else
    {
        snprintf(rxFilePath, sizeof(rxFilePath), "%s%d/MSG_%05lu.opus",
                 RX_DIR_PREFIX, rxChannel + 1, rxSequence);
    }
    
    rxFile = SD.open(rxFilePath, FILE_WRITE);
    if (!rxFile) {
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
                if (rxLengthPos >= 4) {
                    rxFileLength = rxLengthBytes[0] |
                                   (rxLengthBytes[1] << 8) |
                                   (rxLengthBytes[2] << 16) |
                                   (rxLengthBytes[3] << 24);
                    
                    if (rxFileLength == 0 || rxFileLength > MAX_FILE_SIZE) {
                        resetRxState();
                    } else {
                        rxState = RX_READ_CHANNEL;
                    }
                }
                break;

            case RX_READ_CHANNEL:
                rxChannel = byte & 0x0F;
                rxState = RX_READ_USERNAME_LEN;
                break;

            case RX_READ_USERNAME_LEN:
                rxUsernameLen = byte;
                rxUsernamePos = 0;
                rxUsername[0] = '\0';
                
                if (rxUsernameLen > MAX_USERNAME_LEN) {
                    rxUsernameLen = MAX_USERNAME_LEN;
                }
                
                if (rxUsernameLen == 0) {
                    // No username, open file and go to data
                    rxBytesReceived = 0;
                    if (!openRxFile()) {
                        resetRxState();
                    } else {
                        rxState = RX_READ_DATA;
                    }
                } else {
                    rxState = RX_READ_USERNAME;
                }
                break;

            case RX_READ_USERNAME:
                rxUsername[rxUsernamePos++] = (char)byte;
                
                if (rxUsernamePos >= rxUsernameLen) {
                    rxUsername[rxUsernameLen] = '\0';
                    rxBytesReceived = 0;
                    
                    if (!openRxFile()) {
                        resetRxState();
                    } else {
                        rxState = RX_READ_DATA;
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
                    resetRxState();
                    return true;
                }
                break;
        }
    }

    return false;
}