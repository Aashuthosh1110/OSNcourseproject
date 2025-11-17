# Phase 4.5: ACL Persistence Implementation

## Overview
Phase 4.5 implements ACL persistence by making the Storage Server's `.meta` file the **single source of truth** for access control lists. This ensures that ACL changes made by the Name Server are persisted to disk and survive restarts.

## Architecture

### Key Changes

1. **Protocol Extension**
   - Added `CMD_UPDATE_ACL` command to protocol.h
   - Added `CMD_GET_ACL` command for future use
   - Format: NM sends `CMD_UPDATE_ACL` with args: `<filename> <serialized_acl>`

2. **Storage Server (storage_server.c)**
   - **Serialization**: `serialize_acl_from_meta()` converts ACL arrays to compact string format
     - Format: `"user1:RW,user2:R,user3:RW"`
     - Permissions: `RW` (read+write), `R` (read-only), `-` (none)
   - **Parsing**: `parse_acl_into_meta()` parses serialized ACL string into metadata struct
   - **Persistence Handler**: `handle_update_acl_request()` 
     - Parses request args (filename and new ACL)
     - Reads existing .meta to preserve non-ACL fields (owner, timestamps, size, etc.)
     - Rewrites .meta file with updated ACL entries
     - Sends STATUS_OK or error back to Name Server

3. **Name Server (name_server.c)**
   - **ADDACCESS handler update**: After modifying in-memory ACL:
     1. Snapshots old metadata (for rollback)
     2. Serializes the new ACL
     3. Sends CMD_UPDATE_ACL request to Storage Server
     4. Waits for SS acknowledgment
     5. On success: reply to client
     6. On failure: rollback in-memory ACL, return error to client
   - **REMACCESS handler update**: Same pattern as ADDACCESS
     1. Snapshot metadata before ACL removal
     2. Remove ACL entry in memory
     3. Serialize and send to SS
     4. Rollback on failure

## Data Flow

### ADDACCESS Flow
```
Client -> NM: CMD_ADDACCESS -R filename targetuser
NM: Validates ownership and updates in-memory ACL
NM -> SS: CMD_UPDATE_ACL "filename alice:RW,bob:R"
SS: Reads existing .meta, rewrites with new ACL
SS -> NM: STATUS_OK
NM -> Client: "Access granted successfully"
```

### Error Handling & Rollback
```
Client -> NM: CMD_ADDACCESS -W filename targetuser
NM: Updates in-memory ACL, snapshots old state
NM -> SS: CMD_UPDATE_ACL "filename ..."
SS: Disk full / permission error
SS -> NM: STATUS_ERROR_INTERNAL "Failed to write metadata"
NM: Rollback in-memory ACL to snapshot
NM -> Client: STATUS_ERROR_INTERNAL "Failed to communicate with storage server"
```

## .meta File Format

The Storage Server writes ACL entries in this format:
```
owner=alice
created=1700000000
modified=1700001234
accessed=1700001234
accessed_by=alice
size=0
word_count=0
char_count=0
access_count=2
access_0=alice:RW
access_1=bob:R
```

## Testing

### Manual Test Steps

1. **Start servers**:
   ```bash
   ./bin/name_server 8080 &
   ./bin/storage_server 127.0.0.1 8080 ss_storage 9001 &
   ```

2. **Connect client (user alice)**:
   ```bash
   ./bin/client alice 127.0.0.1 8080
   ```

3. **Create a file**:
   ```
   docs++ > CREATE testfile.txt
   # Expected: File created successfully
   ```

4. **Check initial .meta on disk**:
   ```bash
   cat ss_storage/testfile.txt.meta
   # Should show: access_0=alice:RW
   ```

5. **Grant access to bob**:
   ```
   docs++ > ADDACCESS -R testfile.txt bob
   # Expected: Access granted successfully
   ```

6. **Verify .meta updated**:
   ```bash
   cat ss_storage/testfile.txt.meta
   # Should now show:
   #   access_count=2
   #   access_0=alice:RW
   #   access_1=bob:R
   ```

7. **Revoke bob's access**:
   ```
   docs++ > REMACCESS testfile.txt bob
   # Expected: Access revoked successfully
   ```

8. **Verify .meta updated again**:
   ```bash
   cat ss_storage/testfile.txt.meta
   # Should show: access_count=1, access_0=alice:RW only
   ```

9. **Test rollback on SS unavailability**:
   - Stop Storage Server (kill SS process)
   - Try `ADDACCESS -R testfile.txt charlie`
   - Expected: Error message from NM (network failure or timeout)
   - Restart SS and check .meta is unchanged

10. **Test Name Server restart persistence**:
    - Grant access to multiple users
    - Check .meta file on disk (multiple access_X lines)
    - Restart Name Server
    - Verify SS still has correct ACL in .meta (SS is source of truth)

### Automated Test Script
Run `./test_phase4_5.sh` (see below).

## Benefits

1. **Data Integrity**: ACL changes persist across server restarts
2. **Single Source of Truth**: SS .meta file is authoritative; NM holds cache only
3. **Rollback Safety**: NM rolls back in-memory changes if SS persistence fails
4. **Consistency**: Synchronous update ensures client never sees inconsistent state
5. **Error Recovery**: Failed ACL writes don't corrupt NM state or file access

## Known Limitations

- **Synchronous**: NM blocks on SS response (may add latency)
- **No Concurrency Lock**: Concurrent ACL updates to same file not handled (Phase 6+)
- **Network Dependency**: If SS is down, ACL changes fail (as designed; safety over availability)

## Next Steps (Phase 5)

- Implement READ/WRITE operations using ACL checks
- Add sentence-level locking for concurrent writes
- Stream file content to clients
- Implement EXEC command for file execution
- Add UNDO functionality

## Files Modified

- `include/protocol.h`: Added CMD_UPDATE_ACL, CMD_GET_ACL
- `src/storage_server/storage_server.c`: Added serialize/parse helpers and handle_update_acl_request
- `src/name_server/name_server.c`: Modified handle_addaccess and handle_remaccess to persist ACL changes to SS

## Summary

Phase 4.5 closes the ACL persistence gap. All ACL modifications now trigger a synchronous write to the Storage Server's `.meta` file. The Name Server snapshots its state before making changes and rolls back if the Storage Server fails to persist. This ensures that the `.meta` file remains the authoritative source for access control, allowing the system to recover correctly after restarts.
