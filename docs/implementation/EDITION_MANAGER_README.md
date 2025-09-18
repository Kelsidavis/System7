# Edition Manager - System 7 Publish/Subscribe Implementation

A complete, portable implementation of Apple's System 7.1 Edition Manager, providing live data sharing and linking between applications for dynamic document collaboration.

## Overview

The Edition Manager was a revolutionary component of Mac OS System 7 that enabled publish/subscribe functionality, allowing applications to share live data through edition containers. This implementation provides full API compatibility while adding modern cross-platform support and integration with contemporary data sharing systems.

## Features

### Core Edition Manager Functionality
- **Complete API Compatibility**: Full implementation of the original Mac OS Edition Manager API
- **Publisher Management**: Create and manage data publishers that share content
- **Subscriber Management**: Create and manage data subscribers that consume shared content
- **Edition Containers**: File-based storage for shared data with multiple format support
- **Live Data Synchronization**: Real-time updates between publishers and subscribers
- **Format Negotiation**: Automatic data format conversion and compatibility
- **Edition Browser**: User interface for finding and selecting edition containers
- **Notification System**: Event-driven updates and callbacks

### Modern Enhancements
- **Cross-Platform Support**: Works on Linux, macOS, Windows, and FreeBSD
- **Thread Safety**: Multi-threaded implementation with proper synchronization
- **Network Sharing**: Share editions across network connections
- **Cloud Integration**: Support for cloud storage providers (Dropbox, Google Drive, etc.)
- **Real-Time Collaboration**: Integration with modern collaboration frameworks
- **Security Features**: Data encryption, digital signatures, and access control
- **Performance Optimization**: Caching, compression, and efficient I/O

## Architecture

```
Edition Manager
├── Core Components
│   ├── EditionManagerCore      - Main API and section management
│   ├── PublisherManager        - Publisher creation and data publishing
│   ├── SubscriberManager       - Subscriber creation and data consumption
│   ├── EditionFiles            - Edition file storage and management
│   ├── DataSynchronization     - Live data updating and synchronization
│   ├── FormatNegotiation       - Data format conversion and negotiation
│   ├── EditionBrowser          - Edition browsing and selection interface
│   └── NotificationSystem      - Update notifications and callbacks
└── Platform Abstraction
    ├── Modern Data Sharing     - OLE/COM, Pasteboard, D-Bus integration
    ├── Cloud Storage           - Dropbox, Google Drive, OneDrive, iCloud
    ├── Network Protocols       - TCP/IP, HTTP, WebSocket support
    ├── Real-Time Collaboration - WebRTC, ShareJS, Y.js integration
    ├── UI Framework Support    - Qt, GTK, Electron, Web integration
    └── Security & Permissions  - Encryption, signing, access control
```

## Quick Start

### Building the Edition Manager

```bash
# Clone the repository
cd /home/k/System7.1-Portable

# Build the Edition Manager
make -f EditionManager.mk

# Run tests
make -f EditionManager.mk check

# Install system-wide
sudo make -f EditionManager.mk install
```

### Basic Usage Example

```c
#include "EditionManager/EditionManager.h"

int main() {
    // Initialize the Edition Manager
    OSErr err = InitEditionPack();
    if (err != noErr) {
        fprintf(stderr, "Failed to initialize Edition Manager: %d\n", err);
        return 1;
    }

    // Create a publisher section
    EditionContainerSpec container;
    strcpy(container.theFile.path, "./MyEdition.edition");
    strcpy(container.theFile.name, "MyEdition.edition");
    container.thePart = 0;
    strcpy(container.thePartName, "Main");

    FSSpec document;
    strcpy(document.path, "./MyDocument.txt");
    strcpy(document.name, "MyDocument.txt");

    SectionHandle publisherH;
    err = NewSection(&container, &document, stPublisher, 1, pumOnSave, &publisherH);
    if (err != noErr) {
        fprintf(stderr, "Failed to create publisher: %d\n", err);
        QuitEditionPack();
        return 1;
    }

    // Register the publisher
    bool aliasUpdated;
    err = RegisterSection(&document, publisherH, &aliasUpdated);
    if (err != noErr) {
        fprintf(stderr, "Failed to register publisher: %d\n", err);
        QuitEditionPack();
        return 1;
    }

    // Create edition container file
    err = CreateEditionContainerFile(&container.theFile, 'EDIT', 0);
    if (err != noErr && err != dupFNErr) {
        fprintf(stderr, "Failed to create edition file: %d\n", err);
        QuitEditionPack();
        return 1;
    }

    // Open edition for writing
    EditionRefNum refNum;
    err = OpenNewEdition(publisherH, 'EDIT', &document, &refNum);
    if (err != noErr) {
        fprintf(stderr, "Failed to open edition: %d\n", err);
        QuitEditionPack();
        return 1;
    }

    // Write data to edition
    const char* data = "Hello, Edition Manager!";
    Size dataSize = strlen(data);
    err = WriteEdition(refNum, 'TEXT', data, dataSize);
    if (err != noErr) {
        fprintf(stderr, "Failed to write edition data: %d\n", err);
    }

    // Close edition
    CloseEdition(refNum, true);

    // Clean up
    UnRegisterSection(publisherH);
    QuitEditionPack();

    printf("Edition Manager example completed successfully!\n");
    return 0;
}
```

