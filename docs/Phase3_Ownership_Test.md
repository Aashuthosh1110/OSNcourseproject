# Phase 3: Ownership Validation Testing Guide

## Overview
This guide helps you verify that ownership checking has been successfully moved from the Name Server to the Storage Server. The Storage Server now reads `.meta` files to validate ownership before allowing DELETE operations.

## Architecture Changes Made

### Before (Incorrect)
- **Name Server**: Checked ownership against in-memory registry
- **Storage Server**: No ownership validation, just deleted files
- **Problem**: Name Server shouldn't enforce access control; it's just a router

### After (Correct) ✅
- **Name Server**: Routes DELETE requests without checking ownership
- **Storage Server**: Reads `.meta` file from disk, validates owner matches requester
- **Benefit**: Source of truth (disk metadata) enforced at correct layer

## Testing Procedure

### Step 1: Start the Servers

**Terminal 1 - Name Server:**
```bash
cd /home/aashuthosh-s-sharma/Desktop/OSN/courseproject/course-project-alphaq/course-project-alphaq
./bin/name_server
```

**Terminal 2 - Storage Server:**
```bash
cd /home/aashuthosh-s-sharma/Desktop/OSN/courseproject/course-project-alphaq/course-project-alphaq
./bin/storage_server localhost 8000 ./test_storage/ss1
```

### Step 2: Create File as First User

**Terminal 3 - Client 1 (Owner):**
```bash
cd /home/aashuthosh-s-sharma/Desktop/OSN/courseproject/course-project-alphaq/course-project-alphaq
./bin/client
```

When prompted for username, enter: `aashuthosh`

Then execute:
```
CREATE testfile.txt
```

**Expected Output:**
```
File created successfully
```

**Verify metadata file created:**
```bash
cat test_storage/ss1/testfile.txt.meta
```

Should show:
```
owner=aashuthosh
created=<timestamp>
modified=<timestamp>
access_control=owner
```

### Step 3: Attempt DELETE as Different User

**Terminal 4 - Client 2 (Non-Owner):**
```bash
cd /home/aashuthosh-s-sharma/Desktop/OSN/courseproject/course-project-alphaq/course-project-alphaq
./bin/client
```

When prompted for username, enter: `notaashu`

Then execute:
```
DELETE testfile.txt
```

**Expected Output (Client):**
```
Error: Only the owner can delete this file
```

**Expected Log (Storage Server - Terminal 2):**
```
[WARNING] [STORAGE_SERVER] User 'notaashu' attempted to delete file owned by 'aashuthosh'
```

**Critical Check:** Verify the ownership warning appears in **Storage Server logs**, NOT Name Server logs!

### Step 4: DELETE as Owner (Should Succeed)

Switch back to **Terminal 3 (Client 1 - aashuthosh)**:
```
DELETE testfile.txt
```

**Expected Output:**
```
File deleted successfully
```

**Verify file removed:**
```bash
ls test_storage/ss1/testfile.txt
# Should output: No such file or directory
```

## Verification Checklist

Use this checklist to confirm correct implementation:

- [ ] **Storage Server reads `.meta` file** - Check logs show "Ownership verified" message
- [ ] **Ownership check happens on Storage Server** - Warning log appears in SS terminal, NOT NM terminal
- [ ] **Non-owner DELETE fails** - Error message: "Only the owner can delete this file"
- [ ] **Owner DELETE succeeds** - File and metadata removed from disk
- [ ] **Name Server forwards all DELETEs** - No ownership filtering at NM level
- [ ] **Metadata is source of truth** - Even if NM registry differs, SS uses disk metadata

## Testing Edge Cases

### Test Case 1: File Without Metadata
```bash
# Manually create file without .meta
touch test_storage/ss1/orphan.txt

# Try to delete as any user
DELETE orphan.txt
```

**Expected:** Should succeed (no metadata = no ownership enforcement)

