# Phase 5 Testing Guide

Complete guide for testing all Phase 5 operations: READ, STREAM, WRITE, ETIRW, UNDO, and EXEC.

---

## Prerequisites

### 1. Compile the System
```bash
cd course-project-alphaq
make clean && make
```

### 2. Setup Storage Directories
```bash
# Create storage directories
mkdir -p ss_storage1
mkdir -p ss_storage2

# Clean any old files
rm -f ss_storage1/* ss_storage2/*
```

### 3. Start the Servers

**Terminal 1 - Name Server:**
```bash
./bin/name_server 8080
```

**Terminal 2 - Storage Server 1:**
```bash
./bin/storage_server 127.0.0.1 8080 ss_storage1 9001
```

**Terminal 3 - Storage Server 2 (optional for load testing):**
```bash
./bin/storage_server 127.0.0.1 8080 ss_storage2 9002
```

---

## Test 1: READ Operation

### Setup
**Terminal 4 - Client (alice):**
```bash
./bin/client alice 127.0.0.1 8080
```

### Test Steps

**1. Create a test file:**
```
CREATE read_test.txt
```

**2. Add content to the file manually:**
```bash
# In another terminal:
echo "This is a test file for READ operation.
It has multiple lines.
Line three here." > ss_storage1/read_test.txt
```

**3. Test READ (should work - alice is owner):**
```
READ read_test.txt
```

**Expected Output:**
```
--- File Content: read_test.txt ---
This is a test file for READ operation.
It has multiple lines.
Line three here.
--- End of File ---
```

**4. Test READ with another user (bob):**

Open new client terminal:
```bash
./bin/client bob 127.0.0.1 8080
```

Try to read (should fail - no permission):
```
READ read_test.txt
```

**Expected:** `Error: Permission denied: You do not have read access to this file`

**5. Grant bob read access (from alice client):**
```
ADDACCESS -R read_test.txt bob
```

**6. Bob tries again (should succeed now):**
```
READ read_test.txt
```

**Expected:** File content displays successfully

---

## Test 2: STREAM Operation

### Test Steps

**1. Create a file with multiple words:**
```bash
# In terminal:
echo "Hello World from the streaming test system" > ss_storage1/stream_test.txt
```

**2. Create metadata (from alice client):**
```
CREATE stream_test.txt
```

**3. Test STREAM (alice):**
```
STREAM stream_test.txt
```

**Expected Output:**
```
--- Streaming: stream_test.txt ---
Hello 
World 
from 
the 
streaming 
test 
system 
--- Stream Complete ---
```

**Note:** Each word should appear with a ~0.1 second delay

**4. Test STREAM with bob (no permission):**
```
STREAM stream_test.txt
```

**Expected:** `Error: Permission denied`

**5. Grant access and retry:**
```
# From alice:
ADDACCESS -R stream_test.txt bob

# From bob:
STREAM stream_test.txt
```

**Expected:** Words stream with delays

---

## Test 3: WRITE Operation (Sentence Locking)

### Test Steps

**1. Create a file for writing:**
```bash
echo "Word0 Word1 Word2 Word3 Word4" > ss_storage1/write_test.txt
```

**2. Create metadata (alice):**
```
CREATE write_test.txt
```

**3. Start WRITE session (alice):**
```
WRITE write_test.txt 0
```

**Expected Output:**
```
Lock acquired for sentence 0 of 'write_test.txt'
Enter word updates in format: <word_index> <content>
Type 'ETIRW' when done to save changes

WRITE>
```

**4. Update words:**
```
WRITE> 0 Hello
Word 0 updated

WRITE> 1 Beautiful
Word 1 updated

WRITE> 2 World
Word 2 updated
```

**5. Try to acquire same lock from bob (should fail):**

In bob's terminal:
```
WRITE write_test.txt 0
```

**Expected:** `Error: Sentence 0 is locked by another user`

**6. Complete WRITE session (alice):**
```
WRITE> ETIRW
Changes saved successfully
```

**7. Verify changes:**
```bash
# In terminal:
cat ss_storage1/write_test.txt
```

**Expected:** File should contain the updates

**8. Verify backup exists:**
```bash
ls -la ss_storage1/write_test.txt.bak
```

**Expected:** Backup file exists (for UNDO)

---

## Test 4: ETIRW (Save Changes)

### Test Steps

**1. Start WRITE session:**
```
WRITE write_test.txt 0
```

**2. Make some changes:**
```
WRITE> 3 Testing
WRITE> 4 ETIRW
```

