# Speech Manager for System 7.1 Portable

This directory contains a complete, portable implementation of the Macintosh Speech Manager API from System 7.1, enhanced with modern speech synthesis capabilities and cross-platform support.

## Overview

The Speech Manager provides text-to-speech synthesis functionality for accessibility and system feedback. This implementation maintains full compatibility with the original Mac OS Speech Manager API while adding modern features like neural text-to-speech, cloud services integration, and advanced audio processing.

## Architecture

### Core Components

1. **SpeechManagerCore.c** - Main API entry points and system integration
2. **VoiceManager.c** - Voice enumeration, selection, and management
3. **TextToSpeech.c** - Text processing, analysis, and conversion
4. **SpeechSynthesis.c** - Speech synthesis engine integration
5. **VoiceResources.c** - Voice resource loading and caching
6. **SpeechChannels.c** - Speech channel management and control
7. **PronunciationEngine.c** - Phoneme processing and pronunciation
8. **SpeechOutput.c** - Audio output device control and routing

### Key Features

- **Complete API Compatibility**: Full implementation of the original Mac OS Speech Manager API
- **Modern Synthesis Engines**: Support for SAPI, eSpeak, Festival, and neural TTS engines
- **Cross-Platform Audio**: Portable audio output supporting multiple platforms
- **Advanced Text Processing**: Sophisticated text analysis, normalization, and phoneme conversion
- **Voice Management**: Comprehensive voice resource management with caching
- **Real-Time Processing**: Low-latency speech synthesis for interactive applications
- **Accessibility Support**: Enhanced features for screen readers and assistive technology
- **Multi-Language Support**: International language and voice support
- **Cloud Integration**: Support for cloud-based speech services

## API Reference

### Core Speech Manager Functions

```c
// System initialization
uint32_t SpeechManagerVersion(void);
OSErr InitializeSpeechManager(void);
void CleanupSpeechManager(void);

// Voice management
OSErr CountVoices(short *numVoices);
OSErr GetIndVoice(short index, VoiceSpec *voice);
OSErr GetVoiceDescription(VoiceSpec *voice, VoiceDescription *info, long infoLength);

// Channel management
OSErr NewSpeechChannel(VoiceSpec *voice, SpeechChannel *chan);
OSErr DisposeSpeechChannel(SpeechChannel chan);

// Speech synthesis
OSErr SpeakString(StringPtr textString);
OSErr SpeakText(SpeechChannel chan, void *textBuf, long textBytes);
OSErr SpeakBuffer(SpeechChannel chan, void *textBuf, long textBytes, long controlFlags);

// Speech control
OSErr StopSpeech(SpeechChannel chan);
OSErr PauseSpeechAt(SpeechChannel chan, long whereToPause);
OSErr ContinueSpeech(SpeechChannel chan);

// Parameter control
OSErr SetSpeechRate(SpeechChannel chan, Fixed rate);
OSErr GetSpeechRate(SpeechChannel chan, Fixed *rate);
OSErr SetSpeechPitch(SpeechChannel chan, Fixed pitch);
OSErr GetSpeechPitch(SpeechChannel chan, Fixed *pitch);
```

### Quick Start Functions

```c
// Simple usage
OSErr SpeechQuickStart(void);
OSErr SpeechQuickSpeak(const char *text);
OSErr SpeechQuickSpeakWithVoice(const char *text, const char *voiceName);
void SpeechQuickCleanup(void);

// Voice selection helpers
OSErr SpeechSelectBestVoice(const char *language, VoiceGender preferredGender, VoiceSpec *voice);
OSErr SpeechGetVoiceNames(char ***voiceNames, short *voiceCount);
```

## Usage Examples

### Basic Text-to-Speech

```c
#include "SpeechManager/SpeechManager.h"

int main() {
    // Quick start
    OSErr err = SpeechQuickStart();
    if (err != noErr) {
        printf("Failed to initialize Speech Manager: %d\n", err);
        return 1;
    }

    // Speak some text
    err = SpeechQuickSpeak("Hello, world! This is the Speech Manager.");
    if (err != noErr) {
        printf("Failed to speak text: %d\n", err);
    }

    // Clean up
    SpeechQuickCleanup();
    return 0;
}
```

### Advanced Channel Management

