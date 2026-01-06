/*
 * SerialProtocol.cpp - File transfer implementation
 *
 * TX: Simple header without username
 * RX: Header includes username from bridge
 * Control messages: Join/Part notifications
 */

#include "SerialProtocol.h"
#include <stdarg.h>

SerialProtocol::SerialProtocol()
    : serial(nullptr), rxState(RX_WAIT_SYNC1), rxFileLength(0),
      rxBytesReceived(0), rxChannel(0), rxLengthPos(0),
      rxMsgType(0), rxUsernameLen(0), rxUsernamePos(0),
      rxFileReady(false), rxSequence(0), lastActivityTime(0),
      userCount(0), userListChanged(false)
{
    rxUsername[0] = '\0';
    rxFilePath[0] = '\0';
    
    // Clear user list
    for (uint8_t i = 0; i < MAX_USERS; i++) {
        users[i][0] = '\0';
    }
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
    rxMsgType = 0;
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

const char* SerialProtocol::getUser(uint8_t index) const
{
    if (index >= userCount) return nullptr;
    return users[index];
}

int SerialProtocol::findUser(const char* username) const
{
    for (uint8_t i = 0; i < userCount; i++) {
        if (strcmp(users[i], username) == 0) {
            return i;
        }
    }
    return -1;
}

void SerialProtocol::addUser(const char* username)
{
    if (userCount >= MAX_USERS) return;
    if (findUser(username) >= 0) return; // Already exists
    
    strncpy(users[userCount], username, MAX_USERNAME_LEN);
    users[userCount][MAX_USERNAME_LEN] = '\0';
    userCount++;
    userListChanged = true;
    
    sendLogf("User joined: %s (total: %d)", username, userCount);
}

void SerialProtocol::removeUser(const char* username)
{
    int idx = findUser(username);
    if (idx < 0) return;
    
    // Shift remaining users down
    for (uint8_t i = idx; i < userCount - 1; i++) {
        strcpy(users[i], users[i + 1]);
    }
    userCount--;
    users[userCount][0] = '\0';
    userListChanged = true;
    
    sendLogf("User left: %s (total: %d)", username, userCount);
}

bool SerialProtocol::sendFile(const char* filepath, uint8_t channel)
{
    if (!serial) return false;

    File file = SD.open(filepath, FILE_READ);
    if (!file) {
        sendLogf("Failed to open file: %s", filepath);
        return false;
    }

    uint32_t fileSize = file.size();
    if (fileSize > MAX_FILE_SIZE) {
        sendLogf("File too large: %lu", fileSize);
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
    sendLogf("Sent file: %s (%lu bytes)", filepath, fileSize);
    return true;
}

void SerialProtocol::sendLog(const char* message)
{
    if (!serial) return;
    
    size_t len = strlen(message);
    if (len > 255) len = 255;  // Cap at 255 bytes
    
    // Header: sync(2) + length(4) + channel(1=0xFE)
    uint8_t header[TX_HEADER_SIZE];
    header[0] = SYNC_BYTE_1;
    header[1] = SYNC_BYTE_2;
    header[2] = len & 0xFF;
    header[3] = (len >> 8) & 0xFF;
    header[4] = (len >> 16) & 0xFF;
    header[5] = (len >> 24) & 0xFF;
    header[6] = LOG_CHANNEL;
    
    serial->write(header, TX_HEADER_SIZE);
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
    // Generate filename with username if present
    // rxChannel from protocol is 1-indexed (1-5), matches directory names
    
    if (rxUsernameLen > 0)
    {
        snprintf(rxFilePath, sizeof(rxFilePath), "%s%d/MSG_%05lu_from_%s.opus",
                 RX_DIR_PREFIX, rxChannel, rxSequence, rxUsername);
    }
    else
    {
        snprintf(rxFilePath, sizeof(rxFilePath), "%s%d/MSG_%05lu.opus",
                 RX_DIR_PREFIX, rxChannel, rxSequence);
    }
    
    sendLogf("Creating RX file: %s", rxFilePath);
    sendLogf("  Channel: %d", rxChannel);
    sendLogf("  Username: %s", rxUsernameLen > 0 ? rxUsername : "(none)");
    
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
                if (rxLengthPos >= 4) {
                    rxFileLength = rxLengthBytes[0] |
                                   (rxLengthBytes[1] << 8) |
                                   (rxLengthBytes[2] << 16) |
                                   (rxLengthBytes[3] << 24);
                    
                    sendLogf("RX length: %lu", rxFileLength);
                    
                    if (rxFileLength > MAX_FILE_SIZE) {
                        sendLog("Length too large, resetting");
                        resetRxState();
                    } else {
                        rxState = RX_READ_CHANNEL;
                    }
                }
                break;

            case RX_READ_CHANNEL:
                rxChannel = byte;
                sendLogf("RX channel: 0x%02X", rxChannel);
                
                // Check if this is a control message
                if (rxChannel == CONTROL_CHANNEL && rxFileLength == 0) {
                    sendLog("Control message detected");
                    rxState = RX_READ_MSG_TYPE;
                } else if (rxFileLength == 0) {
                    // Invalid: non-control with zero length
                    sendLog("Invalid: zero length non-control");
                    resetRxState();
                } else {
                    // Audio message - channel is 1-indexed (1-5)
                    rxState = RX_READ_USERNAME_LEN;
                }
                break;

            case RX_READ_MSG_TYPE:
                rxMsgType = byte;
                if (rxMsgType == MSG_TYPE_JOIN || rxMsgType == MSG_TYPE_PART) {
                    rxState = RX_READ_USERNAME_LEN;
                } else if (rxMsgType == MSG_TYPE_PING) {
                    // Ping received - just reset state, lastActivityTime already updated
                    resetRxState();
                } else {
                    // Unknown control message type
                    sendLogf("Unknown msg type: 0x%02X", rxMsgType);
                    resetRxState();
                }
                break;

            case RX_READ_USERNAME_LEN:
                rxUsernameLen = byte;
                rxUsernamePos = 0;
                rxUsername[0] = '\0';
                
                sendLogf("RX username len: %d", rxUsernameLen);
                
                if (rxUsernameLen > MAX_USERNAME_LEN) {
                    rxUsernameLen = MAX_USERNAME_LEN;
                }
                
                if (rxUsernameLen == 0) {
                    // No username
                    if (rxChannel == CONTROL_CHANNEL) {
                        // Control message with no username - invalid
                        sendLog("Control msg with no username");
                        resetRxState();
                    } else {
                        // Audio file with no username
                        rxBytesReceived = 0;
                        if (!openRxFile()) {
                            resetRxState();
                        } else {
                            rxState = RX_READ_DATA;
                        }
                    }
                } else {
                    rxState = RX_READ_USERNAME;
                }
                break;

            case RX_READ_USERNAME:
                rxUsername[rxUsernamePos++] = (char)byte;
                
                if (rxUsernamePos >= rxUsernameLen) {
                    rxUsername[rxUsernameLen] = '\0';
                    sendLogf("RX username: %s", rxUsername);
                    
                    // Handle control messages
                    if (rxChannel == CONTROL_CHANNEL) {
                        if (rxMsgType == MSG_TYPE_JOIN) {
                            addUser(rxUsername);
                        } else if (rxMsgType == MSG_TYPE_PART) {
                            removeUser(rxUsername);
                        }
                        resetRxState();
                        return false; // Not a file, but state changed
                    }
                    
                    // Audio file - try to open, but even if it fails we need to consume the data
                    rxBytesReceived = 0;
                    if (!openRxFile()) {
                        // File creation failed, but we still need to consume the incoming data
                        // to keep the protocol in sync
                        sendLog("Will discard incoming audio data");
                        rxState = RX_DISCARD_DATA;
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
                    sendLogf("File complete: %lu bytes", rxBytesReceived);
                    resetRxState();
                    return true;
                }
                break;

            case RX_DISCARD_DATA:
                // Discard byte but keep counting
                rxBytesReceived++;
                
                if (rxBytesReceived >= rxFileLength) {
                    sendLogf("Discarded %lu bytes", rxBytesReceived);
                    resetRxState();
                }
                break;
        }
    }

    return false;
}