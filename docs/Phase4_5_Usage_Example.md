# Phase 4.5 Usage Example

This document provides step-by-step commands to test the Phase 4.5 ACL persistence feature.

## Setup

### Terminal 1: Start Name Server
```bash
cd /home/aashuthosh-s-sharma/Desktop/OSN/courseproject/course-project-alphaq
./bin/name_server 8080
```

### Terminal 2: Start Storage Server
```bash
cd /home/aashuthosh-s-sharma/Desktop/OSN/courseproject/course-project-alphaq
mkdir -p ss_storage
./bin/storage_server 127.0.0.1 8080 ss_storage 9001
```

### Terminal 3: Start Client (as alice)
```bash
cd /home/aashuthosh-s-sharma/Desktop/OSN/courseproject/course-project-alphaq
./bin/client alice 127.0.0.1 8080
```

## Test Scenario 1: Basic ACL Persistence

### Step 1: Create a file
```
docs++ > CREATE myfile.txt
```
Expected output: `File created successfully`

### Step 2: Check initial .meta file
Open Terminal 4:
```bash
cat ss_storage/myfile.txt.meta
```

Expected output (example):
```
owner=alice
created=1731878400
modified=1731878400
accessed=1731878400
accessed_by=alice
size=0
word_count=0
char_count=0
access_count=1
access_0=alice:RW
```

✅ **Verify**: `access_0=alice:RW` (owner has read-write permission)

### Step 3: Grant read access to bob
```
docs++ > ADDACCESS -R myfile.txt bob
```
Expected output: `Access granted successfully`

### Step 4: Verify .meta file updated
In Terminal 4:
```bash
cat ss_storage/myfile.txt.meta
```

Expected output should now include:
```
access_count=2
access_0=alice:RW
access_1=bob:R
```

✅ **Verify**: ACL persisted to disk with bob having read permission

### Step 5: Grant write access to charlie
```
docs++ > ADDACCESS -W myfile.txt charlie
```
Expected output: `Access granted successfully`

### Step 6: Check .meta again
```bash
cat ss_storage/myfile.txt.meta
```

Expected output:
```
access_count=3
access_0=alice:RW
access_1=bob:R
access_2=charlie:RW
```

✅ **Verify**: Charlie has RW permission (write implies read)

### Step 7: View file info
```
docs++ > INFO myfile.txt
```

Expected output should show:
```
File Information:
  Name: myfile.txt
  Owner: alice
  ...
  Access Control:
    alice: RW
    bob: R
    charlie: RW
```

### Step 8: Remove bob's access
```
docs++ > REMACCESS myfile.txt bob
```
Expected output: `Access revoked successfully`

### Step 9: Verify removal persisted
```bash
cat ss_storage/myfile.txt.meta
```

Expected output:
```
access_count=2
access_0=alice:RW
access_1=charlie:RW
```

✅ **Verify**: Bob's entry removed and ACL compacted

## Test Scenario 2: Persistence Across Restart

### Step 1: Note current ACL
```bash
cat ss_storage/myfile.txt.meta
# Remember the current access_count and entries
```

### Step 2: Restart Name Server
In Terminal 1 (Name Server), press Ctrl+C, then restart:
```bash
./bin/name_server 8080
```

### Step 3: Reconnect client
In Terminal 3, exit and reconnect:
```
docs++ > EXIT
```
```bash
./bin/client alice 127.0.0.1 8080
```

### Step 4: Check file info
```
docs++ > INFO myfile.txt
```

✅ **Expected**: INFO may show outdated ACL (NM lost state), BUT the .meta file on SS still has correct ACL

### Step 5: Verify .meta unchanged
```bash
cat ss_storage/myfile.txt.meta
```

✅ **Verify**: Storage Server `.meta` file retained the ACL (it's the source of truth)

## Test Scenario 3: Rollback on Failure

### Step 1: Stop Storage Server
In Terminal 2 (Storage Server), press Ctrl+C to stop it.

### Step 2: Try to add access
In Terminal 3 (Client):
```
docs++ > ADDACCESS -R myfile.txt dave
```

Expected output: `Failed to communicate with storage server` (or similar error)

### Step 3: Restart Storage Server
In Terminal 2:
```bash
./bin/storage_server 127.0.0.1 8080 ss_storage 9001
```

### Step 4: Check .meta file unchanged
```bash
cat ss_storage/myfile.txt.meta
```

✅ **Verify**: No `dave` entry (rollback succeeded; NM did not persist failed change)

### Step 5: Retry add access (SS now running)
```
docs++ > ADDACCESS -R myfile.txt dave
```

Expected output: `Access granted successfully`

### Step 6: Verify dave added
```bash
cat ss_storage/myfile.txt.meta
```

✅ **Verify**: Dave's entry now present in ACL

## Test Scenario 4: Multiple Users

Start additional clients in new terminals:

### Terminal 5: Client as bob
```bash
./bin/client bob 127.0.0.1 8080
```

### Terminal 6: Client as charlie
```bash
./bin/client charlie 127.0.0.1 8080
```

### As alice: Grant access to both
```
docs++ > ADDACCESS -R myfile.txt bob
docs++ > ADDACCESS -W myfile.txt charlie
```

### As bob: View files
```
docs++ > VIEW
```
✅ **Expected**: Should see `myfile.txt` in list (bob has read access)

### As charlie: View files
```
docs++ > VIEW
```
✅ **Expected**: Should see `myfile.txt` (charlie has write access)

### Verify .meta has all entries
```bash
cat ss_storage/myfile.txt.meta
```

Expected:
```
access_count=3
access_0=alice:RW
access_1=bob:R
access_2=charlie:RW
```

## Expected Behavior Summary

| Action | NM Behavior | SS Behavior | Result |
|--------|-------------|-------------|--------|
| ADDACCESS | Update memory → Send CMD_UPDATE_ACL | Rewrite .meta → Reply OK | ACL persisted |
| REMACCESS | Update memory → Send CMD_UPDATE_ACL | Rewrite .meta → Reply OK | ACL persisted |
| SS Down + ADDACCESS | Update memory → Send fails → Rollback | N/A | Error to client, no change |
| NM Restart | Loses in-memory state | Retains .meta | SS is source of truth |

## Troubleshooting

### Issue: "Failed to communicate with storage server"
- **Cause**: Storage Server not running or network issue
- **Fix**: Ensure Storage Server is running on correct port (9001)

### Issue: .meta file not updating
- **Cause**: SS may have failed to write (permissions, disk full)
- **Check**: SS logs for errors
- **Fix**: Check SS storage directory permissions

### Issue: ACL shows in INFO but not persisted
- **Cause**: Implementation bug or SS write failure
- **Check**: Compare `INFO` output with actual .meta file content
- **Fix**: Restart SS and re-test; check logs

## Clean Up

After testing:
```bash
# Stop all servers (Ctrl+C in each terminal)
# Remove test storage
rm -rf ss_storage
```

## Quick Verification Script

Or use the automated test:
```bash
./test_phase4_5.sh
```

This script will guide you through verification steps interactively.
