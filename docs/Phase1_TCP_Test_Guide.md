# Phase 1: Basic TCP Connectivity Test Guide

## Overview
Phase 1 successfully implements basic TCP skeleton connectivity between all three components:
- Name Server (accepts multiple connections using select())
- Storage Server (connects to Name Server)  
- Client (connects to Name Server)

## How to Test Manually

### Step 1: Start Name Server
```bash
# Terminal 1
cd /home/arpit-mahtele/Desktop/sem-3/OSN/proj/course-project-alphaq
./bin/name_server 8080
```

Expected output:
```
Name Server starting on port 8080...
TCP socket initialized successfully on port 8080
Name Server listening on port 8080, waiting for connections...
```

### Step 2: Connect Storage Server
```bash
# Terminal 2
./bin/storage_server 127.0.0.1 8080
```

Expected output in Terminal 2:
```
Storage Server starting...
Attempting to connect to Name Server at 127.0.0.1:8080
Connected to Name Server at 127.0.0.1:8080
Storage Server connected. Press Ctrl+C to exit.
```

Expected output in Terminal 1 (Name Server):
```
New connection received from 127.0.0.1:XXXXX (fd=4)
```

### Step 3: Connect Client
```bash
# Terminal 3
./bin/client 127.0.0.1 8080
# When prompted, enter any username (e.g., "testuser")
```

Expected output in Terminal 3:
```
=== Docs++ Client (Phase 1) ===
Enter your username: testuser
Attempting to connect to Name Server at 127.0.0.1:8080
Connected to Name Server. Username: testuser
Client connected successfully. Press Ctrl+C to exit.
```

Expected output in Terminal 1 (Name Server):
```
New connection received from 127.0.0.1:XXXXX (fd=5)
```

## Test Results

✅ **Name Server**:
- Creates TCP socket on specified port
- Uses select() for non-blocking connection handling
- Successfully accepts multiple connections
- Prints "New connection received" for each connection
- Detects when connections close

✅ **Storage Server**:
- Takes Name Server IP and port as command line arguments
- Successfully connects to Name Server
- Prints "Connected to Name Server" on success
- Keeps connection alive until terminated

✅ **Client**:
- Takes Name Server IP and port as command line arguments
- Asks for username input
- Successfully connects to Name Server
- Prints "Connected to Name Server. Username: [username]" on success
- Keeps connection alive until terminated

## Architecture Demonstrated

```
[Name Server:8080] ←─── [Storage Server] (Connection 1)
      ↑
      └─────────────── [Client] (Connection 2)
```

The Name Server successfully manages multiple concurrent TCP connections using select(), proving the foundation for the distributed system is working correctly.

## Next Steps for Phase 2
- Add actual protocol message exchange
- Implement registration handshake
- Add proper message routing
- Handle connection cleanup and reconnection