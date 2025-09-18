/*
 * SoundResources.c - Sound Resource Management and Format Conversion
 *
 * Handles Mac OS sound resource formats and provides conversion utilities:
 * - snd resource loading (format 1 and format 2)
 * - Sound header parsing (standard, extended, compressed)
 * - Audio format conversion between different encodings
 * - Sample rate conversion and resampling
 * - MACE compression/decompression
 * - Sound resource creation and manipulation
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../include/SoundManager/SoundManager.h"
#include "../../include/SoundManager/SoundTypes.h"

/* Sound Resource Format Detection */
static SoundHeaderFormat DetectSoundFormat(const uint8_t* data, uint32_t size);
static OSErr ParseSoundListResource(SndListResource* resource, SndCommand** commands, uint16_t* numCommands);
static OSErr ConvertSoundHeaderFormat(SoundHeader* src, ExtSoundHeader* dst);

/* Sound Resource Loading */
OSErr LoadSoundResource(Handle soundHandle, SndListResource** resource)
{
    SndListResource* sndRes;
    uint8_t* data;
    uint32_t size;

    if (soundHandle == NULL || resource == NULL) {
        return paramErr;
    }

    /* This would typically interface with the Resource Manager */
    /* For now, return placeholder implementation */

    sndRes = (SndListResource*)calloc(1, sizeof(SndListResource));
    if (sndRes == NULL) {
        return memFullErr;
    }

    sndRes->format = soundListRsrc;
    sndRes->numModifiers = 0;
    sndRes->numCommands = 1;

    *resource = sndRes;
    return noErr;
}

/* Sound Header Offset Calculation */
void GetSoundHeaderOffset(SndListHandle sndHandle, int32_t* offset)
{
    if (offset != NULL) {
        *offset = sizeof(SndListResource);
    }
}

/* Compression Information */
UnsignedFixed GetCompressionInfo(int16_t compressionID, OSType format,
                                int16_t numChannels, int16_t sampleSize,
                                CompressionInfoPtr cp)
{
    if (cp != NULL) {
        cp->recordSize = sizeof(CompressionInfo);
        cp->format = format;
        cp->compressionID = compressionID;

        switch (compressionID) {
            case kMACE3Compression:
                cp->samplesPerPacket = 6;
                cp->bytesPerPacket = 2;
                break;
            case kMACE6Compression:
                cp->samplesPerPacket = 12;
                cp->bytesPerPacket = 2;
                break;
            default:
                cp->samplesPerPacket = 1;
                cp->bytesPerPacket = (sampleSize * numChannels) / 8;
                break;
        }

        cp->bytesPerFrame = cp->bytesPerPacket;
        cp->bytesPerSample = sampleSize / 8;
    }

    return X2Fixed(1); /* No compression ratio */
}

/* Audio Format Conversion Functions */
void ConvertSampleFormat(void* src, void* dest, uint32_t samples,
                        AudioEncodingType srcFormat, AudioEncodingType destFormat)
{
    uint8_t* srcBytes = (uint8_t*)src;
    uint8_t* destBytes = (uint8_t*)dest;
    uint32_t i;
    int16_t sample16;

    if (srcFormat == destFormat) {
        /* Direct copy */
        uint32_t sampleSize = (srcFormat == k8BitOffsetBinaryFormat) ? 1 : 2;
        memcpy(dest, src, samples * sampleSize);
        return;
    }

    /* Convert between 8-bit and 16-bit formats */
    for (i = 0; i < samples; i++) {
        if (srcFormat == k8BitOffsetBinaryFormat && destFormat == k16BitBigEndianFormat) {
            /* 8-bit unsigned to 16-bit signed */
            sample16 = (srcBytes[i] - 128) << 8;
            destBytes[i * 2] = (sample16 >> 8) & 0xFF;
            destBytes[i * 2 + 1] = sample16 & 0xFF;
        } else if (srcFormat == k16BitBigEndianFormat && destFormat == k8BitOffsetBinaryFormat) {
            /* 16-bit signed to 8-bit unsigned */
            sample16 = (srcBytes[i * 2] << 8) | srcBytes[i * 2 + 1];
            destBytes[i] = (sample16 >> 8) + 128;
        }
    }
}

