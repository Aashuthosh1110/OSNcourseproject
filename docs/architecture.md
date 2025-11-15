# System Architecture

## Overview

Docs++ is a distributed document collaboration system consisting of three main components that communicate over TCP sockets:

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   User Client   │◄──►│   Name Server   │◄──►│ Storage Server  │
│                 │    │      (NM)       │    │      (SS)       │
│ - User Interface│    │ - Coordination  │    │ - File Storage  │
│ - Command Parser│    │ - File Registry │    │ - Sentence Lock │
│ - Communication │    │ - Access Control│    │ - Streaming     │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## Component Details

### Name Server (NM)

**Role**: Central coordinator and metadata manager

**Responsibilities**:
- Client and Storage Server registration
- File location management and routing
- Access control enforcement
- Efficient file search (O(log n) or better)
- Caching for performance optimization
- System-wide logging and monitoring

**Key Data Structures**:
- Storage Server registry (hash table)
- File metadata registry (trie for efficient search)
- Client session management
- Access control lists (ACL)

### Storage Server (SS)

**Role**: Physical file storage and direct client communication

**Responsibilities**:
- File CRUD operations (Create, Read, Update, Delete)
- Sentence-level locking for concurrent editing
- File streaming with word-by-word delivery
- Data persistence and backup management
- Undo functionality implementation
- Direct client communication for file operations

**Key Features**:
- Concurrent file access with sentence locking
- Atomic write operations with backup/restore
- Word-by-word streaming with 0.1s delays
- File metadata synchronization

### User Client

**Role**: User interface and command processing

**Responsibilities**:
- Interactive command-line interface
- User authentication and session management
- Command parsing and validation
- Name Server communication for metadata operations
- Direct Storage Server communication for file I/O
- Error handling and user feedback

## Communication Patterns

### 1. Registration Phase
```
Client/SS → NM: Registration request
NM → Client/SS: Acknowledgment
```

### 2. Metadata Operations (VIEW, LIST, INFO, ACCESS)
```
Client → NM: Request
NM → Client: Response (direct handling)
```

### 3. File Operations (READ, WRITE, STREAM)
```
Client → NM: File operation request
NM → Client: Storage Server location
Client → SS: Direct file operation
SS → Client: File data/acknowledgment
```

### 4. Administrative Operations (CREATE, DELETE)
```
Client → NM: Administrative request
NM → SS: Execute operation
SS → NM: Acknowledgment
NM → Client: Result
```

## Data Flow

### File Creation
1. Client sends CREATE request to NM
2. NM selects optimal Storage Server (load balancing)
3. NM forwards request to selected SS
4. SS creates file and updates local metadata
5. SS acknowledges to NM
6. NM updates global file registry
7. NM responds to Client

### File Reading
1. Client sends READ request to NM
2. NM locates file in registry (efficient search)
3. NM returns SS information to Client
4. Client connects directly to SS
5. SS sends file content to Client
6. SS updates access timestamps

### File Writing
1. Client sends WRITE request to NM
2. NM checks write permissions
3. NM returns SS information if authorized
4. Client connects to SS and requests sentence lock
5. SS grants lock if sentence available
6. Client sends write operations
7. SS processes writes atomically
8. SS creates backup before committing changes
9. SS releases lock and acknowledges completion

### File Streaming
1. Similar to READ but with special streaming protocol
2. SS sends words individually with 0.1s delays
3. Connection maintained until complete file transmitted
4. Special STOP message indicates end of stream

## Concurrency Model

### Sentence-Level Locking
- Files are divided into sentences (delimited by `.`, `!`, `?`)
- Each sentence can be locked independently
- Multiple users can read simultaneously
- Only one user can edit a sentence at a time
- Locks are automatically released after ETIRW command or timeout

### Lock Management
```c
typedef struct {
    char filename[MAX_FILENAME_LEN];
    int sentence_index;
    char locked_by[MAX_USERNAME_LEN];
    time_t lock_time;
    time_t timeout;
} sentence_lock_t;
```

## Error Handling

### Error Code System
- Standardized error codes across all components
- Hierarchical error classification
- User-friendly error messages
- Detailed logging for debugging

### Fault Tolerance
- Graceful handling of client disconnections
- Storage Server failure detection
- Automatic lock cleanup on timeout
- Data persistence across restarts

## Performance Optimizations

### Name Server
- **Trie-based file search**: O(m) where m is filename length
- **HashMap for SS lookup**: O(1) average case
- **LRU cache for recent searches**: Configurable size
- **Connection pooling**: Reuse client connections

### Storage Server
- **Memory-mapped files**: For large file operations
- **Write-through caching**: Balance performance and durability
- **Async I/O**: Non-blocking file operations where possible
- **Batch operations**: Group multiple writes when possible

### Network
- **Persistent connections**: Avoid connection overhead
- **Message pipelining**: Send multiple commands without waiting
- **Compression**: Optional compression for large file transfers
- **Binary protocol**: Efficient serialization format

## Security Considerations

### Access Control
- User-based authentication
- File-level read/write permissions
- Owner-based access management
- Access control list (ACL) enforcement

### Data Integrity
- Atomic write operations
- Backup before modification
- Checksum verification (future enhancement)
- Transaction logging

## Scalability Design

### Horizontal Scaling
- Multiple Storage Servers support
- Load balancing algorithms
- File distribution strategies
- Client load distribution

### Vertical Scaling
- Efficient data structures
- Memory optimization
- I/O optimization
- CPU utilization optimization

## Future Enhancements

### Replication (Bonus Feature)
- Master-slave replication
- Asynchronous replication
- Failure detection and recovery
- Data consistency guarantees

### Hierarchical Storage (Bonus Feature)
- Folder structure support
- Path-based file organization
- Nested permission inheritance
- Efficient directory operations