/*
 * SerialProtocol.h - Simplified file transfer protocol for VoiceInterface
 *
 * TX (Songbird -> Host): [SYNC:2][LENGTH:2][opus_file_data...]
 * RX (Host -> Songbird): [SYNC:2][LENGTH:2][opus_file_data...]
 * LOG: [SYNC:2][LENGTH:2][log_string...]
 *
 * Received files are saved as: /RX/MSG_NNNNN.opus
 */

#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include <Arduino.h>
#include <SD.h>
#include "Config.h"

// Protocol constants
#define SYNC_BYTE_1         0xAA
#define SYNC_BYTE_2         0x55
#define HEADER_SIZE         4           // sync(2) + length(2)
#define MAX_FILE_SIZE       65535       // 16-bit length field max
#define MAX_LOG_SIZE        255         // Max log message length

class SerialProtocol
{
public:
    SerialProtocol();

    // Initialization
    bool begin(Stream* serialPort);

    // Send a complete Opus file
    bool sendFile(const char* filepath);

    // Send a log message for debugging
    void sendLog(const char* message);
    void sendLogf(const char* format, ...);

    // Receive - call in loop
    // Returns true if a complete file is ready
    bool processIncoming();

    // Get received file info (valid after processIncoming returns true)
    bool hasReceivedFile() const { return rxFileReady; }
    const char* getReceivedFilePath() const { return rxFilePath; }
    void clearReceivedFile() { rxFileReady = false; }

private:
    Stream* serial;

    // RX state machine
    enum RxState { 
        RX_WAIT_SYNC1, 
        RX_WAIT_SYNC2, 
        RX_READ_LENGTH,
        RX_READ_DATA,
        RX_DISCARD_DATA         // Discard data when file creation fails
    };
    
    RxState rxState;
    uint16_t rxFileLength;
    uint16_t rxBytesReceived;
    uint8_t rxLengthBytes[2];
    uint8_t rxLengthPos;
    File rxFile;
    char rxFilePath[64];
    bool rxFileReady;
    uint16_t rxSequence;
    
    uint32_t lastActivityTime;

    // Helper
    void resetRxState();
    bool openRxFile();
};

#endif // SERIAL_PROTOCOL_H