**3. Save with ETIRW:**
```
WRITE> ETIRW
```

**Expected:**
- File updated on disk
- Backup created (.bak)
- Lock released
- Connection closed

**4. Verify lock released (bob can now write):**
```
# From bob (after granting write access):
WRITE write_test.txt 0
```

**Expected:** Lock acquired successfully

---

## Test 5: UNDO Operation

### Test Steps

**1. Create and modify a file:**
```bash
# Create original
echo "Original content here" > ss_storage1/undo_test.txt
```

**2. Create metadata:**
```
CREATE undo_test.txt
```

**3. Grant write access to bob:**
```
ADDACCESS -W undo_test.txt bob
```

**4. Make changes with WRITE (from bob):**
```
WRITE undo_test.txt 0
WRITE> 0 Modified
WRITE> 1 Content
WRITE> ETIRW
```

**5. Verify changes:**
```
READ undo_test.txt
```

**Expected:** Shows "Modified Content ..."

**6. Perform UNDO (bob):**
```
UNDO undo_test.txt
```

**Expected:** `File 'undo_test.txt' restored from backup`

**7. Verify restoration:**
```
READ undo_test.txt
```

**Expected:** Shows original content "Original content here"

**8. Test UNDO without backup:**
```
UNDO undo_test.txt
```

**Expected:** `Error: No backup found for 'undo_test.txt'`

---

## Test 6: EXEC Operation

### Test Steps

**1. Create a simple shell script:**
```bash
cat > ss_storage1/test_script.sh << 'EOF'
#!/bin/bash
echo "Hello from EXEC!"
echo "Current date: $(date)"
echo "File listing:"
ls -la /tmp | head -5
EOF
```

**2. Create metadata:**
```
CREATE test_script.sh
```

**3. Execute the script (alice):**
```
EXEC test_script.sh
```

**Expected Output:**
```
--- EXEC Output ---
Hello from EXEC!
Current date: Sun Nov 17 ...
File listing:
(first 5 lines of /tmp listing)
```

**4. Test EXEC with bob (no permission):**
```
# From bob:
EXEC test_script.sh
```

**Expected:** `Error: Permission denied: You do not have read access to this file`

**5. Grant access and retry:**
```
# From alice:
ADDACCESS -R test_script.sh bob

# From bob:
EXEC test_script.sh
```

**Expected:** Script output displays

**6. Test script with errors:**
```bash
echo "exit 1" > ss_storage1/error_script.sh
```

```
CREATE error_script.sh
EXEC error_script.sh
```

**Expected:** Output captured, exit code logged in NM

---

## Test 7: Concurrent Operations

### Test Multiple Concurrent Reads

**Terminal 4 - alice:**
```
READ large_file.txt
```

**Terminal 5 - bob:**
```
READ large_file.txt
```

**Terminal 6 - charlie:**
```
READ large_file.txt
```

**Expected:** All three succeed simultaneously

### Test Lock Contention

**Terminal 4 - alice:**
```
WRITE shared_file.txt 0
(don't type ETIRW yet - keep lock held)
```

**Terminal 5 - bob:**
```
WRITE shared_file.txt 0
```

**Expected:** Bob receives "Sentence 0 is locked by alice"

**Terminal 4 - alice:**
```
WRITE> ETIRW
```

**Terminal 5 - bob:**
```
WRITE shared_file.txt 0
```

**Expected:** Bob now acquires lock successfully

### Test Different Sentence Locks

**Terminal 4 - alice:**
```
WRITE multi_sentence.txt 0
```

**Terminal 5 - bob:**
```
WRITE multi_sentence.txt 1
```

**Expected:** Both acquire locks successfully (different sentences)

---

## Test 8: Permission Edge Cases

### Test Write Without Read Permission

**1. Create file (alice):**
```
CREATE perm_test.txt
```

**2. Try to write without any access (bob):**
```
WRITE perm_test.txt 0
```

**Expected:** `Error: Permission denied: You do not have write access to this file`

**3. Grant only read (alice):**
```
ADDACCESS -R perm_test.txt bob
```

**4. Bob tries to write:**
```
WRITE perm_test.txt 0
```

**Expected:** `Error: Permission denied: You do not have write access to this file`

**5. Grant write (alice):**
```
ADDACCESS -W perm_test.txt bob
```

**6. Bob tries to write:**
```
WRITE perm_test.txt 0
```

**Expected:** Lock acquired successfully

---

## Test 9: Error Conditions

