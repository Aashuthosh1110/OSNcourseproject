# Phase 4.5 Quick Reference

## What Changed in Phase 4.5?

**Goal**: Make Storage Server `.meta` file the **authoritative source** for Access Control Lists (ACLs).

Previously, the Name Server would update ACLs in memory only. Now, all ACL changes are persisted to the Storage Server's `.meta` file synchronously.

## New Protocol Commands

- `CMD_UPDATE_ACL` - Name Server → Storage Server
  - Args format: `"<filename> <acl_string>"`
  - ACL string format: `"user1:RW,user2:R,user3:RW"`
  - Permissions: `RW` (read+write), `R` (read), `-` (none)

- `CMD_GET_ACL` - Reserved for future use

## How It Works

### ADDACCESS Flow
1. Client sends `ADDACCESS -R filename user`
2. Name Server validates ownership
3. NM updates in-memory ACL (takes snapshot first)
4. NM serializes ACL and sends `CMD_UPDATE_ACL` to Storage Server
5. Storage Server reads `.meta`, rewrites with new ACL, replies STATUS_OK
6. NM responds to client
7. **On SS failure**: NM rolls back in-memory ACL, returns error to client

### REMACCESS Flow
Same pattern: snapshot → update memory → persist to SS → rollback on failure

## Testing

1. **Start servers**:
   ```bash
   ./bin/name_server 8080 &
   ./bin/storage_server 127.0.0.1 8080 ss_storage 9001 &
   ./bin/client alice 127.0.0.1 8080
   ```

2. **Quick verification**:
   ```bash
   # In client:
   CREATE test.txt
   ADDACCESS -R test.txt bob
   
   # Check on disk:
   cat ss_storage/test.txt.meta
   # Should show: access_0=alice:RW, access_1=bob:R
   ```

3. **Run automated test** (interactive):
   ```bash
   ./test_phase4_5.sh
   ```

## Files Modified

- `include/protocol.h` - Added CMD_UPDATE_ACL, CMD_GET_ACL
- `src/storage_server/storage_server.c` - Added serialization, parsing, and handler
- `src/name_server/name_server.c` - Updated ADDACCESS/REMACCESS to persist changes

## Benefits

✅ **Persistence**: ACL changes survive restarts  
✅ **Consistency**: SS `.meta` is single source of truth  
✅ **Safety**: NM rolls back on SS failure  
✅ **Integrity**: Synchronous updates prevent inconsistency  

## Next: Phase 5

Implement READ, WRITE, STREAM, EXEC, UNDO operations using the persisted ACLs.

See `docs/Phase4_5_ACL_Persistence.md` for full documentation.