### Subscriber Example

```c
#include "EditionManager/EditionManager.h"

void subscribeToEdition() {
    // Create a subscriber section
    EditionContainerSpec container;
    strcpy(container.theFile.path, "./MyEdition.edition");
    strcpy(container.theFile.name, "MyEdition.edition");

    FSSpec document;
    strcpy(document.path, "./SubscriberDoc.txt");
    strcpy(document.name, "SubscriberDoc.txt");

    SectionHandle subscriberH;
    OSErr err = NewSection(&container, &document, stSubscriber, 2, sumAutomatic, &subscriberH);
    if (err != noErr) return;

    // Register subscriber
    bool aliasUpdated;
    RegisterSection(&document, subscriberH, &aliasUpdated);

    // Open edition for reading
    EditionRefNum refNum;
    err = OpenEdition(subscriberH, &refNum);
    if (err != noErr) return;

    // Read data from edition
    char buffer[1024];
    Size bufferSize = sizeof(buffer);
    err = ReadEdition(refNum, 'TEXT', buffer, &bufferSize);
    if (err == noErr) {
        buffer[bufferSize] = '\0';
        printf("Read from edition: %s\n", buffer);
    }

    // Close and clean up
    CloseEdition(refNum, false);
    UnRegisterSection(subscriberH);
}
```

## API Reference

### Core Functions

#### Initialization
- `OSErr InitEditionPack(void)` - Initialize the Edition Manager
- `OSErr QuitEditionPack(void)` - Clean up the Edition Manager

#### Section Management
- `OSErr NewSection(...)` - Create a new publisher or subscriber section
- `OSErr RegisterSection(...)` - Register a section with the system
- `OSErr UnRegisterSection(SectionHandle)` - Unregister a section
- `OSErr IsRegisteredSection(SectionHandle)` - Check if section is registered

#### Edition Container Management
- `OSErr CreateEditionContainerFile(...)` - Create a new edition container file
- `OSErr DeleteEditionContainerFile(...)` - Delete an edition container file

#### Edition I/O
- `OSErr OpenEdition(...)` - Open edition for reading (subscriber)
- `OSErr OpenNewEdition(...)` - Open edition for writing (publisher)
- `OSErr CloseEdition(...)` - Close an open edition
- `OSErr ReadEdition(...)` - Read data from edition
- `OSErr WriteEdition(...)` - Write data to edition

#### Format Operations
- `OSErr EditionHasFormat(...)` - Check if edition contains specific format
- `OSErr GetEditionFormatMark(...)` - Get current position in format data
- `OSErr SetEditionFormatMark(...)` - Set position in format data

#### User Interface
- `OSErr NewSubscriberDialog(...)` - Show subscriber creation dialog
- `OSErr NewPublisherDialog(...)` - Show publisher creation dialog
- `OSErr SectionOptionsDialog(...)` - Show section options dialog

### Data Structures

#### SectionRecord
```c
struct SectionRecord {
    uint8_t version;            // Version (always 0x01)
    SectionType kind;           // stSubscriber or stPublisher
    UpdateMode mode;            // auto or manual update mode
    TimeStamp mdDate;           // last modification date
    int32_t sectionID;          // unique section identifier
    int32_t refCon;             // application-specific data
    Handle alias;               // alias to edition container
    int32_t subPart;            // part of container file
    SectionHandle nextSection;  // linked list pointer
    Handle controlBlock;        // internal control data
    EditionRefNum refNum;       // edition reference when open
};
```

#### EditionContainerSpec
```c
struct EditionContainerSpec {
    FSSpec theFile;             // edition file specification
    ScriptCode theFileScript;   // file name script code
    int32_t thePart;            // part number within file
    Str31 thePartName;          // part name
    ScriptCode thePartScript;   // part name script code
};
```

## Advanced Features

### Format Negotiation

The Edition Manager includes a sophisticated format negotiation system that allows applications with different data format capabilities to share data:

```c
// Register a custom format converter
OSErr RegisterFormatConverter('RTFD', 'TEXT', ConvertRTFDToText, 10);

// Negotiate best format between publisher and subscriber
FormatType publisherFormats[] = {'RTFD', 'TEXT', 'PICT'};
FormatType subscriberFormats[] = {'TEXT', 'HTML'};
FormatType bestFormat;
float compatibility;

OSErr err = NegotiateBestFormat(publisherFormats, 3, subscriberFormats, 2,
                               &bestFormat, &compatibility);
```

