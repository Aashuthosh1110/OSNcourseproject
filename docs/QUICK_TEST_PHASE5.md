# Phase 5 Quick Test Instructions

## 🚀 Quick Start (3 Steps)

### 1. Compile & Setup
```bash
cd course-project-alphaq
make clean && make
mkdir -p ss_storage_test
```

### 2. Start Servers (3 terminals)

**Terminal 1 - Name Server:**
```bash
./bin/name_server 8080
```

**Terminal 2 - Storage Server:**
```bash
./bin/storage_server 127.0.0.1 8080 ss_storage_test 9001
```

**Terminal 3 - Run Test Script:**
```bash
./test_phase5.sh
```

---

## 📋 Manual Testing (Quick Commands)

### Terminal 4 - Client (alice):
```bash
./bin/client alice 127.0.0.1 8080
```

### Test Each Operation:

#### 1. **READ** (2 minutes)
```bash
# Setup
echo "Test file content here" > ss_storage_test/test.txt

# In client
CREATE test.txt
READ test.txt
```
**Expected:** File content displays

#### 2. **STREAM** (2 minutes)
```bash
# Setup
echo "Word1 Word2 Word3 Word4 Word5" > ss_storage_test/stream.txt

# In client
CREATE stream.txt
STREAM stream.txt
```
**Expected:** Words appear with 0.1s delays

#### 3. **WRITE** (3 minutes)
```bash
# Setup
echo "Original words here" > ss_storage_test/write.txt

# In client
CREATE write.txt
WRITE write.txt 0

# Interactive session
WRITE> 0 Hello
WRITE> 1 World
WRITE> ETIRW
```
**Expected:** Lock acquired, words updated, backup created

#### 4. **UNDO** (2 minutes)
```bash
# After doing WRITE above
UNDO write.txt
READ write.txt
```
**Expected:** File restored to original

#### 5. **EXEC** (2 minutes)
```bash
# Setup
echo "echo 'Hello from script!'" > ss_storage_test/script.sh

# In client
CREATE script.sh
EXEC script.sh
```
**Expected:** Script output displayed

---

## 🧪 Test Checklist

### Phase 5.1: Concurrency ✓
- [ ] Multiple clients connect without blocking
- [ ] Open 3 terminals, connect 3 clients simultaneously
- [ ] All should connect successfully

### Phase 5.2: READ/STREAM ✓
- [ ] READ displays file content immediately
- [ ] STREAM shows words with delays
- [ ] Permission denied for unauthorized users
- [ ] ADDACCESS -R grants read access

### Phase 5.3: WRITE/ETIRW/UNDO ✓
- [ ] WRITE acquires lock
- [ ] Second user gets "locked" error
- [ ] ETIRW saves and creates .bak file
- [ ] UNDO restores from backup
- [ ] Lock released after ETIRW

### Phase 5.4: EXEC ✓
- [ ] Script executes on Name Server
- [ ] Output captured and displayed
- [ ] Permission checks enforced

---

## ⚡ One-Liner Tests

```bash
# Test all at once
echo "Test" > ss_storage_test/quick.txt && \
./bin/client alice 127.0.0.1 8080 << EOF
CREATE quick.txt
READ quick.txt
STREAM quick.txt
WRITE quick.txt 0
0 Modified
ETIRW
UNDO quick.txt
EXIT
EOF
```

---

## 🔍 Verify Results

### Check files created:
```bash
ls -la ss_storage_test/
```

### Check backup exists:
```bash
ls -la ss_storage_test/*.bak
```

### Check metadata:
```bash
cat ss_storage_test/*.meta
```

---

## 🐛 Troubleshooting

| Problem | Solution |
|---------|----------|
| "Connection refused" | Start servers first |
| "File not found" | Run CREATE command first |
| "Permission denied" | Use ADDACCESS to grant access |
| "Sentence locked" | Wait for other user's ETIRW |
| No output from STREAM | Check file has content |
| EXEC doesn't work | Check script syntax, /tmp permissions |

---

## 📊 Expected Timings

| Operation | Time |
|-----------|------|
| READ | Instant (<100ms) |
| STREAM (10 words) | ~1 second |
| WRITE (interactive) | User-dependent |
| ETIRW | ~100ms |
| UNDO | ~100ms |
| EXEC | Script-dependent |

---

## 🎯 Success Criteria

✅ All 6 operations complete without crashes  
✅ Permissions enforced correctly  
✅ Locks prevent race conditions  
✅ Backups created for UNDO  
✅ Multiple clients work concurrently  
✅ Error messages are clear  

---

## 📞 Quick Help

**Full Guide:** See `PHASE5_TESTING_GUIDE.md`  
**Interactive Script:** Run `./test_phase5.sh`  
**Issues:** Check server logs in `logs/` directory

---

**Total Test Time: ~15-20 minutes for complete testing**