/* Sample Rate Conversion */
OSErr ConvertSampleRate(int16_t* srcBuffer, uint32_t srcFrames, uint32_t srcRate,
                       int16_t* destBuffer, uint32_t* destFrames, uint32_t destRate)
{
    uint32_t i, srcIndex;
    double ratio, position;

    if (srcBuffer == NULL || destBuffer == NULL || destFrames == NULL) {
        return paramErr;
    }

    if (srcRate == destRate) {
        /* Direct copy */
        uint32_t copyFrames = (*destFrames < srcFrames) ? *destFrames : srcFrames;
        memcpy(destBuffer, srcBuffer, copyFrames * sizeof(int16_t));
        *destFrames = copyFrames;
        return noErr;
    }

    ratio = (double)srcRate / (double)destRate;
    position = 0.0;

    for (i = 0; i < *destFrames && position < srcFrames - 1; i++) {
        srcIndex = (uint32_t)position;

        /* Linear interpolation */
        double fraction = position - srcIndex;
        int16_t sample1 = srcBuffer[srcIndex];
        int16_t sample2 = srcBuffer[srcIndex + 1];

        destBuffer[i] = (int16_t)(sample1 + fraction * (sample2 - sample1));
        position += ratio;
    }

    *destFrames = i;
    return noErr;
}

/* MACE Compression/Decompression Stubs */
OSErr CompressMACE3(int16_t* srcBuffer, uint32_t srcFrames,
                   uint8_t* destBuffer, uint32_t* destBytes)
{
    /* MACE 3:1 compression would be implemented here */
    /* This is a complex algorithm requiring detailed implementation */
    return paramErr; /* Not implemented */
}

OSErr DecompressMACE3(uint8_t* srcBuffer, uint32_t srcBytes,
                     int16_t* destBuffer, uint32_t* destFrames)
{
    /* MACE 3:1 decompression would be implemented here */
    return paramErr; /* Not implemented */
}

/* Channel Conversion */
void ConvertChannels(int16_t* srcBuffer, uint16_t srcChannels,
                    int16_t* destBuffer, uint16_t destChannels,
                    uint32_t frameCount)
{
    uint32_t i;

    if (srcChannels == destChannels) {
        /* Direct copy */
        memcpy(destBuffer, srcBuffer, frameCount * srcChannels * sizeof(int16_t));
        return;
    }

    if (srcChannels == 1 && destChannels == 2) {
        /* Mono to stereo */
        for (i = 0; i < frameCount; i++) {
            destBuffer[i * 2] = srcBuffer[i];
            destBuffer[i * 2 + 1] = srcBuffer[i];
        }
    } else if (srcChannels == 2 && destChannels == 1) {
        /* Stereo to mono */
        for (i = 0; i < frameCount; i++) {
            destBuffer[i] = (srcBuffer[i * 2] + srcBuffer[i * 2 + 1]) / 2;
        }
    }
}

/* Internal Helper Functions */
static SoundHeaderFormat DetectSoundFormat(const uint8_t* data, uint32_t size)
{
    if (size < 2) {
        return standardHeader;
    }

    uint16_t format = (data[0] << 8) | data[1];

    switch (format) {
        case 0x0001: return soundListRsrc;
        case 0x0002: return soundHeaderRsrc;
        case 0x00FF: return extendedHeader;
        case 0x00FE: return compressedHeader;
        default: return standardHeader;
    }
}

static OSErr ParseSoundListResource(SndListResource* resource, SndCommand** commands, uint16_t* numCommands)
{
    if (resource == NULL || commands == NULL || numCommands == NULL) {
        return paramErr;
    }

    *numCommands = resource->numCommands;

    if (*numCommands > 0) {
        *commands = (SndCommand*)calloc(*numCommands, sizeof(SndCommand));
        if (*commands == NULL) {
            return memFullErr;
        }

        /* Parse commands from resource data */
        /* This would require detailed parsing of the resource format */
    }

    return noErr;
}

static OSErr ConvertSoundHeaderFormat(SoundHeader* src, ExtSoundHeader* dst)
{
    if (src == NULL || dst == NULL) {
        return paramErr;
    }

    /* Convert standard header to extended header */
    dst->samplePtr = src->samplePtr;
    dst->numChannels = 1; /* Standard header is mono */
    dst->sampleRate = src->sampleRate;
    dst->loopStart = src->loopStart;
    dst->loopEnd = src->loopEnd;
    dst->encode = src->encode;
    dst->baseFrequency = src->baseFrequency;
    dst->numFrames = src->length;
    dst->sampleSize = (src->encode == k8BitOffsetBinaryFormat) ? 8 : 16;

    /* Clear extended fields */
    memset(dst->AIFFSampleRate, 0, sizeof(dst->AIFFSampleRate));
    dst->markerChunk = 0;
    dst->instrumentChunks = 0;
    dst->AESRecording = 0;
    dst->futureUse1 = 0;
    dst->futureUse2 = 0;
    dst->futureUse3 = 0;
    dst->futureUse4 = 0;

    return noErr;
}