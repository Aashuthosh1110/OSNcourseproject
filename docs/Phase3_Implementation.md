# Phase 3 Implementation Complete: CREATE and DELETE Operations

## Overview
Phase 3 implements the CREATE and DELETE file operations following the complete request-response cycle: **Client → Name Server → Storage Server → Name Server → Client**.

## Components Implemented

### 1. Client (`src/client/client.c`)

#### CREATE Command
- **Function**: `handle_create_command()`
- **Usage**: `CREATE <filename>`
- **Flow**:
  1. Parse and validate filename
  2. Create request packet with `CMD_CREATE` command
  3. Send packet to Name Server
  4. Wait for response
  5. Display success or error message

#### DELETE Command
- **Function**: `handle_delete_command()`
- **Usage**: `DELETE <filename>`
- **Flow**:
  1. Parse and validate filename
  2. Create request packet with `CMD_DELETE` command
  3. Send packet to Name Server
  4. Wait for response
  5. Display success or error message

#### Features
- Filename validation (no path traversal, special characters)
- Error handling with user-friendly messages
- Logging of all operations

### 2. Name Server (`src/name_server/name_server.c`)

#### CREATE Handler
- **Function**: `handle_create_file()`
- **Steps**:
  1. Extract filename from request
  2. Validate filename
  3. Check if file already exists in registry (returns error if exists)
  4. Select storage server using round-robin algorithm
  5. Forward CREATE request to selected storage server
  6. Wait for SS acknowledgment
  7. Add file to Name Server's file registry with metadata
  8. Send success response to client

#### DELETE Handler
- **Function**: `handle_delete_file()`
- **Steps**:
  1. Extract filename from request
  2. Find file in registry (returns error if not found)
  3. Check ownership (only owner can delete)
  4. Get storage server holding the file
  5. Forward DELETE request to storage server
  6. Wait for SS acknowledgment
  7. Remove file from Name Server's registry
  8. Send success response to client

#### Storage Server Selection
- **Function**: `select_storage_server_for_create()`
- **Algorithm**: Round-robin selection
- **Future**: Can be enhanced to least-loaded or other strategies

### 3. Storage Server (`src/storage_server/storage_server.c`)

#### CREATE Handler
- **Function**: `handle_create_request()`
- **Steps**:
  1. Extract filename from request
  2. Construct file paths (data file and metadata file)
  3. Check if file already exists (returns error if exists)
  4. Create empty file on disk
  5. Create metadata file (.meta) with:
     - Owner
     - Created timestamp
     - Modified timestamp
     - Size (0 for new file)
     - Word count (0)
     - Character count (0)
     - Access control list (owner has read/write)
  6. Send success response to Name Server

#### DELETE Handler
- **Function**: `handle_delete_request()`
- **Steps**:
  1. Extract filename from request
  2. Construct file paths
  3. Check if file exists (returns error if not found)
  4. Delete main file from disk
  5. Delete metadata file (.meta)
  6. Delete backup file (.bak) if exists
  7. Send success response to Name Server

#### Metadata Management
- **Function**: `create_file_metadata()`
- **Format**: Key-value pairs in text file
- **Fields**:
  - `owner=<username>`
  - `created=<timestamp>`
  - `modified=<timestamp>`
  - `accessed=<timestamp>`
  - `accessed_by=<username>`
  - `size=0`
  - `word_count=0`
  - `char_count=0`
  - `access_count=1`
  - `access_0=<username>:RW`

## Error Handling

### Client Errors
- Invalid filename format
- Network communication failures
- Empty filename

### Name Server Errors
- File already exists (`STATUS_ERROR_FILE_EXISTS`)
- File not found (`STATUS_ERROR_NOT_FOUND`)
- No storage servers available (`STATUS_ERROR_SERVER_UNAVAILABLE`)
- Not file owner (`STATUS_ERROR_OWNER_REQUIRED`)
- Network errors (`STATUS_ERROR_NETWORK`)

### Storage Server Errors
- File already exists on disk
- File not found on disk
- Permission denied (file system level)
- I/O errors

## Data Structures

### File Registry (Name Server)
```c
typedef struct file_hash_entry {
    char filename[MAX_FILENAME_LEN];
    int ss_socket_fd;
    file_metadata_t metadata;
    struct file_hash_entry* next;
} file_hash_entry_t;
```

### File Metadata
```c
typedef struct {
    char filename[MAX_FILENAME_LEN];
    char owner[MAX_USERNAME_LEN];
    time_t created;
    time_t last_modified;
    time_t last_accessed;
    char last_accessed_by[MAX_USERNAME_LEN];
    size_t size;
    int word_count;
    int char_count;
    char access_list[MAX_CLIENTS][MAX_USERNAME_LEN];
    int access_permissions[MAX_CLIENTS];
    int access_count;
} file_metadata_t;
```

## Protocol

### Request Packet
```c
typedef struct {
    uint32_t magic;                     // PROTOCOL_MAGIC (0xD0C5)
    command_t command;                  // CMD_CREATE or CMD_DELETE
    char username[MAX_USERNAME_LEN];    // Username of requester
    char args[MAX_ARGS_LEN];           // Filename
    uint32_t checksum;                  // Packet integrity
} request_packet_t;
```

