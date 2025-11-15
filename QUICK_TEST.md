# Phase 3 Testing - Quick Reference

## 🎯 Your Servers Are Running!

The Name Server and Storage Server are currently running in the background.

## 📝 How to Test (Interactive Mode)

### Open a NEW terminal window and run:

```bash
cd /home/aashuthosh-s-sharma/Desktop/OSN/courseproject/course-project-alphaq/course-project-alphaq
./bin/client localhost 8080
```

When prompted for username, type: **testuser** and press Enter

---

## 🧪 Test Commands (Copy-Paste One at a Time)

### Test 1: Create your first file
```
CREATE file1.txt
```
✅ Expected: `File 'file1.txt' created successfully!`

### Test 2: Create more files
```
CREATE file2.txt
```
✅ Expected: `File 'file2.txt' created successfully!`

```
CREATE file3.txt
```
✅ Expected: `File 'file3.txt' created successfully!`

### Test 3: Try to create a duplicate (should fail)
```
CREATE file1.txt
```
❌ Expected: `Error: File 'file1.txt' already exists`

### Test 4: Delete a file
```
DELETE file1.txt
```
✅ Expected: `File 'file1.txt' deleted successfully!`

### Test 5: Try to delete the same file again (should fail)
```
DELETE file1.txt
```
❌ Expected: `Error: File 'file1.txt' not found`

### Test 6: Delete another file
```
DELETE file2.txt
```
✅ Expected: `File 'file2.txt' deleted successfully!`

### Test 7: View help
```
HELP
```
✅ Expected: Shows list of all available commands

### Test 8: Exit the client
```
EXIT
```
✅ Expected: Client exits gracefully

---

## 🔍 Verification Commands (Run in another terminal)

### Check created files:
```bash
ls -la /home/aashuthosh-s-sharma/Desktop/OSN/courseproject/course-project-alphaq/course-project-alphaq/test_storage/ss1/
```

You should see:
- `file3.txt` (the only remaining file)
- `file3.txt.meta` (metadata file)

### View metadata content:
```bash
cat /home/aashuthosh-s-sharma/Desktop/OSN/courseproject/course-project-alphaq/course-project-alphaq/test_storage/ss1/file3.txt.meta
```

Should show:
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

## 🎬 Full Test Sequence (All at Once)

If you want to test quickly, paste all these commands after the client starts:

```
CREATE test_a.txt
CREATE test_b.txt
CREATE test_c.txt
CREATE test_a.txt
DELETE test_a.txt
DELETE test_a.txt
DELETE test_b.txt
HELP
EXIT
```

Then verify with:
```bash
ls -la test_storage/ss1/
# Should only show test_c.txt and test_c.txt.meta
```

---

## 🛑 When Done Testing

To stop the servers, go back to the terminal where you started the test script and press **Ctrl+C**

Or manually:
```bash
pkill -f "bin/name_server"
pkill -f "bin/storage_server"
```

To clean up test files:
```bash
rm -rf test_storage/
```

---

## ✅ What to Look For

### Success Indicators:
- ✓ Files appear in `test_storage/ss1/` after CREATE
- ✓ Files disappear after DELETE
- ✓ `.meta` files created alongside data files
- ✓ Duplicate creates are rejected
- ✓ Non-existent deletes are rejected
- ✓ Client shows clear success/error messages

### Check Server Outputs:
Look at the terminal running servers - you should see:
- "New connection received" when client connects
- "Registered client 'testuser'" when client initializes
- Command processing messages for CREATE/DELETE

---

## 🐛 Troubleshooting

### If client won't connect:
1. Check servers are running: `ps aux | grep bin/name_server`
2. Check port 8080 is listening: `netstat -tuln | grep 8080`

### If commands don't work:
1. Make sure you typed the username correctly
2. Wait 1-2 seconds between commands
3. Check for typos in filenames

### If files aren't created:
1. Check storage directory exists: `ls -la test_storage/ss1/`
2. Check Storage Server is connected (see server output)
3. Check permissions on test_storage directory

---

## 📊 Current Status

✅ **Name Server**: Running on port 8080  
✅ **Storage Server**: Running on port 9001, storing files in `test_storage/ss1/`  
✅ **Client**: Ready to connect

**You're all set!** Open a new terminal and start testing! 🚀

---

## Quick Commands Reference

| Command | Description | Example |
|---------|-------------|---------|
| `CREATE <filename>` | Create new empty file | `CREATE myfile.txt` |
| `DELETE <filename>` | Delete file (owner only) | `DELETE myfile.txt` |
| `HELP` | Show all commands | `HELP` |
| `EXIT` or `QUIT` | Close client | `EXIT` |

---

**Have fun testing!** 🎉
