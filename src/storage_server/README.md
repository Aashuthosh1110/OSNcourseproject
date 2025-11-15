# Storage Server Implementation

Storage Servers handle the actual file storage, retrieval, and persistence in the Docs++ system.

## Files

- `storage_server.c` - Main storage server implementation
- `file_handler.c` - File I/O operations and management  
- `sentence_manager.c` - Sentence-level locking and operations
- `backup_manager.c` - File backup and undo functionality
- `client_connection.c` - Direct client communication handling
- `persistence.c` - Data persistence and recovery

## Key Responsibilities

1. **File Storage**: Physical storage and retrieval of file data
2. **Sentence Locking**: Implement sentence-level locks for concurrent editing
3. **File Operations**: Handle read, write, create, delete operations
4. **Streaming**: Provide word-by-word streaming functionality
5. **Backup Management**: Maintain file backups for undo operations
6. **Persistence**: Ensure data durability across restarts
7. **Metadata Sync**: Keep file metadata synchronized with Name Server