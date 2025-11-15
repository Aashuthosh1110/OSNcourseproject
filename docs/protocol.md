# Network Protocol Specification

## Protocol Overview

Docs++ uses a custom TCP-based protocol for communication between components. The protocol is designed for reliability, efficiency, and extensibility.

## Message Format

All messages use a standardized binary format:

```c
typedef struct {
    uint32_t magic;           // Protocol magic number (0xDOC5)
    uint16_t version;         // Protocol version
    uint16_t type;           // Message type
    uint32_t length;         // Message length
    uint32_t sequence;       // Sequence number
    uint32_t flags;          // Message flags
    char sender[64];         // Sender identification
    char data[MAX_DATA];     // Variable length data
} message_header_t;
```

## Message Types

### Registration Messages
- `MSG_CLIENT_REGISTER` (0x0001) - Client registration
- `MSG_SS_REGISTER` (0x0002) - Storage Server registration
- `MSG_REGISTER_ACK` (0x0003) - Registration acknowledgment

### File Operation Messages
- `MSG_CREATE_FILE` (0x0100) - Create new file
- `MSG_READ_FILE` (0x0101) - Read file content
- `MSG_WRITE_FILE` (0x0102) - Write to file
- `MSG_DELETE_FILE` (0x0103) - Delete file
- `MSG_INFO_FILE` (0x0104) - Get file information
- `MSG_STREAM_FILE` (0x0105) - Stream file content

### Metadata Messages
- `MSG_VIEW_FILES` (0x0200) - List files
- `MSG_LIST_USERS` (0x0201) - List users
- `MSG_ADD_ACCESS` (0x0202) - Grant file access
- `MSG_REM_ACCESS` (0x0203) - Remove file access

### System Messages
- `MSG_HEARTBEAT` (0x0300) - Keep-alive message
- `MSG_ERROR` (0x0301) - Error response
- `MSG_ACK` (0x0302) - Acknowledgment
- `MSG_STOP` (0x0303) - Stop/End message

## Communication Flows

### Client Registration
```
Client                    Name Server
   |                           |
   |--- MSG_CLIENT_REGISTER -->|
   |                           |
   |<-- MSG_REGISTER_ACK ------|
   |                           |
```

### File Creation
```
Client              Name Server         Storage Server
   |                     |                     |
   |-- MSG_CREATE_FILE ->|                     |
   |                     |-- MSG_CREATE_FILE->|
   |                     |                     |
   |                     |<--- MSG_ACK -------|
   |<--- MSG_ACK --------|                     |
   |                     |                     |
```

### File Reading
```
Client              Name Server         Storage Server
   |                     |                     |
   |-- MSG_READ_FILE --->|                     |
   |                     |                     |
   |<-- SS_INFO ---------|                     |
   |                                           |
   |------------- MSG_READ_FILE ------------->|
   |                                           |
   |<------------ FILE_DATA -----------------|
   |                                           |
```

## Data Serialization

### Registration Data
```c
typedef struct {
    char component_type[16];  // "CLIENT" or "STORAGE_SERVER"
    char ip[INET_ADDRSTRLEN];
    uint16_t nm_port;
    uint16_t client_port;     // Storage servers only
    char username[64];        // Clients only
    uint32_t file_count;      // Storage servers only
    char files[][256];        // Storage servers only
} registration_data_t;
```

### File Operation Data
```c
typedef struct {
    char operation[32];       // Operation type
    char filename[256];       // Target file
    char username[64];        // Requesting user
    uint32_t sentence_index;  // For WRITE operations
    uint32_t word_index;      // For WRITE operations
    uint32_t flags;           // Operation flags
    uint32_t data_length;     // Length of following data
    char data[];              // Variable length data
} file_operation_t;
```

## Error Handling

### Error Message Format
```c
typedef struct {
    uint32_t error_code;      // Standardized error code
    char error_message[256];  // Human-readable message
    char context[512];        // Additional context
} error_response_t;
```

### Standard Error Codes
- 1001: File not found
- 1002: Access denied
- 1003: File locked
- 1004: Invalid index
- 1005: Server unavailable
- 1006: File already exists
- 1007: Invalid filename
- 1008: Invalid username

## Connection Management

### Connection States
- `DISCONNECTED` - No connection established
- `CONNECTING` - Connection attempt in progress  
- `CONNECTED` - Connection established
- `AUTHENTICATED` - Authentication completed
- `ACTIVE` - Normal operation state
- `ERROR` - Error state, connection may be unstable

### Heartbeat Protocol
- Heartbeat messages sent every 30 seconds
- Connection timeout after 60 seconds without response
- Automatic reconnection attempts with exponential backoff

## Security Considerations

### Authentication
- Username-based authentication
- Session tokens for persistent connections
- Access control validation on each operation

### Data Integrity
- Message sequence numbers to detect lost messages
- Checksums for data integrity verification
- Atomic operations to prevent partial updates

## Protocol Extensions

### Future Enhancements
- Message compression for large data transfers
- Encryption for secure communications
- Multi-part messages for large files
- Batch operations for efficiency

### Backward Compatibility
- Version negotiation during handshake
- Feature flags for optional capabilities
- Graceful degradation for unsupported features