```c
#include "SpeechManager/SpeechManager.h"

void AdvancedSpeechExample() {
    VoiceSpec voice;
    SpeechChannel channel;
    OSErr err;

    // Get the first available voice
    err = GetIndVoice(1, &voice);
    if (err != noErr) return;

    // Create a speech channel
    err = NewSpeechChannel(&voice, &channel);
    if (err != noErr) return;

    // Set speech parameters
    SetSpeechRate(channel, 0x00018000);  // 1.5x normal rate
    SetSpeechPitch(channel, 0x00012000); // 1.125x normal pitch

    // Speak text
    const char *text = "This is spoken at a faster rate with higher pitch.";
    err = SpeakText(channel, (void *)text, strlen(text));

    // Wait for completion (in real code, use callbacks)
    while (SpeechBusy()) {
        usleep(100000); // 100ms
    }

    // Clean up
    DisposeSpeechChannel(channel);
}
```

### Voice Selection and Management

```c
#include "SpeechManager/VoiceManager.h"

void EnumerateVoices() {
    short voiceCount;
    OSErr err = CountVoices(&voiceCount);

    if (err == noErr) {
        printf("Found %d voices:\n", voiceCount);

        for (short i = 1; i <= voiceCount; i++) {
            VoiceSpec voice;
            VoiceDescription desc;

            err = GetIndVoice(i, &voice);
            if (err == noErr) {
                desc.length = sizeof(VoiceDescription);
                err = GetVoiceDescription(&voice, &desc, sizeof(desc));

                if (err == noErr) {
                    // Convert Pascal string to C string
                    char voiceName[64];
                    memcpy(voiceName, &desc.name[1], desc.name[0]);
                    voiceName[desc.name[0]] = '\0';

                    printf("  %d: %s (Gender: %d, Age: %d)\n",
                           i, voiceName, desc.gender, desc.age);
                }
            }
        }
    }
}
```

### Text Processing and Phonemes

```c
#include "SpeechManager/TextToSpeech.h"
#include "SpeechManager/PronunciationEngine.h"

void TextProcessingExample() {
    const char *text = "Hello, Dr. Smith! How are you today?";
    char *processedText = NULL;
    char *phonemes = NULL;

    // Process text (normalize, expand abbreviations, etc.)
    OSErr err = SpeechProcessText(text, &processedText);
    if (err == noErr && processedText) {
        printf("Processed text: %s\n", processedText);

        // Convert to phonemes
        err = SpeechConvertToPhonemes(processedText, &phonemes);
        if (err == noErr && phonemes) {
            printf("Phonemes: %s\n", phonemes);
            free(phonemes);
        }

        free(processedText);
    }
}
```

### Modern Features Integration

```c
#include "SpeechManager/SpeechManagerIntegration.h"

void ModernFeaturesExample() {
    // Configure for high-quality synthesis
    SpeechManagerConfiguration config = kSpeechConfig_HighQuality;
    ApplySpeechManagerConfiguration(&config);

    // Enable neural TTS if available
    ConfigureNeuralTTSEngine("/path/to/neural/models");

    // Set up accessibility support
    SetupAccessibilitySupport(true, true);

    // Use SSML for advanced speech control
    const char *ssmlText = "<speak>"
                           "<prosody rate=\"slow\" pitch=\"+10Hz\">"
                           "This text is spoken slowly with higher pitch."
                           "</prosody>"
                           "</speak>";

    SpeechChannel chan;
    VoiceSpec voice;
    GetIndVoice(1, &voice);
    NewSpeechChannel(&voice, &chan);

    SpeechSynthesizeSSML(ssmlText, chan);

    DisposeSpeechChannel(chan);
}
```

## Platform Integration

### Windows (SAPI)

```c
#ifdef _WIN32
    // Initialize Windows speech platform
    InitializeSpeechManagerWindows();
    ConfigureSAPIIntegration(true);
    ConfigureWindowsSpeechPlatform();
#endif
```

### macOS (AVSpeechSynthesizer)

```c
#ifdef __APPLE__
    // Initialize macOS speech platform
    InitializeSpeechManagerMacOS();
    ConfigureAVSpeechIntegration(true);
    ConfigureAccessibilityIntegration(true);
#endif
```

### Linux (eSpeak/Festival)

