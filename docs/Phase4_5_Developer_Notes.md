# Phase 4.5 Developer Notes

## What Changed for Developers

### High-Level Changes
Phase 4.5 adds **ACL persistence** by making the Storage Server's `.meta` file the authoritative source for access control. Previously, ACL changes were only stored in the Name Server's memory and were lost on restart.

### Key Architectural Shift

**Before:**
- Name Server maintains ACL in memory only
- Storage Server writes initial ACL on CREATE but never updates it
- ACL changes (ADDACCESS/REMACCESS) only affect NM memory
- Restart = lost ACL changes

**After:**
- Name Server maintains ACL cache in memory
- Storage Server is the **source of truth** (.meta file)
- ADDACCESS/REMACCESS trigger synchronous write to SS .meta
- Restart = SS .meta persists ACL, NM can rebuild cache

## New Protocol Commands

### CMD_UPDATE_ACL
- **Direction**: Name Server → Storage Server
- **Purpose**: Instruct SS to update .meta file with new ACL
- **Args Format**: `"<filename> <serialized_acl>"`
  - Example: `"myfile.txt alice:RW,bob:R,charlie:RW"`
- **Response**: STATUS_OK or error status

### CMD_GET_ACL
- **Status**: Reserved for future use (Phase 5+)
- **Planned Use**: Allow NM to query SS for current ACL (for cache rebuild)

## New Functions (Storage Server)

### `char* serialize_acl_from_meta(const file_metadata_t* meta)`
Converts ACL from metadata struct to compact string.

**Input**: Metadata struct with ACL arrays
**Output**: Heap-allocated string (caller must free)
**Format**: `"user1:RW,user2:R,user3:-"`

**Permission Encoding**:
- `RW` = Read + Write
- `R` = Read only
- `-` = No permissions (or ACCESS_NONE)

**Example**:
```c
file_metadata_t meta;
// ... populate meta with 3 ACL entries ...
char* acl = serialize_acl_from_meta(&meta);
// acl = "alice:RW,bob:R,charlie:RW"
free(acl); // caller must free
```

### `int parse_acl_into_meta(file_metadata_t* meta, const char* acl_str)`
Parses ACL string into metadata struct.

**Input**: 
- `meta` - Metadata struct to populate (existing ACL will be cleared)
- `acl_str` - Serialized ACL string

**Output**: 0 on success, -1 on error

**Notes**:
- Resets `meta->access_count` to 0 before parsing
- Write permission implicitly adds read permission
- Ignores malformed entries (no ':' separator)

**Example**:
```c
file_metadata_t meta;
memset(&meta, 0, sizeof(meta));
parse_acl_into_meta(&meta, "alice:RW,bob:R");
// Result:
//   meta.access_count = 2
//   meta.access_list[0] = "alice", access_permissions[0] = ACCESS_BOTH
//   meta.access_list[1] = "bob", access_permissions[1] = ACCESS_READ
```

### `void handle_update_acl_request(request_packet_t* req)`
Handler for CMD_UPDATE_ACL from Name Server.

**Request Args**: `"<filename> <acl_string>"`
**Response**: STATUS_OK or error

**Logic**:
1. Parse filename and ACL string from req->args
2. Build .meta file path
3. Read existing .meta to preserve non-ACL fields (owner, timestamps, size)
4. Parse new ACL into temp metadata struct
5. Rewrite .meta file with preserved fields + new ACL
6. Send response back to NM

**Error Handling**:
- File not found → STATUS_ERROR_NOT_FOUND
- Invalid args → STATUS_ERROR_INVALID_ARGS
- Write failure → STATUS_ERROR_INTERNAL

## Modified Functions (Name Server)

### `handle_addaccess()`
**New Behavior**: After updating in-memory ACL, persist to SS.

**Steps**:
1. Validate ownership and args (unchanged)
2. **NEW**: Snapshot current metadata (for rollback)
3. Update in-memory ACL (unchanged)
4. **NEW**: Serialize ACL to string
5. **NEW**: Build CMD_UPDATE_ACL request with serialized ACL
6. **NEW**: Send to SS and wait for response
7. **NEW**: On success → reply to client
8. **NEW**: On failure → rollback in-memory ACL, reply error to client

**Rollback Logic**:
```c
file_metadata_t old_meta;
memcpy(&old_meta, &file_entry->metadata, sizeof(file_metadata_t));

// ... update in-memory ACL ...

// Send to SS
if (send_packet(...) < 0 || ss_response.status != STATUS_OK) {
    // Rollback
    memcpy(&file_entry->metadata, &old_meta, sizeof(file_metadata_t));
    // Return error to client
}
```

