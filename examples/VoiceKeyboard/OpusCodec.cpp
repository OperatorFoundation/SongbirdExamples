/*
 * OpusCodec.cpp - Opus audio compression implementation
 */

#include "OpusCodec.h"
#include "Config.h"

OpusCodec::OpusCodec()
    : encoder(nullptr), decoder(nullptr), accumulatorCount(0),
      encodedPacketSize(0), packetReady(false),
      encodedPacketCount(0), decodedPacketCount(0),
      lastError(0), lastSample(0)
{
}

OpusCodec::~OpusCodec()
{
    end();
}

bool OpusCodec::begin()
{
    int err;

    // Create encoder
    encoder = opus_encoder_create(OPUS_SAMPLE_RATE, OPUS_CHANNELS, OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK || !encoder)
    {
        DEBUG_PRINTF("Opus encoder creation failed: %d\n", err);
        lastError = err;
        return false;
    }

    // Configure encoder for voice
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(OPUS_BITRATE));
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(5));  // Balance quality/CPU
    opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(encoder, OPUS_SET_VBR(1));  // Variable bitrate
    opus_encoder_ctl(encoder, OPUS_SET_DTX(1));  // Discontinuous transmission for silence

    // Create decoder
    decoder = opus_decoder_create(OPUS_SAMPLE_RATE, OPUS_CHANNELS, &err);
    if (err != OPUS_OK || !decoder)
    {
        DEBUG_PRINTF("Opus decoder creation failed: %d\n", err);
        lastError = err;
        opus_encoder_destroy(encoder);
        encoder = nullptr;
        return false;
    }

    DEBUG_PRINTLN("Opus codec initialized");
    DEBUG_PRINTF("  Sample rate: %d Hz\n", OPUS_SAMPLE_RATE);
    DEBUG_PRINTF("  Bitrate: %d bps\n", OPUS_BITRATE);
    DEBUG_PRINTF("  Frame size: %d ms (%d samples)\n", OPUS_FRAME_MS, OPUS_FRAME_SAMPLES);

    return true;
}

void OpusCodec::end()
{
    if (encoder)
    {
        opus_encoder_destroy(encoder);
        encoder = nullptr;
    }
    if (decoder)
    {
        opus_decoder_destroy(decoder);
        decoder = nullptr;
    }
}

int OpusCodec::addSamples(const int16_t* samples, size_t count)
{
    if (!encoder) return -1;

    // Add samples to accumulator
    size_t spaceAvailable = ACCUMULATOR_SIZE - accumulatorCount;
    size_t toCopy = min(count, spaceAvailable);

    memcpy(&accumulator[accumulatorCount], samples, toCopy * sizeof(int16_t));
    accumulatorCount += toCopy;

    // Check if we have enough samples for a frame
    // Need RESAMPLE_INPUT_SAMPLES (882) samples at 44.1kHz for one 16kHz frame
    int packetsEncoded = 0;
    while (accumulatorCount >= RESAMPLE_INPUT_SAMPLES)
    {
        // Downsample to 16kHz
        downsample(accumulator, RESAMPLE_INPUT_SAMPLES, resampleBuffer, OPUS_FRAME_SAMPLES);

        // Encode the frame
        if (encodeFrame())
        {
            packetsEncoded++;
        }

        // Shift remaining samples
        size_t remaining = accumulatorCount - RESAMPLE_INPUT_SAMPLES;
        if (remaining > 0)
        {
            memmove(accumulator, &accumulator[RESAMPLE_INPUT_SAMPLES], remaining * sizeof(int16_t));
        }
        accumulatorCount = remaining;
    }

    return packetsEncoded;
}

bool OpusCodec::encodeFrame()
{
    int bytes = opus_encode(encoder, resampleBuffer, OPUS_FRAME_SAMPLES,
                           encodedPacket, OPUS_MAX_PACKET_SIZE);
    if (bytes < 0)
    {
        DEBUG_PRINTF("Opus encode error: %d\n", bytes);
        lastError = bytes;
        return false;
    }

    encodedPacketSize = bytes;
    packetReady = true;
    encodedPacketCount++;

    return true;
}

