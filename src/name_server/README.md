# Name Server Implementation

The Name Server acts as the central coordinator of the Docs++ system, managing file locations, metadata, and coordinating between clients and storage servers.

## Files

- `name_server.c` - Main name server implementation
- `file_manager.c` - File metadata and location management
- `client_handler.c` - Client request handling
- `storage_manager.c` - Storage server management and load balancing
- `search.c` - Efficient file search algorithms (tries, hashmaps)
- `cache.c` - Caching implementation for recent searches

## Key Responsibilities

1. **Client Registration**: Handle client connections and maintain active client list
2. **Storage Server Registration**: Manage storage server connections and file listings
3. **File Location**: Direct clients to appropriate storage servers for file operations
4. **Access Control**: Enforce file permissions and ownership
5. **Metadata Management**: Maintain file metadata and user information
6. **Load Balancing**: Distribute files across storage servers efficiently
7. **Logging**: Record all operations and system events