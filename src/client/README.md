# User Client Implementation

The User Client provides the interface for users to interact with the Docs++ system, handling all user commands and communications with the Name Server and Storage Servers.

## Files

- `client.c` - Main client implementation and user interface
- `command_parser.c` - Parse and validate user commands
- `nm_communication.c` - Name Server communication handling
- `ss_communication.c` - Direct Storage Server communication  
- `file_viewer.c` - File viewing and listing functionality
- `stream_handler.c` - Handle file streaming operations

## Key Responsibilities

1. **User Interface**: Provide command-line interface for user operations
2. **Command Parsing**: Parse and validate user commands
3. **Authentication**: Handle user login and session management
4. **File Operations**: Execute file operations (VIEW, READ, CREATE, WRITE, etc.)
5. **Access Control**: Handle permission-related commands
6. **Streaming**: Display streamed file content with proper timing
7. **Error Handling**: Display user-friendly error messages