### Test Case 2: Corrupted Metadata
```bash
# Create file normally
CREATE corrupted.txt

# Manually corrupt metadata
echo "garbage data" > test_storage/ss1/corrupted.txt.meta

# Try to delete
DELETE corrupted.txt
```

**Expected:** Should succeed (unparseable metadata = no enforcement)

### Test Case 3: Multiple Files, Different Owners
```bash
# Client 1 (aashuthosh)
CREATE file1.txt
CREATE file2.txt

# Client 2 (notaashu)
DELETE file1.txt  # Should fail
DELETE file2.txt  # Should fail

# Client 1 (aashuthosh)
DELETE file1.txt  # Should succeed
DELETE file2.txt  # Should succeed
```

## Log Analysis

### Name Server Log (Correct Behavior)
```
[INFO] [NAME_SERVER] Received DELETE request from client
[INFO] [NAME_SERVER] File 'testfile.txt' found in registry
[INFO] [NAME_SERVER] Forwarding DELETE to storage server
[INFO] [NAME_SERVER] File 'testfile.txt' deleted successfully by user 'aashuthosh'
```

**Notice:** No ownership warnings! Name Server just routes.

### Storage Server Log (Correct Behavior)
```
[INFO] [STORAGE_SERVER] Handling DELETE request for file: testfile.txt by user: notaashu
[WARNING] [STORAGE_SERVER] User 'notaashu' attempted to delete file owned by 'aashuthosh'
```

OR (for successful delete):

```
[INFO] [STORAGE_SERVER] Handling DELETE request for file: testfile.txt by user: aashuthosh
[INFO] [STORAGE_SERVER] Ownership verified: user 'aashuthosh' owns file 'testfile.txt'
[INFO] [STORAGE_SERVER] Deleted file: ./test_storage/ss1/testfile.txt
[INFO] [STORAGE_SERVER] Deleted metadata file: ./test_storage/ss1/testfile.txt.meta
```

## Common Issues and Solutions

### Issue: Ownership check still happening on Name Server
**Symptom:** Log message "User 'X' attempted to delete file owned by 'Y'" appears in Name Server terminal

**Solution:** Recompile with `make clean && make` to ensure changes applied

### Issue: Storage Server not reading metadata
**Symptom:** Non-owner DELETE succeeds when it shouldn't

**Solution:** 
1. Check metadata file exists: `ls test_storage/ss1/*.meta`
2. Verify metadata format: `cat test_storage/ss1/testfile.txt.meta`
3. Ensure owner field is populated correctly

### Issue: All DELETEs failing
**Symptom:** Even owner cannot delete files

**Solution:**
1. Check Storage Server logs for error messages
2. Verify file permissions: `ls -la test_storage/ss1/`
3. Ensure username matches exactly (case-sensitive)

## Success Criteria

Your implementation is correct when:

1. ✅ Ownership warnings appear in **Storage Server logs only**
2. ✅ Name Server logs show simple routing (no access control logic)
3. ✅ Non-owner DELETE operations fail with "Only the owner can delete this file"
4. ✅ Owner DELETE operations succeed
5. ✅ Storage Server reads `.meta` files before deletion
6. ✅ Metadata on disk is the authoritative source for ownership

## Next Steps

Once ownership validation is working correctly:
1. Implement Phase 4 commands (VIEW, READ, WRITE)
2. Add more granular access control (read vs. write permissions)
3. Implement file locking for concurrent access
4. Add backup/redundancy features

## Architecture Notes

**Why this matters:**
- **Separation of Concerns**: Name Server = routing/coordination, Storage Server = data + access control
- **Scalability**: Multiple storage servers can independently enforce their own access control
- **Security**: Access control at data layer prevents bypass attempts
- **Consistency**: Metadata on disk is single source of truth, no sync issues between NM and SS

This architecture mirrors real distributed systems like HDFS, where:
- **NameNode** (our Name Server) = metadata + routing
- **DataNode** (our Storage Server) = data storage + block-level operations