### Real-Time Synchronization

Enable automatic synchronization between publishers and subscribers:

```c
// Set automatic synchronization mode
SetSynchronizationMode(publisherH, kSyncRealtime);

// Register for update notifications
SetNotificationCallback(subscriberH, MyUpdateCallback, userData,
                       kNotificationEventDataChanged);

void MyUpdateCallback(const NotificationEvent* event, void* userData) {
    printf("Data updated in section %p\n", event->sectionH);
    // Refresh subscriber data
    RefreshSubscriberData((SectionHandle)userData);
}
```

### Network Sharing

Share editions across network connections:

```c
// Enable network sharing for a publisher
NetworkConfig networkConfig = {
    .protocol = kNetworkProtocolHTTP,
    .serverAddress = "192.168.1.100",
    .port = 8080,
    .useEncryption = true
};

OSErr err = PublishEditionOnNetwork(publisherH, &networkConfig);

// Subscribe to network edition
OSErr err = SubscribeToNetworkEdition("http://192.168.1.100:8080/edition1", subscriberH);
```

### Cloud Storage Integration

Sync editions with cloud storage:

```c
// Configure cloud storage
CloudStorageConfig cloudConfig = {
    .provider = kCloudProviderDropbox,
    .apiKey = "your_api_key",
    .accessToken = "your_access_token",
    .basePath = "/EditionManager",
    .syncEnabled = true,
    .syncInterval = 60  // seconds
};

OSErr err = InitializeCloudStorage(kCloudProviderDropbox, &cloudConfig);

// Upload edition to cloud
OSErr err = UploadEditionToCloud(&container);

// Enable automatic sync
OSErr err = SyncEditionWithCloud(&container, false);
```

## Platform Integration

### Windows Integration

On Windows, the Edition Manager integrates with OLE/COM for data sharing:

```c
#ifdef _WIN32
// Initialize OLE integration
InitializeWindowsOLE();

// Create OLE data object for publisher
void* oleObject;
CreateOLEDataObject(publisherH, &oleObject);

// Update OLE data when edition changes
UpdateOLEData(oleObject, 'TEXT', data, dataSize);
#endif
```

### macOS Integration

On macOS, integration with NSPasteboard and modern sharing APIs:

```c
#ifdef __APPLE__
// Initialize pasteboard integration
InitializeMacOSPasteboard();

// Create pasteboard item
void* pasteboardItem;
CreatePasteboardItem(publisherH, &pasteboardItem);

// Update pasteboard data
UpdatePasteboardData(pasteboardItem, 'TEXT', data, dataSize);
#endif
```

### Linux Integration

On Linux, integration with D-Bus for inter-application communication:

```c
#ifdef __linux__
// Initialize D-Bus connection
InitializeDBusConnection();

// Create D-Bus service for publisher
CreateDBusService(publisherH, "com.example.EditionPublisher");

// Publish data via D-Bus
PublishDataViaDBus("com.example.EditionPublisher", 'TEXT', data, dataSize);
#endif
```

## Error Handling

The Edition Manager uses the classic Mac OS error code system:

```c
OSErr err = SomeEditionManagerFunction();
switch (err) {
    case noErr:
        // Success
        break;
    case editionMgrInitErr:
        fprintf(stderr, "Edition Manager initialization failed\n");
        break;
    case badSectionErr:
        fprintf(stderr, "Invalid section handle\n");
        break;
    case notAnEditionContainerErr:
        fprintf(stderr, "File is not an edition container\n");
        break;
    case badEditionFileErr:
        fprintf(stderr, "Edition file is corrupted or invalid\n");
        break;
    default:
        fprintf(stderr, "Unknown error: %d\n", err);
        break;
}
```

## Configuration

### Build Options

The Edition Manager can be configured at build time:

```bash
# Debug build with extensive logging
make -f EditionManager.mk DEBUG=1

# Release build with optimizations
make -f EditionManager.mk

# Platform-specific build
make -f EditionManager.mk PLATFORM=linux
```

### Runtime Configuration

Configure the Edition Manager at runtime:

```c
// Set synchronization interval
SetSyncInterval(500);  // 500ms

// Enable notification logging
EnableNotificationLogging("/var/log/editionmanager.log", true);

// Set memory allocation strategy
MemoryConfig memConfig = {
    .strategy = kMemoryStrategyPooled,
    .poolSize = 16 * 1024 * 1024,  // 16MB
    .useGarbageCollection = false
};
InitializePlatformMemory(&memConfig);
```

## Testing

### Unit Tests

Run the complete test suite:

```bash
make -f EditionManager.mk check
```

### Integration Tests

Test with real applications:

