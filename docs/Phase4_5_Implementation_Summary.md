# Phase 4.5 Implementation Summary

## Objective
Implement ACL persistence so that the Storage Server's `.meta` file becomes the single source of truth for access control lists. This ensures that ACL changes made through the Name Server are persisted to disk and survive server restarts.

## Changes Made

### 1. Protocol Extension (`include/protocol.h`)

Added two new commands to `command_t` enum:
- `CMD_UPDATE_ACL` - Used by Name Server to instruct Storage Server to update ACL in `.meta` file
- `CMD_GET_ACL` - Reserved for future use (Phase 5+)

### 2. Storage Server Changes (`src/storage_server/storage_server.c`)

**New Functions**:

1. **`serialize_acl_from_meta(const file_metadata_t* meta)`**
   - Converts ACL arrays from metadata struct into compact string format
   - Format: `"user1:RW,user2:R,user3:RW"`
   - Permissions: `RW` (read+write), `R` (read-only), `-` (none)
   - Returns heap-allocated string (caller must free)

2. **`parse_acl_into_meta(file_metadata_t* meta, const char* acl_str)`**
   - Parses serialized ACL string back into metadata struct
   - Resets existing ACL and populates from string
   - Handles permission flags: R (read), W (write), RW (both), - (none)
   - Write permission implies read permission

3. **`handle_update_acl_request(request_packet_t* req)`**
   - Handler for `CMD_UPDATE_ACL` command from Name Server
   - Parses args: `"<filename> <acl_string>"`
   - Reads existing `.meta` file to preserve non-ACL fields (owner, timestamps, size, etc.)
   - Parses new ACL from request
   - Rewrites `.meta` file with updated ACL entries
   - Returns `STATUS_OK` on success or error status on failure

**Modified Functions**:
- Updated `handle_nm_commands()` switch statement to handle `CMD_UPDATE_ACL`
- Added function prototypes at top of file

### 3. Name Server Changes (`src/name_server/name_server.c`)

**Modified `handle_addaccess()`**:
1. Takes snapshot of metadata before making changes (for rollback)
2. Updates in-memory ACL as before
3. Serializes the updated ACL into compact string format
4. Builds `CMD_UPDATE_ACL` request packet with filename and serialized ACL
5. Sends request to Storage Server socket
6. Waits for acknowledgment from Storage Server
7. **On success**: Sends success response to client
8. **On failure**: Rolls back in-memory ACL to snapshot, returns error to client

**Modified `handle_remaccess()`**:
1. Takes snapshot of metadata before making changes
2. Removes ACL entry from in-memory arrays
3. Serializes the updated ACL (with entry removed)
4. Sends `CMD_UPDATE_ACL` to Storage Server
5. Waits for acknowledgment
6. **On success**: Sends success response to client
7. **On failure**: Rolls back in-memory ACL to snapshot, returns error to client

## Architecture & Flow

### Before Phase 4.5
```
Client → NM: ADDACCESS
NM: Updates in-memory ACL
NM → Client: OK
[ACL change NOT persisted to disk]
```

### After Phase 4.5
```
Client → NM: ADDACCESS
NM: Snapshot metadata, update in-memory ACL
NM → SS: CMD_UPDATE_ACL "filename user1:RW,user2:R"
SS: Read .meta, rewrite with new ACL
SS → NM: STATUS_OK
NM → Client: OK

[On SS failure:]
NM → SS: CMD_UPDATE_ACL
SS → NM: STATUS_ERROR (or timeout)
NM: Rollback in-memory ACL to snapshot
NM → Client: ERROR
```

## Key Benefits

1. **Persistence**: ACL changes survive Name Server and Storage Server restarts
2. **Single Source of Truth**: Storage Server `.meta` file is authoritative
3. **Consistency**: Synchronous updates ensure client never sees inconsistent state
4. **Safety**: Rollback on failure prevents NM and SS from diverging
5. **Error Recovery**: Failed writes don't corrupt Name Server state

## Testing Strategy

### Manual Testing
1. Create a file with one user
2. Check `.meta` file shows initial ACL (owner:RW)
3. Grant access to another user
4. Verify `.meta` file updated with new entry
5. Revoke access
6. Verify `.meta` file updated (entry removed)
7. Restart Name Server - SS still has correct ACL in `.meta`

### Error Testing
1. Stop Storage Server
2. Try to modify ACL (ADDACCESS/REMACCESS)
3. Verify client receives error
4. Verify Name Server rolled back in-memory change
5. Restart SS and check `.meta` unchanged

### Automated Test
- Created `test_phase4_5.sh` - Interactive test script that walks through manual verification steps

## Files Created/Modified

### Modified:
- `include/protocol.h` - Added CMD_UPDATE_ACL, CMD_GET_ACL
- `src/storage_server/storage_server.c` - Added 3 new functions, updated command handler
- `src/name_server/name_server.c` - Updated handle_addaccess() and handle_remaccess()

### Created:
- `docs/Phase4_5_ACL_Persistence.md` - Full documentation
- `PHASE4_5_QUICK_REFERENCE.md` - Quick reference guide
- `test_phase4_5.sh` - Interactive test script

## Compilation Status

✅ Code compiles successfully with warnings only (no errors)
- Warnings: snprintf truncation (informational), unused parameters, unused variables
- All three binaries built: `bin/name_server`, `bin/storage_server`, `bin/client`

## Known Limitations

1. **Synchronous blocking**: Name Server blocks waiting for Storage Server response (may add latency)
2. **No concurrency handling**: Concurrent ACL modifications to same file not handled (Phase 6+)
3. **Network dependency**: If SS is unavailable, ACL changes fail (by design - safety over availability)
4. **Snapshot overhead**: Full metadata snapshot on every ACL change (small overhead but safe)

## Next Steps (Phase 5)

With ACL persistence complete, Phase 5 can implement:
- READ command (check ACL for read permission)
- WRITE command (check ACL for write permission, implement sentence-level locking)
- STREAM command (check ACL, stream file content)
- EXEC command (execute file)
- UNDO functionality

These operations will rely on the persisted ACLs in the Storage Server's `.meta` files.

## Summary

Phase 4.5 successfully implements ACL persistence by:
1. Extending the protocol with CMD_UPDATE_ACL
2. Adding serialization/parsing helpers on Storage Server
3. Modifying Name Server to synchronously persist ACL changes to Storage Server
4. Implementing rollback safety to prevent inconsistency
5. Making Storage Server `.meta` file the authoritative source for ACLs

The system now maintains ACL consistency across restarts and handles failures gracefully by rolling back uncommitted changes.