### Response Packet
```c
typedef struct {
    uint32_t magic;                         // PROTOCOL_MAGIC
    status_t status;                        // STATUS_OK or error code
    char data[MAX_RESPONSE_DATA_LEN];      // Success message or error details
    uint32_t checksum;                      // Packet integrity
} response_packet_t;
```

## Testing

### Manual Testing
```bash
# Start the system
./scripts/manual_test.sh

# In another terminal, run client
./bin/client localhost 8080

# Test commands
CREATE test1.txt        # Should succeed
CREATE test1.txt        # Should fail (already exists)
CREATE test2.txt        # Should succeed
DELETE test1.txt        # Should succeed (as owner)
DELETE test1.txt        # Should fail (not found)
DELETE test2.txt        # Should succeed
```

### Verification
```bash
# Check files in storage
ls -la test_storage/ss1/

# Check metadata files
cat test_storage/ss1/*.meta

# Check logs
tail -f logs/nm_manual.log
tail -f logs/ss_manual.log
```

## Files Modified/Created

### Modified Files
1. `src/client/client.c`
   - Implemented `handle_create_command()`
   - Implemented `handle_delete_command()`
   - Updated `main()` to call `handle_user_commands()`

2. `src/name_server/name_server.c`
   - Implemented `handle_create_file()`
   - Implemented `handle_delete_file()`
   - Implemented `select_storage_server_for_create()`
   - Updated `process_connection_data()` to handle CREATE/DELETE

3. `src/storage_server/storage_server.c`
   - Implemented `handle_create_request()`
   - Implemented `handle_delete_request()`
   - Implemented `create_file_metadata()`
   - Updated `handle_nm_commands()` to handle CREATE/DELETE
   - Added hostname resolution support

4. `src/common/common.c`
   - Already had `validate_filename()` implemented

### Created Files
1. `scripts/test_phase3.sh` - Automated integration tests
2. `scripts/manual_test.sh` - Manual testing helper
3. `docs/Phase3_Implementation.md` - This documentation

## Key Design Decisions

### 1. **Synchronous Communication**
- Name Server waits for Storage Server response before responding to client
- Ensures consistency: file is registered only after successful creation on disk
- Trade-off: Blocking operation, but acceptable for MVP

### 2. **Metadata Storage**
- Dual storage: Name Server (in-memory) + Storage Server (persistent .meta files)
- Name Server: Fast lookups for file location
- Storage Server: Persistent metadata survives restarts

### 3. **Access Control**
- Preliminary check in Name Server (ownership for DELETE)
- Can be enhanced to full SS-side validation in future phases
- Owner always has read/write access

### 4. **Round-Robin Load Balancing**
- Simple but effective for initial implementation
- Can be upgraded to least-loaded or other strategies
- Ensures even distribution across storage servers

### 5. **Filename Validation**
- Client-side and Name Server-side validation
- Prevents path traversal attacks
- Ensures compatibility across file systems

## Future Enhancements (Phase 4+)

1. **Asynchronous Operations**
   - Non-blocking I/O with pending request tracking
   - Better scalability for multiple concurrent operations

2. **Two-Phase Commit**
   - Reserve in registry before SS creation
   - Rollback support for partial failures

3. **Advanced Load Balancing**
   - Least-loaded storage server selection
   - Consider disk space, CPU, network latency

4. **Caching**
   - LRU cache for frequent file lookups
   - Reduce hash table lookups

5. **Replication**
   - Create files on multiple storage servers
   - Fault tolerance and high availability

## Compilation

```bash
make clean
make
```

## Running the System

```bash
# Terminal 1: Name Server
./bin/name_server 8080

# Terminal 2: Storage Server
./bin/storage_server localhost 8080 ./storage/ss1 9001

# Terminal 3: Client
./bin/client localhost 8080
```

## Known Limitations

1. **Blocking I/O**: Name Server blocks waiting for Storage Server responses
2. **No Transaction Support**: Partial failures can leave inconsistent state
3. **Single Storage Server Selection**: No failover if selected SS is down
4. **In-Memory Registry**: Name Server registry lost on restart (Phase 5 will add persistence)
5. **Basic Error Recovery**: Limited rollback mechanisms

## Success Criteria Met ✓

- [x] Client can send CREATE command
- [x] Name Server routes CREATE to Storage Server
- [x] Storage Server creates file and metadata on disk
- [x] Name Server updates file registry
- [x] Client receives success confirmation
- [x] Client can send DELETE command
- [x] Name Server enforces ownership for DELETE
- [x] Storage Server deletes file from disk
- [x] Name Server removes file from registry
- [x] Error handling for duplicate files, missing files
- [x] Proper logging at all levels
- [x] Hostname resolution support

## Next Steps: Phase 4

1. Implement VIEW command (list files with flags)
2. Implement INFO command (file metadata display)
3. Implement LIST command (list all users)
4. Implement ADDACCESS/REMACCESS (access control)
5. Begin READ/WRITE operations planning
