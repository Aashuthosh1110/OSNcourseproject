# Phase 5.1: Concurrency Fix - Thread-Based Client Handling

## Problem Statement

The original Storage Server implementation had a critical concurrency issue:
- The server listened on two sockets: `nm_socket` (for Name Server) and `client_server_socket` (for clients)
- When a client connected, the main loop called `handle_client_connections()` 
- If this function blocked (serving one client), the **entire server would freeze**
- The server could not accept new clients or respond to Name Server commands

This is a **blocking architecture** problem that prevents concurrent operations.

## Solution: Detached Thread Per Client

Phase 5.1 implements a **thread-based concurrency model**:
- The main loop's **only job** is to accept connections and spawn threads
- Each client connection gets its own **detached thread**
- The main thread remains free to handle Name Server commands and accept new clients
- Threads clean up automatically when clients disconnect

## Implementation Details

### 1. New Thread Handler Function

**`void* client_connection_thread(void* arg)`**

This function runs in a separate thread for each connected client.

**Logic:**
1. Extract socket from `void* arg` and free the allocated memory
2. Call `pthread_detach(pthread_self())` to make thread self-cleaning
3. Enter infinite loop:
   - Call `recv_request()` to get commands from client
   - If connection closed (bytes <= 0), log disconnect, close socket, exit thread
   - Validate packet integrity
   - Switch on `request.command` to handle client commands
4. Return `NULL` when client disconnects

**Supported Commands (stubs for now):**
- `CMD_READ` - Read file content (not yet implemented)
- `CMD_WRITE` - Write to file (not yet implemented)
- `CMD_STREAM` - Stream file word-by-word (not yet implemented)
- Unknown commands return error response

### 2. Modified Main Loop

**Before (Blocking):**
```c
if (FD_ISSET(client_server_socket, &read_fds)) {
    int client_socket = accept(...);
    handle_client_connections();  // BLOCKS HERE!
    close(client_socket);
}
```

**After (Non-Blocking):**
```c
if (FD_ISSET(client_server_socket, &read_fds)) {
    int client_socket = accept(...);
    
    // Allocate memory to pass socket to thread
    int* sock_ptr = malloc(sizeof(int));
    *sock_ptr = client_socket;
    
    // Create detached thread
    pthread_t tid;
    pthread_create(&tid, NULL, client_connection_thread, (void*)sock_ptr);
    
    // Main loop continues immediately!
}
```

### Key Points:
1. **Heap allocation**: Socket is passed via `malloc()` because stack variable would be invalid when thread runs
2. **Thread detachment**: `pthread_detach()` means thread cleans up automatically (no `pthread_join()` needed)
3. **Error handling**: If malloc or pthread_create fails, socket is closed and memory freed
4. **Main loop freedom**: Main thread immediately returns to `select()` after spawning thread

## Architecture Flow

### Multi-Client Scenario

```
Main Thread (select loop):
  ├─ Listening on nm_socket (Name Server)
  ├─ Listening on client_server_socket (Clients)
  │
  ├─ [Client A connects]
  │   └─> Spawn Thread A → Handles Client A's commands
  │
  ├─ [Client B connects]
  │   └─> Spawn Thread B → Handles Client B's commands
  │
  ├─ [Name Server sends CMD_UPDATE_ACL]
  │   └─> Main thread handles immediately (no blocking!)
  │
  ├─ [Client C connects]
  │   └─> Spawn Thread C → Handles Client C's commands
  │
  └─ ... continues accepting connections ...

Thread A: while(1) { recv from Client A, process command, send response }
Thread B: while(1) { recv from Client B, process command, send response }
Thread C: while(1) { recv from Client C, process command, send response }
```

### Timeline Example

```
Time  | Main Thread        | Thread A (Client A) | Thread B (Client B)
------|--------------------|---------------------|--------------------
t0    | select() waiting   |                     |
t1    | Accept Client A    |                     |
t2    | Spawn Thread A     | recv_request()      |
t3    | select() waiting   | Processing READ     |
t4    | Accept Client B    | send_response()     |
t5    | Spawn Thread B     | recv_request()      | recv_request()
t6    | select() waiting   | Processing WRITE    | Processing STREAM
t7    | NM sends UPDATE_ACL| Still writing...    | Still streaming...
t8    | Handle UPDATE_ACL  | send_response()     | send_response()
t9    | send OK to NM      |                     |
t10   | select() waiting   | recv_request()      | recv_request()
```

**Key Observation**: Main thread handles Name Server at t7-t9 **while clients are still being served**!

## Benefits