### `handle_remaccess()`
**New Behavior**: Same pattern as `handle_addaccess()`.

**Steps**:
1. Validate ownership (unchanged)
2. **NEW**: Snapshot metadata
3. Remove ACL entry from in-memory arrays (unchanged)
4. **NEW**: Serialize updated ACL
5. **NEW**: Send CMD_UPDATE_ACL to SS
6. **NEW**: On failure → rollback, on success → reply to client

## Developer Guidelines

### When Adding New ACL Operations
If you implement a new command that modifies ACL (e.g., CMD_TRANSFER_OWNERSHIP):
1. **Snapshot** metadata before changes
2. Modify in-memory ACL
3. **Serialize** updated ACL
4. **Send** CMD_UPDATE_ACL to SS
5. **Wait** for SS response
6. **Rollback** on failure
7. **Respond** to client only after SS confirms

### Error Handling Best Practices
- Always snapshot metadata before modifying ACL
- Always check `send_packet()` return value
- Always check `ss_response.status`
- Roll back in-memory changes on any error
- Log errors for debugging (use LOG_ERROR_MSG)

### Testing New ACL Features
1. Test normal flow (ACL change persists to .meta)
2. Test rollback (stop SS, verify ACL not changed)
3. Test restart (verify .meta survives NM restart)
4. Test concurrent modifications (future phase - locking)

## Common Pitfalls

### Pitfall 1: Forgetting to Snapshot
**Bad**:
```c
// Modify ACL
file_entry->metadata.access_count++;
// Send to SS
if (send_fails) {
    // Can't rollback! No snapshot.
}
```

**Good**:
```c
file_metadata_t old_meta;
memcpy(&old_meta, &file_entry->metadata, sizeof(file_metadata_t));
// Modify ACL
file_entry->metadata.access_count++;
// Send to SS
if (send_fails) {
    memcpy(&file_entry->metadata, &old_meta, sizeof(file_metadata_t));
}
```

### Pitfall 2: Not Checking SS Response
**Bad**:
```c
send_packet(ss_fd, &ss_request);
// Assume success
send_response(client_fd, &ok_response);
```

**Good**:
```c
if (send_packet(ss_fd, &ss_request) < 0) {
    // Rollback and error
}
response_packet_t ss_response;
if (recv_packet(ss_fd, &ss_response) <= 0 || ss_response.status != STATUS_OK) {
    // Rollback and error
}
// Only now send OK to client
```

### Pitfall 3: Inconsistent Serialization
The serialization format is: `"user:PERM,user:PERM,..."`
- Always use `:` as separator between user and permission
- Always use `,` as separator between entries
- Permissions: `RW`, `R`, or `-` (not `W` alone; write implies read)

## Debugging Tips

### Check .meta File
Always inspect the actual .meta file on disk:
```bash
cat ss_storage/filename.txt.meta
```

Look for:
- `access_count=N` line
- `access_0=user:PERM` lines (one per ACL entry)

### Enable Verbose Logging
Logs are in `logs/` directory:
```bash
tail -f logs/name_server.log
tail -f logs/storage_server.log
```

Look for:
- `Handling UPDATE_ACL request`
- `Updated ACL for file ... successfully`
- Errors: `Failed to send UPDATE_ACL`, `SS failed to persist ACL`

### Simulate SS Failure
To test rollback:
1. Stop SS process
2. Try ACL operation from client
3. Check NM log for rollback message
4. Restart SS
5. Verify .meta unchanged

## Future Enhancements (Post Phase 4.5)

### Phase 5
- Use ACL for READ/WRITE permission checks
- Implement sentence-level locking for concurrent writes

### Phase 6+ (Potential)
- Add CMD_GET_ACL to allow NM to query SS for current ACL
- Rebuild NM ACL cache from SS .meta files on restart
- Add ACL versioning or timestamps to detect staleness
- Implement async ACL updates (background thread) to reduce latency
- Add mutex locking around file_entry metadata modifications

## Migration Checklist for Existing Code

If you have custom code that modifies ACL:
- [ ] Snapshot metadata before changes
- [ ] Serialize ACL after in-memory update
- [ ] Send CMD_UPDATE_ACL to SS
- [ ] Check SS response
- [ ] Rollback on failure
- [ ] Log operations for debugging
- [ ] Update tests to verify .meta file changes

## Questions?

See full documentation in:
- `docs/Phase4_5_ACL_Persistence.md` - Architecture and design
- `docs/Phase4_5_Usage_Example.md` - Testing examples
- `PHASE4_5_QUICK_REFERENCE.md` - Quick command reference
