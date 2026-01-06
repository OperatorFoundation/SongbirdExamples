/*
 * SerialProtocol.h - File transfer protocol for VoiceChat
 *
 * TX (Songbird -> Bridge): [SYNC:2][LENGTH:4][CHANNEL:1][opus_file_data...]
 * RX (Bridge -> Songbird): [SYNC:2][LENGTH:4][CHANNEL:1][USERNAME_LEN:1][USERNAME:0-31][opus_file_data...]
 *
 * Control messages (Bridge -> Songbird):
 *   Join:  [SYNC:2][LENGTH:4=0][CHANNEL:0xFF][MSG_TYPE:1=0x01][USERNAME_LEN:1][USERNAME...]
 *   Part:  [SYNC:2][LENGTH:4=0][CHANNEL:0xFF][MSG_TYPE:1=0x02][USERNAME_LEN:1][USERNAME...]
 *
 * The bridge injects the sender's username into incoming messages.
 * Received files are saved as: /RX/CHx/MSG_NNNNN_from_Username.opus
 */

#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include <Arduino.h>
#include <SD.h>
#include "Config.h"

// Protocol constants
#define SYNC_BYTE_1         0xAA
#define SYNC_BYTE_2         0x55
#define TX_HEADER_SIZE      7       // sync(2) + length(4) + channel(1)
#define MAX_FILE_SIZE       65536   // 64KB max file size
#define MAX_USERNAME_LEN    31      // Max username length
#define MAX_USERS           20      // Max tracked users

// Control message types (sent with channel=0xFF, length=0)
#define CONTROL_CHANNEL     0xFF
#define MSG_TYPE_JOIN       0x01
#define MSG_TYPE_PART       0x02
#define MSG_TYPE_PING       0x03

// Log message (sent with channel=0xFE, length=log string length)
#define LOG_CHANNEL         0xFE

class SerialProtocol
{
public:
    SerialProtocol();

    // Initialization
    bool begin(Stream* serialPort);

    // Send a complete Opus file (no username - bridge will add sender info)
    bool sendFile(const char* filepath, uint8_t channel);

    // Send a log message to the bridge for debugging
    void sendLog(const char* message);
    void sendLogf(const char* format, ...);

    // Receive - call in loop
    // Returns true if a complete file is ready
    bool processIncoming();

    // Get received file info (valid after processIncoming returns true)
    bool hasReceivedFile() const { return rxFileReady; }
    uint8_t getReceivedChannel() const { return rxChannel; }
    const char* getReceivedFilePath() const { return rxFilePath; }
    const char* getReceivedUsername() const { return rxUsername; }
    void clearReceivedFile() { rxFileReady = false; }

    // User list management
    uint8_t getUserCount() const { return userCount; }
    const char* getUser(uint8_t index) const;
    bool hasUserListChanged() const { return userListChanged; }
    void clearUserListChanged() { userListChanged = false; }

    // Connection status (based on recent activity)
    bool isConnected() const;

private:
    Stream* serial;

    // RX state machine
    enum RxState { 
        RX_WAIT_SYNC1, 
        RX_WAIT_SYNC2, 
        RX_READ_LENGTH,
        RX_READ_CHANNEL,
        RX_READ_MSG_TYPE,       // For control messages
        RX_READ_USERNAME_LEN,
        RX_READ_USERNAME,
        RX_READ_DATA,
        RX_DISCARD_DATA         // Discard data when file creation fails
    };
    
    RxState rxState;
    uint32_t rxFileLength;
    uint32_t rxBytesReceived;
    uint8_t rxChannel;
    uint8_t rxLengthBytes[4];
    uint8_t rxLengthPos;
    uint8_t rxMsgType;          // For control messages
    uint8_t rxUsernameLen;
    uint8_t rxUsernamePos;
    char rxUsername[MAX_USERNAME_LEN + 1];
    File rxFile;
    char rxFilePath[64];
    bool rxFileReady;
    uint32_t rxSequence;
    
    uint32_t lastActivityTime;

    // User tracking
    char users[MAX_USERS][MAX_USERNAME_LEN + 1];
    uint8_t userCount;
    bool userListChanged;

    // Helper
    void resetRxState();
    bool openRxFile();
    
    // User list helpers
    void addUser(const char* username);
    void removeUser(const char* username);
    int findUser(const char* username) const;
};

#endif // SERIAL_PROTOCOL_H