int OpusCodec::getEncodedPacket(uint8_t* outputPacket, size_t maxSize)
{
    if (!packetReady || encodedPacketSize == 0)
    {
        return 0;
    }

    if (maxSize < encodedPacketSize)
    {
        return -1;  // Buffer too small
    }

    memcpy(outputPacket, encodedPacket, encodedPacketSize);
    int size = encodedPacketSize;

    packetReady = false;
    encodedPacketSize = 0;

    return size;
}

void OpusCodec::resetEncoder()
{
    if (encoder)
    {
        opus_encoder_ctl(encoder, OPUS_RESET_STATE);
    }
    accumulatorCount = 0;
    packetReady = false;
    encodedPacketSize = 0;
    lastSample = 0;
}

int OpusCodec::decode(const uint8_t* packet, size_t packetSize,
                      int16_t* outputSamples, size_t maxSamples)
{
    if (!decoder) return -1;

    // Decode to 16kHz
    int samples = opus_decode(decoder, packet, packetSize,
                              decodeBuffer, OPUS_FRAME_SAMPLES, 0);
    if (samples < 0)
    {
        DEBUG_PRINTF("Opus decode error: %d\n", samples);
        lastError = samples;
        return -1;
    }

    decodedPacketCount++;

    // Upsample to 44.1kHz for Teensy playback
    size_t outputCount = upsample(decodeBuffer, samples, outputSamples, maxSamples);

    return outputCount;
}

void OpusCodec::downsample(const int16_t* input, size_t inputCount,
                           int16_t* output, size_t outputCount)
{
    // Simple linear interpolation downsampling from 44.1kHz to 16kHz
    // Ratio: 44100/16000 = 2.75625

    const float ratio = (float)TEENSY_SAMPLE_RATE / (float)OPUS_SAMPLE_RATE;

    for (size_t i = 0; i < outputCount; i++)
    {
        float srcPos = i * ratio;
        size_t srcIdx = (size_t)srcPos;
        float frac = srcPos - srcIdx;

        if (srcIdx + 1 < inputCount)
        {
            // Linear interpolation
            output[i] = (int16_t)(input[srcIdx] * (1.0f - frac) + input[srcIdx + 1] * frac);
        }
        else if (srcIdx < inputCount)
        {
            output[i] = input[srcIdx];
        }
        else
        {
            output[i] = 0;
        }
    }
}

size_t OpusCodec::upsample(const int16_t* input, size_t inputCount,
                           int16_t* output, size_t maxOutput)
{
    // Upsample from 16kHz to 44.1kHz
    // Ratio: 44100/16000 = 2.75625
    // 320 input samples -> 882 output samples

    const float ratio = (float)OPUS_SAMPLE_RATE / (float)TEENSY_SAMPLE_RATE;
    size_t outputCount = (size_t)(inputCount / ratio);

    if (outputCount > maxOutput)
    {
        outputCount = maxOutput;
    }

    for (size_t i = 0; i < outputCount; i++)
    {
        float srcPos = i * ratio;
        size_t srcIdx = (size_t)srcPos;
        float frac = srcPos - srcIdx;

        if (srcIdx + 1 < inputCount)
        {
            output[i] = (int16_t)(input[srcIdx] * (1.0f - frac) + input[srcIdx + 1] * frac);
        }
        else if (srcIdx < inputCount)
        {
            output[i] = input[srcIdx];
        }
        else
        {
            output[i] = lastSample;
        }
    }

    if (inputCount > 0)
    {
        lastSample = input[inputCount - 1];
    }

    return outputCount;
}

void OpusCodec::setBitrate(int bps)
{
    if (encoder)
    {
        opus_encoder_ctl(encoder, OPUS_SET_BITRATE(bps));
        DEBUG_PRINTF("Opus bitrate set to %d bps\n", bps);
    }
}

void OpusCodec::setComplexity(int complexity)
{
    if (encoder)
    {
        complexity = constrain(complexity, 0, 10);
        opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(complexity));
        DEBUG_PRINTF("Opus complexity set to %d\n", complexity);
    }
}