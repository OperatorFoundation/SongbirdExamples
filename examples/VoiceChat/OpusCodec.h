/*
 * OpusCodec.h - Opus audio compression for VoiceChat
 *
 * Wraps arduino-libopus for encoding/decoding voice audio.
 * Handles resampling from Teensy's 44.1kHz to Opus's 16kHz.
 */

#ifndef VOICECHAT_OPUSCODEC_H
#define VOICECHAT_OPUSCODEC_H

#include <Arduino.h>
#include <opus.h>

// Opus configuration
#define OPUS_SAMPLE_RATE      16000   // 16kHz for voice (wideband)
#define OPUS_CHANNELS         1       // Mono
#define OPUS_FRAME_MS         20      // 20ms frames (standard)
#define OPUS_FRAME_SAMPLES    (OPUS_SAMPLE_RATE * OPUS_FRAME_MS / 1000)  // 320 samples
#define OPUS_BITRATE          16000   // 16 kbps - good voice quality
#define OPUS_MAX_PACKET_SIZE  256     // Max encoded packet size

// Resampling from 44.1kHz to 16kHz
#define TEENSY_SAMPLE_RATE    44100
#define RESAMPLE_RATIO        (TEENSY_SAMPLE_RATE / OPUS_SAMPLE_RATE)  // ~2.76

// Input buffer size: need enough 44.1kHz samples to produce one 16kHz frame
// 320 samples at 16kHz = 320 * (44100/16000) = 882 samples at 44.1kHz
#define RESAMPLE_INPUT_SAMPLES  ((OPUS_FRAME_SAMPLES * TEENSY_SAMPLE_RATE) / OPUS_SAMPLE_RATE + 1)

// Accumulator buffer for collecting Teensy audio blocks (128 samples each)
// Need ~7 blocks to get 882 samples
#define ACCUMULATOR_SIZE      1024

class OpusCodec
{
public:
    OpusCodec();
    ~OpusCodec();

    // Initialization
    bool begin();
    void end();

    // Encoding (recording path)
    // Feed 44.1kHz samples, get Opus packets out
    // Returns: number of bytes written to outputPacket, or 0 if no packet ready, -1 on error
    int addSamples(const int16_t* samples, size_t count);
    int getEncodedPacket(uint8_t* outputPacket, size_t maxSize);
    bool hasEncodedPacket() const { return packetReady; }
    void resetEncoder();

    // Decoding (playback path)
    // Feed Opus packets, get 44.1kHz samples out
    // Returns: number of samples written to outputSamples, or -1 on error
    int decode(const uint8_t* packet, size_t packetSize, int16_t* outputSamples, size_t maxSamples);

    // Statistics
    uint32_t getEncodedPackets() const { return encodedPacketCount; }
    uint32_t getDecodedPackets() const { return decodedPacketCount; }
    int getLastError() const { return lastError; }

    // Configuration
    void setBitrate(int bps);
    void setComplexity(int complexity);  // 0-10, higher = better quality, more CPU

private:
    OpusEncoder* encoder;
    OpusDecoder* decoder;

    // Accumulator for collecting samples before encoding
    int16_t accumulator[ACCUMULATOR_SIZE];
    size_t accumulatorCount;

    // Resampled buffer (16kHz)
    int16_t resampleBuffer[OPUS_FRAME_SAMPLES];

    // Encoded packet buffer
    uint8_t encodedPacket[OPUS_MAX_PACKET_SIZE];
    size_t encodedPacketSize;
    bool packetReady;

    // Decoded samples buffer (16kHz, before upsampling)
    int16_t decodeBuffer[OPUS_FRAME_SAMPLES];

    // Statistics
    uint32_t encodedPacketCount;
    uint32_t decodedPacketCount;
    int lastError;

    // Resampling state for interpolation
    int16_t lastSample;  // For linear interpolation

    // Internal methods
    void downsample(const int16_t* input, size_t inputCount, int16_t* output, size_t outputCount);
    size_t upsample(const int16_t* input, size_t inputCount, int16_t* output, size_t maxOutput);
    bool encodeFrame();
};

#endif // VOICECHAT_OPUSCODEC_H