```bash
# Build test applications
make -f EditionManager.mk test

# Run publisher test
./bin/publisher_test

# Run subscriber test (in another terminal)
./bin/subscriber_test
```

### Performance Testing

Benchmark the Edition Manager:

```bash
make -f EditionManager.mk benchmark
```

### Memory Testing

Check for memory leaks:

```bash
make -f EditionManager.mk valgrind
```

## Performance Considerations

### Memory Usage

The Edition Manager is designed to be memory-efficient:

- **Edition Files**: Lazy loading of format data
- **Caching**: Configurable preview and format caching
- **Memory Pools**: Optional memory pooling for frequent allocations

### Network Performance

For network sharing:

- **Compression**: Automatic data compression for network transfer
- **Batching**: Batch multiple updates for efficiency
- **Caching**: Local caching of remote edition data

### Disk I/O

Edition file performance:

- **Buffered I/O**: Configurable I/O buffer sizes
- **Async Operations**: Background file operations
- **Compaction**: Periodic file compaction to reduce fragmentation

## Security

### Data Protection

- **Encryption**: AES-256 encryption for sensitive edition data
- **Digital Signatures**: Code signing and data integrity verification
- **Access Control**: File-based and application-based permissions

### Network Security

- **TLS/SSL**: Encrypted network communication
- **Authentication**: Token-based authentication for network sharing
- **Firewall Integration**: Automatic firewall rule management

## Troubleshooting

### Common Issues

#### Edition Manager Won't Initialize
```c
OSErr err = InitEditionPack();
if (err == editionMgrInitErr) {
    // Check if another instance is running
    // Verify file permissions
    // Check available memory
}
```

#### Edition File Corruption
```c
bool isValid;
OSErr err = VerifyEditionFile(&fileSpec, &isValid);
if (!isValid) {
    // Attempt repair
    err = RepairEditionFile(&fileSpec);
}
```

#### Network Connection Issues
```c
bool isConnected;
char remoteAddress[256];
OSErr err = GetNetworkSyncStatus(sectionH, &isConnected, remoteAddress, sizeof(remoteAddress));
if (!isConnected) {
    // Retry connection
    // Check network configuration
    // Verify credentials
}
```

### Debug Logging

Enable debug logging for troubleshooting:

```c
// Enable verbose logging
LoggingConfig logConfig = {
    .level = kLogLevelVerbose,
    .logFilePath = "/tmp/editionmanager_debug.log",
    .useConsole = true,
    .maxLogSize = 10 * 1024 * 1024  // 10MB
};
InitializePlatformLogging(&logConfig);
```

## Migration from Classic Mac OS

### API Compatibility

The portable Edition Manager maintains full API compatibility with the original Mac OS implementation:

```c
// Original Mac OS code works unchanged
OSErr err = InitEditionPack();
// ... existing code ...
QuitEditionPack();
```

### Data Format Migration

Edition files created on classic Mac OS can be opened directly:

```c
// Open classic Mac OS edition file
EditionContainerSpec classicContainer;
strcpy(classicContainer.theFile.path, "/path/to/classic/edition");
// ... rest of code works normally ...
```

### Modern Enhancements

Take advantage of modern features while maintaining compatibility:

```c
// Use classic API
OSErr err = NewSection(&container, &document, stPublisher, 1, pumOnSave, &publisherH);

// Add modern enhancements
SetSynchronizationMode(publisherH, kSyncRealtime);
EnableNetworkSharing(publisherH, &networkConfig);
```

## Contributing

### Development Setup

```bash
# Set up development environment
make -f EditionManager.mk dev-setup

# Create platform-specific implementations
make -f EditionManager.mk platform-stubs

# Format code
make -f EditionManager.mk format

# Run static analysis
make -f EditionManager.mk analyze
```

### Adding New Features

1. **Core Features**: Add to appropriate manager (Publisher, Subscriber, etc.)
2. **Platform Features**: Implement in platform-specific files
3. **Tests**: Add unit tests and integration tests
4. **Documentation**: Update API documentation

### Platform Support

To add support for a new platform:

1. Create platform directory: `src/platform/newplatform/`
2. Implement required functions in platform-specific files
3. Add platform detection to Makefile
4. Test with platform-specific features

## License

This implementation is provided for educational and research purposes. The original Mac OS Edition Manager was developed by Apple Computer, Inc.

## References

- [Inside Macintosh: Edition Manager](https://developer.apple.com/library/archive/documentation/mac/pdf/Interapplication_Communication/Edition_Manager.pdf)
- [System 7.1 Technical Documentation](https://archive.org/details/mac-system-7.1-docs)
- [Publish and Subscribe Architecture](https://developer.apple.com/library/archive/documentation/mac/IAC/IAC-99.html)

---

For more information, examples, and updates, visit the project repository at: `/home/k/System7.1-Portable/`