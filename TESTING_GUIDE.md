# Phase 3 Testing Guide

## Quick Start

### Option 1: Automated Script (Recommended)

Run the test script in one terminal:

```bash
./test_phase3_simple.sh
```

This will start both servers and display instructions. Keep this terminal open.

In a **second terminal**, run the client:

```bash
cd /home/aashuthosh-s-sharma/Desktop/OSN/courseproject/course-project-alphaq/course-project-alphaq
./bin/client localhost 8080
```

### Option 2: Manual Setup

**Terminal 1 - Name Server:**
```bash
cd /home/aashuthosh-s-sharma/Desktop/OSN/courseproject/course-project-alphaq/course-project-alphaq
./bin/name_server 8080
```

**Terminal 2 - Storage Server:**
```bash
cd /home/aashuthosh-s-sharma/Desktop/OSN/courseproject/course-project-alphaq/course-project-alphaq
mkdir -p test_storage/ss1
./bin/storage_server localhost 8080 test_storage/ss1 9001
```

**Terminal 3 - Client:**
```bash
cd /home/aashuthosh-s-sharma/Desktop/OSN/courseproject/course-project-alphaq/course-project-alphaq
./bin/client localhost 8080
```

---

## Test Cases

When the client starts, you'll be asked for a username. Enter: `testuser`

Then try these commands in order:

### Test 1: CREATE - Basic File Creation

```
CREATE test1.txt
```

**Expected Output:**
```
File 'test1.txt' created successfully!
```

**Verify:**
```bash
# In another terminal
ls -la test_storage/ss1/
# Should see: test1.txt and test1.txt.meta
```

---

### Test 2: CREATE - Duplicate File (Should Fail)

```
CREATE test1.txt
```

**Expected Output:**
```
Error: File 'test1.txt' already exists
```

---

### Test 3: CREATE - Multiple Files

```
CREATE test2.txt
CREATE test3.txt
```

**Expected Output:**
```
File 'test2.txt' created successfully!
File 'test3.txt' created successfully!
```

**Verify:**
```bash
ls -la test_storage/ss1/
# Should see: test1.txt, test2.txt, test3.txt (and their .meta files)
```

---

### Test 4: CREATE - Invalid Filename (Should Fail)

```
CREATE ../../../etc/passwd
```

**Expected Output:**
```
Error: Invalid filename
```

---

### Test 5: DELETE - Delete Owned File

```
DELETE test1.txt
```

**Expected Output:**
```
File 'test1.txt' deleted successfully!
```

**Verify:**
```bash
ls -la test_storage/ss1/
# test1.txt should be gone
```

---

### Test 6: DELETE - Non-existent File (Should Fail)

```
DELETE test1.txt
```

**Expected Output:**
```
Error: File 'test1.txt' not found
```

---

### Test 7: DELETE - Another File

```
DELETE test2.txt
```

**Expected Output:**
```
File 'test2.txt' deleted successfully!
```

---

### Test 8: HELP - View Available Commands

```
HELP
```

**Expected Output:**
```
=== Docs++ Commands ===
File Operations:
  VIEW [-a] [-l]           - List files (use -a for all, -l for details)
  READ <filename>          - Read file content
  CREATE <filename>        - Create new file
  WRITE <filename> <sent#> - Edit file sentence
  DELETE <filename>        - Delete file
  INFO <filename>          - Show file information
  STREAM <filename>        - Stream file content
  UNDO <filename>          - Undo last change

Access Control:
  ADDACCESS -R <file> <user> - Grant read access
  ADDACCESS -W <file> <user> - Grant write access
  REMACCESS <file> <user>    - Remove access

System:
  LIST                     - List all users
  EXEC <filename>          - Execute file as commands
  HELP                     - Show this help
  QUIT / EXIT              - Exit client
```

---

### Test 9: EXIT - Close Client

```
EXIT
```

**Expected Output:**
```
Goodbye!
```

---

## Advanced Tests

### Test 10: Ownership Check

1. Start a **new client** (different username):
   ```bash
   ./bin/client localhost 8080
   # Username: user2
   ```

2. Try to delete a file created by testuser:
   ```
   DELETE test3.txt
   ```

**Expected Output:**
```
Error: Only the owner can delete this file
```

---

### Test 11: Metadata Verification

Check the metadata file:

```bash
cat test_storage/ss1/test3.txt.meta
```

**Expected Output:**
```
owner=testuser
created=<timestamp>
modified=<timestamp>
accessed=<timestamp>
accessed_by=testuser
size=0
word_count=0
char_count=0
access_count=1
access_0=testuser:RW
```

---

## What to Look For

### ✅ Success Indicators

1. **File Creation:**
   - Client receives success message
   - File appears in `test_storage/ss1/`
   - `.meta` file is created alongside data file
   - Name Server logs show registration
   - Storage Server logs show file creation

2. **File Deletion:**
   - Client receives success message
   - File disappears from `test_storage/ss1/`
   - Both `.txt` and `.meta` files removed
   - Name Server logs show de-registration

3. **Error Handling:**
   - Duplicate files rejected
   - Non-existent files rejected
   - Invalid filenames rejected
   - Ownership enforced

### ❌ Potential Issues

If something doesn't work:

1. **Check if servers are running:**
   ```bash
   ps aux | grep bin/name_server
   ps aux | grep bin/storage_server
   ```

2. **Check logs (if redirected):**
   ```bash
   tail -f logs/nm_test.log
   tail -f logs/ss_test.log
   ```

3. **Check terminal output:**
   - Name Server should show connection messages
   - Storage Server should show file operations
   - Client should show command results

4. **Verify file system:**
   ```bash
   ls -la test_storage/ss1/
   stat test_storage/ss1/test1.txt
   ```

---

## Cleanup After Testing

Stop all servers:
```bash
pkill -f "bin/name_server"
pkill -f "bin/storage_server"
```

Clean test data:
```bash
rm -rf test_storage/
rm -rf logs/
```

---

## Full Test Sequence (Copy-Paste)

Once the client is running, copy and paste these commands one at a time:

```
CREATE file1.txt
CREATE file2.txt
CREATE file3.txt
CREATE file1.txt
DELETE file1.txt
DELETE file1.txt
DELETE file2.txt
HELP
EXIT
```

After running, verify:
```bash
ls -la test_storage/ss1/
# Should only show file3.txt and file3.txt.meta
```

---

## Expected Server Console Output

### Name Server Console:
```
Name Server starting on port 8080...
Name Server listening on port 8080, waiting for connections...
New connection received from 127.0.0.1:<port> (fd=X)
Processing SS_INIT from fd=X
Registered SS from 127.0.0.1:9001 with 0 files (fd=X)
New connection received from 127.0.0.1:<port> (fd=Y)
Processing CLIENT_INIT from fd=Y
Registered client 'testuser' from 127.0.0.1 (fd=Y)
```

### Storage Server Console:
```
Storage Server starting...
Name Server: localhost:8080
Storage Path: test_storage/ss1
Client Port: 9001
Total files discovered: 0
Connected to Name Server at 127.0.0.1:8080
Sending SS_INIT packet with 0 files...
SS initialization successful: SS registered: 0 files
Storage Server registered. Press Ctrl+C to exit.
```

---

## Troubleshooting

### Issue: "Connection refused"
- Ensure Name Server is running on port 8080
- Check: `netstat -tuln | grep 8080`

### Issue: "No storage servers available"
- Ensure Storage Server registered with Name Server
- Check Name Server output for "Registered SS" message
- Wait 2-3 seconds after starting Storage Server

### Issue: "Invalid filename"
- Don't use special characters: `< > : " | ? * /`
- Don't use path separators
- Keep filenames under 256 characters

### Issue: File not deleted
- Check if you're the owner
- Verify file exists in Name Server registry
- Check Storage Server logs for errors

---

## Success Criteria Checklist

- [ ] Name Server starts successfully
- [ ] Storage Server connects and registers
- [ ] Client connects and authenticates
- [ ] CREATE command creates file on disk
- [ ] CREATE command creates metadata file
- [ ] Duplicate CREATE is rejected
- [ ] DELETE command removes file from disk
- [ ] DELETE command removes metadata file
- [ ] DELETE on non-existent file is rejected
- [ ] Ownership is enforced for DELETE
- [ ] HELP command shows all commands
- [ ] EXIT command closes client cleanly
- [ ] Invalid filenames are rejected
- [ ] All operations are logged

---

**That's it! Your Phase 3 implementation is ready for testing. Good luck! 🚀**