### Test Non-Existent File

```
READ nonexistent.txt
```
**Expected:** `Error: File 'nonexistent.txt' not found`

### Test UNDO Without Backup

```
UNDO fresh_file.txt
```
**Expected:** `Error: No backup found for 'fresh_file.txt'`

### Test ETIRW Without WRITE Session

```
# Without starting WRITE first
ETIRW some_file.txt
```
**Expected:** Connection closes or error (client should not allow this)

### Test Storage Server Disconnect

**1. Start WRITE session:**
```
WRITE test.txt 0
```

**2. Kill Storage Server (Ctrl+C in SS terminal)**

**3. Try to update word:**
```
WRITE> 0 Test
```

**Expected:** Network error or timeout

---

## Test 10: Stress Testing

### Create Multiple Concurrent Clients

```bash
# Terminal 1-10: Start 10 clients
for i in {1..10}; do
    gnome-terminal -- bash -c "./bin/client user$i 127.0.0.1 8080; exec bash"
done
```

### Perform Concurrent Operations

From each client:
```
READ test_file.txt    # All should succeed
STREAM test_file.txt  # All should succeed
```

### Test Lock Contention

From all 10 clients simultaneously:
```
WRITE contest_file.txt 0
```

**Expected:** Only one acquires lock, others get "locked" error

---

## Automated Test Script

### Quick Test All Operations

```bash
#!/bin/bash

# Start servers in background
./bin/name_server 8080 > /dev/null 2>&1 &
NM_PID=$!
sleep 1

./bin/storage_server 127.0.0.1 8080 ss_storage1 9001 > /dev/null 2>&1 &
SS_PID=$!
sleep 2

# Test using expect or heredoc
./bin/client alice 127.0.0.1 8080 << EOF
CREATE test.txt
READ test.txt
STREAM test.txt
WRITE test.txt 0
0 Hello
1 World
ETIRW
UNDO test.txt
EXEC test.txt
EXIT
EOF

# Cleanup
kill $NM_PID $SS_PID
```

---

## Verification Checklist

### Phase 5.1: Concurrency ✓
- [ ] Multiple clients connect without blocking
- [ ] Name Server responds while clients active
- [ ] Storage Server handles multiple clients

### Phase 5.2: READ/STREAM ✓
- [ ] READ displays file content
- [ ] STREAM shows word-by-word with delays
- [ ] Permission checks work (owner/ACL)
- [ ] Direct client-SS connection works

### Phase 5.3: WRITE/ETIRW/UNDO ✓
- [ ] WRITE acquires lock successfully
- [ ] Lock prevents concurrent writes (same sentence)
- [ ] Word updates acknowledged
- [ ] ETIRW saves changes and creates backup
- [ ] UNDO restores from backup
- [ ] Lock released after ETIRW

### Phase 5.4: EXEC ✓
- [ ] Script executes on Name Server
- [ ] Output captured and returned
- [ ] Permission checks work
- [ ] Temp file cleaned up

---

## Troubleshooting

### "Connection refused"
- Check servers are running
- Verify ports (8080 for NM, 9001+ for SS)

### "File not found"
- Check file exists in ss_storage directory
- Verify metadata created (CREATE command)

### "Permission denied"
- Check user is owner or has ACL entry
- Verify ACL with `INFO filename`

### "Sentence locked"
- Another user has active WRITE session
- Wait for ETIRW or disconnect

### Lock not released
- Ensure ETIRW is called
- Client disconnect releases locks (TODO: verify implementation)

### EXEC doesn't work
- Check script syntax
- Verify Name Server has permissions
- Check /tmp directory exists

---

## Clean Up After Testing

```bash
# Stop servers (Ctrl+C in each terminal)

# Clean storage
rm -rf ss_storage1/* ss_storage2/*

# Remove logs
rm -f logs/*.log

# Reset
make clean
```

---

## Expected Performance

- **READ**: Instant for small files (<1MB)
- **STREAM**: 0.1s per word
- **WRITE**: Interactive, depends on user input
- **EXEC**: Depends on script complexity
- **Concurrent clients**: 10+ without issues

---

## Success Criteria

✅ All operations complete without crashes  
✅ Permissions enforced correctly  
✅ Locks prevent race conditions  
✅ Files persist across operations  
✅ Backups created for UNDO  
✅ Concurrent operations work  
✅ Error handling graceful  

---

**Phase 5 Implementation Status: COMPLETE** ✓

All operations implemented and ready for testing!
