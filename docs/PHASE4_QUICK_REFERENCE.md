# Phase 4 Quick Reference

## Commands

### LIST - Show connected users
```bash
LIST
```
Example output:
```
Connected Users:
1. alice (connected from 127.0.0.1 at 2025-11-14 10:30:15)
2. bob (connected from 127.0.0.1 at 2025-11-14 10:31:22)
```

---

### VIEW - List files
```bash
VIEW           # Show files you have access to
VIEW -a        # Show ALL files on system
VIEW -l        # Long format (permissions + owner)
VIEW -al       # All files, long format
```

Example output:
```
# VIEW
Files:
  file1.txt
  file2.txt

# VIEW -l
Permissions  Owner          Filename
------------------------------------------------------------
RW           alice          file1.txt
R-           bob            shared.txt
```

---

### INFO - Show file details
```bash
INFO <filename>
```

Example:
```bash
INFO test.txt
```

Output:
```
File Information:
  Name: test.txt
  Owner: alice
  Size: 1024 bytes
  Word Count: 150
  Character Count: 1024
  Created: 2025-11-14 10:30:15
  Last Modified: 2025-11-14 11:45:32
  Last Accessed: 2025-11-14 12:10:05 by alice
  Access Control:
    alice: RW
    bob: R-
```

---

### ADDACCESS - Grant permissions
```bash
ADDACCESS -R <filename> <username>  # Grant read access
ADDACCESS -W <filename> <username>  # Grant write access (+ read)
```

Examples:
```bash
ADDACCESS -R document.txt bob      # Bob can read
ADDACCESS -W document.txt charlie  # Charlie can read and write
```

**Rules:**
- Only the file owner can grant access
- Write permission automatically includes read

---

### REMACCESS - Revoke permissions
```bash
REMACCESS <filename> <username>
```

Example:
```bash
REMACCESS document.txt bob  # Bob loses all access
```

**Rules:**
- Only the file owner can revoke access
- Cannot remove owner's own access

---

## Permission Codes

| Code | Meaning |
|------|---------|
| `RW` | Read + Write (owner or granted) |
| `R-` | Read only |
| `--` | No access |
| `---` | Not accessible (in VIEW -a) |

---

## Quick Test Sequence

1. **Create files:**
   ```bash
   CREATE file1.txt
   CREATE file2.txt
   ```

2. **List users:**
   ```bash
   LIST
   ```

3. **View your files:**
   ```bash
   VIEW
   VIEW -l
   ```

4. **Check file info:**
   ```bash
   INFO file1.txt
   ```

5. **Grant access to another user:**
   ```bash
   ADDACCESS -R file1.txt bob
   ```

6. **Verify access granted:**
   ```bash
   INFO file1.txt
   # Bob should appear in Access Control list
   ```

7. **Test from Bob's client:**
   ```bash
   VIEW
   # Should now see file1.txt
   INFO file1.txt
   # Should work
   ```

8. **Revoke access:**
   ```bash
   REMACCESS file1.txt bob
   ```

9. **Verify access revoked:**
   ```bash
   # From Bob's client:
   VIEW
   # file1.txt should be gone
   INFO file1.txt
   # Should show "Permission denied"
   ```

---

## Common Errors

### Permission Denied
```
Error: Permission denied
```
**Cause:** You don't have access to this file
**Fix:** Ask owner to grant you access with ADDACCESS

---

### File Not Found
```
Error: File 'xxx' not found
```
**Cause:** File doesn't exist
**Fix:** Check filename spelling, use VIEW -a to see all files

---

### Only Owner Can Modify
```
Error: Only the owner can modify access control
```
**Cause:** You're not the file owner
**Fix:** Only the owner can use ADDACCESS/REMACCESS

---

### Cannot Remove Owner
```
Error: Cannot remove owner's access
```
**Cause:** Trying to revoke owner's own access
**Fix:** Owner always has access, cannot be removed

---

### Invalid Permission Flag
```
Error: Invalid permission flag. Use -R or -W
```
**Cause:** Wrong flag provided to ADDACCESS
**Fix:** Use `-R` for read or `-W` for write

---

## Testing with Multiple Clients

**Terminal 1 (Alice):**
```bash
./bin/client
Username: alice
docs++ > CREATE shared.txt
docs++ > ADDACCESS -R shared.txt bob
docs++ > ADDACCESS -W shared.txt charlie
docs++ > LIST
docs++ > INFO shared.txt
```

**Terminal 2 (Bob):**
```bash
./bin/client
Username: bob
docs++ > VIEW
docs++ > INFO shared.txt
# Should work (has read access)
```

**Terminal 3 (Charlie):**
```bash
./bin/client
Username: charlie
docs++ > VIEW
docs++ > INFO shared.txt
# Should work (has read+write access)
```

---

## See Also

- **docs/Phase4_Implementation.md** - Detailed implementation guide
- **docs/Phase4_Summary.md** - Complete summary
- **test_phase4.sh** - Automated test script
- **TESTING_GUIDE.md** - General testing guide