1. **Concurrent Client Handling**: Multiple clients can be served simultaneously
2. **Non-Blocking Main Loop**: Name Server commands handled promptly
3. **Scalability**: Can handle many clients (limited by system resources)
4. **Automatic Cleanup**: Detached threads self-destruct on client disconnect
5. **Simple Design**: No need for thread pools or complex synchronization (yet)

## Thread Safety Considerations

### Current Status: Thread-Safe
- Each thread handles **one client socket** (no shared socket state)
- Name Server socket handled **only by main thread** (no race conditions)
- File operations go through **Name Server** (which serializes them)

### Future Concerns (Phase 5.2+)
When implementing READ/WRITE operations, we'll need to consider:
- **Concurrent file access**: Multiple clients reading/writing same file
- **Metadata updates**: Locking around .meta file modifications
- **Sentence-level locking**: Prevent concurrent writes to same sentence
- **ACL cache**: If we cache ACLs in memory, need locks

## Testing

### Test 1: Multiple Clients Connecting
```bash
# Terminal 1: Start servers
./bin/name_server 8080 &
./bin/storage_server 127.0.0.1 8080 ss_storage 9001 &

# Terminal 2: Client A
./bin/client alice 127.0.0.1 8080

# Terminal 3: Client B
./bin/client bob 127.0.0.1 8080

# Terminal 4: Client C
./bin/client charlie 127.0.0.1 8080

# All clients should connect without blocking each other
```

### Test 2: Name Server Commands During Client Activity
```bash
# With multiple clients connected (as above):

# In Client A terminal:
docs++ > CREATE fileA.txt
docs++ > INFO fileA.txt

# In Client B terminal (simultaneously):
docs++ > CREATE fileB.txt
docs++ > INFO fileB.txt

# Both operations should succeed without blocking
```

### Test 3: Client Disconnect
```bash
# Connect a client
./bin/client alice 127.0.0.1 8080

# Exit client (Ctrl+D or EXIT command)

# Check Storage Server logs:
# Should see: "Client disconnected from socket X"
# Thread should clean up automatically
```

## Code Changes

### Files Modified
- `src/storage_server/storage_server.c`
  - Added `client_connection_thread()` function (~100 lines)
  - Modified main loop to spawn threads instead of blocking
  - Added function prototype

### Lines Changed
- **Added**: ~110 lines (thread function + logic)
- **Modified**: ~15 lines (main loop client handling)
- **Removed**: ~5 lines (old blocking handler stub)

## Known Limitations

1. **No READ/WRITE/STREAM implementation yet**: Commands return "not implemented" error
2. **No thread limit**: Unlimited threads can be created (could exhaust resources)
3. **No graceful shutdown**: Threads not tracked, can't cleanly shut down all clients
4. **No file locking**: Concurrent access to same file not controlled (Phase 5.2+)

## Next Steps (Phase 5.2+)

1. **Implement READ handler**: Read file content, check ACL, return data
2. **Implement WRITE handler**: Write to file, check ACL, update metadata
3. **Implement STREAM handler**: Stream file word-by-word with delays
4. **Add sentence-level locking**: Prevent concurrent writes to same sentence
5. **Add thread pool**: Limit concurrent threads, reuse threads
6. **Track active threads**: Enable graceful shutdown

## Performance Implications

### Advantages
- ✅ **Low latency**: No head-of-line blocking
- ✅ **High throughput**: Parallel processing of requests
- ✅ **Simple model**: Easy to reason about and debug

### Disadvantages
- ❌ **Memory overhead**: Each thread uses ~8MB stack space
- ❌ **Context switching**: Many threads = overhead
- ❌ **Resource exhaustion**: No limit on thread count

### Typical Performance
- **Small deployments** (< 100 clients): Excellent
- **Large deployments** (> 1000 clients): Consider thread pool or async I/O

## Debugging Tips

### Check Active Threads
```bash
# While server running with multiple clients:
ps -T -p $(pgrep storage_server)
# Shows all threads for the process
```

### Monitor Thread Creation
```bash
# Watch Storage Server logs:
tail -f logs/storage_server.log | grep "Client thread started"
```

### Detect Thread Leaks
```bash
# Before: note thread count
ps -T -p $(pgrep storage_server) | wc -l

# Connect and disconnect multiple clients

# After: thread count should return to baseline
# (main thread + 1 per active client)
```

## Summary

Phase 5.1 transforms the Storage Server from a **blocking single-threaded** architecture to a **concurrent multi-threaded** architecture. The main loop now only accepts connections and spawns threads, ensuring the server remains responsive to both Name Server commands and new client connections. Each client gets dedicated thread that handles its requests independently.

This is a **critical foundation** for Phase 5.2+, where we'll implement actual file operations (READ, WRITE, STREAM) on top of this concurrent framework.