```c
#ifdef __linux__
    // Initialize Linux speech platform
    InitializeSpeechManagerLinux();
    ConfigureESpeakIntegration(true);
    ConfigureFestivalIntegration(true);
    ConfigureSpeechDispatcherIntegration(true);
#endif
```

## Building and Integration

### Build Requirements

- C99-compatible compiler
- pthread support
- zlib (for compression)
- Platform-specific audio libraries:
  - Windows: DirectSound or WASAPI
  - macOS: Core Audio
  - Linux: ALSA, PulseAudio, or JACK

### Compilation Flags

```bash
# Basic compilation
gcc -std=c99 -pthread -lz -o speech_example \
    SpeechManagerCore.c VoiceManager.c TextToSpeech.c \
    SpeechSynthesis.c VoiceResources.c SpeechChannels.c \
    PronunciationEngine.c SpeechOutput.c example.c

# With platform-specific libraries
# Linux
gcc -std=c99 -pthread -lz -lasound -lpulse -o speech_example ...

# macOS
gcc -std=c99 -pthread -lz -framework CoreAudio -framework AudioToolbox -o speech_example ...
```

### CMake Integration

```cmake
# Add Speech Manager to your CMake project
add_subdirectory(System7.1-Portable/src/SpeechManager)
target_link_libraries(your_target SpeechManager)
```

## Error Handling

The Speech Manager uses standard Mac OS error codes:

- `noErr` (0) - Success
- `paramErr` (-50) - Parameter error
- `memFullErr` (-108) - Out of memory
- `resNotFound` (-192) - Resource not found
- `voiceNotFound` (-244) - Voice not found
- `noSynthFound` (-245) - No synthesis engine found
- `synthOpenFailed` (-246) - Failed to open synthesis engine
- `synthNotReady` (-247) - Synthesis engine not ready
- `bufTooSmall` (-248) - Buffer too small
- `badInputText` (-249) - Invalid input text

## Performance Considerations

- Voice resources are cached in memory for fast access
- Text processing is optimized for real-time use
- Audio buffers are sized for low-latency playback
- Multiple speech channels can run concurrently
- Background processing minimizes main thread blocking

## Accessibility Features

- Screen reader integration
- VoiceOver support on macOS
- NVDA/JAWS support on Windows
- Speech Dispatcher integration on Linux
- Customizable speech parameters for user preferences
- System event announcements
- UI element speech feedback

## Thread Safety

The Speech Manager is designed to be thread-safe:

- All public APIs can be called from any thread
- Internal state is protected by mutexes
- Callbacks are delivered on appropriate threads
- Audio processing uses dedicated threads

## Memory Management

- Automatic resource cleanup on shutdown
- Reference counting for shared resources
- Configurable cache limits
- Memory pool allocation for performance
- Leak detection in debug builds

## Debugging and Diagnostics

```c
// Enable debug mode
SpeechEnableDebugMode(true);
SpeechSetDebugLevel(3);

// Get system information
char *platformInfo;
SpeechGetSystemInfo(&platformInfo, NULL, NULL);
printf("Platform: %s\n", platformInfo);

// Dump system state
SpeechDumpSystemState("speech_debug.log");

// Validate system integrity
bool isValid;
char *issues;
SpeechValidateSystemIntegrity(&isValid, &issues);
if (!isValid) {
    printf("System issues: %s\n", issues);
}
```

## Contributing

When contributing to the Speech Manager implementation:

1. Maintain API compatibility with the original Mac OS Speech Manager
2. Follow the established coding conventions
3. Add comprehensive error handling
4. Include unit tests for new functionality
5. Update documentation for API changes
6. Test on multiple platforms
7. Consider accessibility implications

## License

This implementation is based on the leaked Apple Macintosh System Software 7.1 source code and is provided for research and educational purposes only. The original code is copyright Apple Computer, Inc.

## References

- [Mac OS Speech Manager API Reference](https://developer.apple.com/documentation/applicationservices/speech_synthesis_manager)
- [System 7.1 Developer Documentation](https://developer.apple.com/library/archive/documentation/mac/pdf/MacintoshToolboxEssentials.pdf)
- [Cross-Platform Speech Synthesis](https://en.wikipedia.org/wiki/Speech_synthesis)