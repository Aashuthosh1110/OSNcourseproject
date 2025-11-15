# Docs++ - Distributed Document System

A simplified, shared document system with support for concurrency and access control, similar in spirit to Google Docs.

## Team: AlphaQ

## Project Overview

Docs++ is a distributed file system consisting of three main components:
- **Name Server (NM)**: Central coordinator managing file locations and metadata
- **Storage Servers (SS)**: Handle file storage, retrieval, and persistence
- **User Clients**: Provide interface for file operations

## Features

### Core Functionalities (150 marks)
- [x] View files with various flags (-a, -l, -al)
- [x] Read file contents
- [x] Create new files
- [x] Write to files with sentence-level locking
- [x] Undo last changes
- [x] Get file information and metadata
- [x] Delete files
- [x] Stream file content word-by-word
- [x] List system users
- [x] Access control (read/write permissions)
- [x] Execute files as shell commands

### System Requirements (40 marks)
- [x] Data persistence
- [x] Access control enforcement
- [x] Comprehensive logging
- [x] Error handling with error codes
- [x] Efficient search algorithms

### Bonus Features (50 marks - Optional)
- [ ] Hierarchical folder structure
- [ ] Checkpoints and versioning
- [ ] Access request system
- [ ] Fault tolerance and replication
- [ ] Unique innovative features

## Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   User Client   │◄──►│   Name Server   │◄──►│ Storage Server  │
│                 │    │      (NM)       │    │      (SS)       │
│ - File ops      │    │ - Coordination  │    │ - File storage  │
│ - User interface│    │ - Metadata      │    │ - Persistence   │
│ - Authentication│    │ - Load balancing│    │ - Concurrency   │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## Getting Started

### Prerequisites
- GCC compiler
- POSIX-compliant system (Linux/Unix)
- TCP socket support

### Building
```bash
make all          # Build all components
make name_server  # Build only name server
make storage_server # Build only storage server  
make client       # Build only client
```

### Running
1. Start the Name Server:
   ```bash
   ./bin/name_server <port>
   ```

2. Start Storage Server(s):
   ```bash
   ./bin/storage_server <nm_ip> <nm_port> <storage_path>
   ```

3. Start Client(s):
   ```bash
   ./bin/client <nm_ip> <nm_port>
   ```

## Usage Examples

### Basic File Operations
```bash
# View files
VIEW                    # User's accessible files
VIEW -a                 # All files in system
VIEW -l                 # With details
VIEW -al               # All files with details

# File operations
CREATE myfile.txt      # Create new file
READ myfile.txt        # Read file content
WRITE myfile.txt 0     # Edit sentence 0
1 Hello world!         # Insert at word position 1
ETIRW                  # End write operation

# Advanced operations
INFO myfile.txt        # Get file metadata
DELETE myfile.txt      # Delete file
STREAM myfile.txt      # Stream content word-by-word
EXEC script.txt        # Execute as shell commands
```

### Access Control
```bash
ADDACCESS -R myfile.txt user2    # Grant read access
ADDACCESS -W myfile.txt user2    # Grant write access
REMACCESS myfile.txt user2       # Remove access
```

## Protocol Specification

### Communication Flow
1. **Client → Name Server**: All requests initially go to NM
2. **Name Server Processing**: 
   - Metadata operations: Handled directly by NM
   - File operations: NM provides SS location to client
3. **Client ↔ Storage Server**: Direct communication for file I/O
4. **Storage Server → Name Server**: Acknowledgments and updates

### Error Codes
- `ERR_FILE_NOT_FOUND`: Requested file doesn't exist
- `ERR_ACCESS_DENIED`: Insufficient permissions
- `ERR_FILE_LOCKED`: Sentence being edited by another user
- `ERR_INVALID_INDEX`: Word/sentence index out of range
- `ERR_SERVER_UNAVAILABLE`: Storage server connection failed

## Development

### Directory Structure
```
├── src/              # Source code
├── include/          # Header files  
├── bin/              # Compiled binaries
├── tests/            # Test files
├── docs/             # Documentation
├── scripts/          # Utility scripts
└── storage/          # Storage server data directories
```

### Testing
```bash
make test            # Run all tests
./scripts/test_basic.sh    # Basic functionality tests
./scripts/test_concurrent.sh # Concurrency tests
```

## Team Members
- Member 1: Name Server implementation
- Member 2: Storage Server implementation  
- Member 3: Client implementation
- Member 4: Testing and integration

## Deadline
November 18, 2025, 11:59 PM IST

## License
MIT License - Copyright © 2024 Karthik Vaidhyanathan