/*
 * User Client - Interactive interface for Docs++ system
 * Handles user commands, Name Server communication, and direct
 * Storage Server interactions for file operations.
 */

#include "../../include/common.h"
#include "../../include/protocol.h"
#include "../../include/logging.h"
#include "../../include/errors.h"
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <netdb.h>

// Global state
static char username[MAX_USERNAME_LEN];
static char nm_ip[INET_ADDRSTRLEN];
static int nm_port;
static int nm_socket = -1;
static int connected = 0;

// Function prototypes
void connect_to_name_server();
void register_with_name_server();
void handle_user_commands();
void execute_command(const char* input);
void cleanup_and_exit(int signal);

// Phase 2: New functions
void send_client_init_packet();
int parse_command(const char* input, char* cmd_str, char* args);
void display_help();
void handle_view_command(command_t cmd, const char* args);
void handle_read_command(command_t cmd, const char* args);
void handle_create_command(command_t cmd, const char* args);
void handle_write_command(command_t cmd, const char* args);
void handle_delete_command(command_t cmd, const char* args);
void handle_info_command(command_t cmd, const char* args);
void handle_stream_command(command_t cmd, const char* args);
void handle_list_command(command_t cmd, const char* args);
void handle_access_command(command_t cmd, const char* args);
void handle_exec_command(command_t cmd, const char* args);
void handle_undo_command(command_t cmd, const char* args);

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <nm_ip> <nm_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    strncpy(nm_ip, argv[1], sizeof(nm_ip) - 1);
    nm_port = atoi(argv[2]);
    
    // Validate arguments
    if (nm_port <= 0 || nm_port > 65535) {
        fprintf(stderr, "Error: Invalid Name Server port\n");
        exit(EXIT_FAILURE);
    }
    
    // Set up signal handlers
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);
    
    printf("=== Docs++ Client (Phase 1) ===\n");
    
    // Get username from user
    printf("Enter your username: ");
    if (fgets(username, sizeof(username), stdin) == NULL) {
        fprintf(stderr, "Error reading username\n");
        exit(EXIT_FAILURE);
    }
    
    // Remove newline character
    username[strcspn(username, "\n")] = 0;
    
    printf("Attempting to connect to Name Server at %s:%d\n", nm_ip, nm_port);
    
    // Phase 2: Connect and send initialization
    connect_to_name_server();
    send_client_init_packet();
    
    printf("Connected to Name Server. Username: %s\n", username);
    printf("Client registered successfully.\n");
    printf("Type 'HELP' for available commands or 'EXIT' to quit.\n\n");
    
    // Enter command loop
    handle_user_commands();
    
    // Cleanup
    cleanup_and_exit(0);
    
    return 0;
}

void connect_to_name_server() {
    // Create TCP socket
    nm_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (nm_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set up Name Server address
    struct sockaddr_in nm_addr;
    memset(&nm_addr, 0, sizeof(nm_addr));
    nm_addr.sin_family = AF_INET;
    nm_addr.sin_port = htons(nm_port);
    
    // Try to convert as IP address first, then as hostname
    if (inet_pton(AF_INET, nm_ip, &nm_addr.sin_addr) <= 0) {
        // Not a valid IP address, try as hostname
        struct hostent* host = gethostbyname(nm_ip);
        if (host == NULL) {
            fprintf(stderr, "Invalid hostname or IP address: %s\n", nm_ip);
            exit(EXIT_FAILURE);
        }
        memcpy(&nm_addr.sin_addr, host->h_addr_list[0], host->h_length);
    }
    
    // Connect to Name Server
    if (connect(nm_socket, (struct sockaddr*)&nm_addr, sizeof(nm_addr)) == -1) {
        perror("Connection to Name Server failed");
        exit(EXIT_FAILURE);
    }
    
    connected = 1;
}

void register_with_name_server() {
    // Create registration packet
    char client_info[MAX_ARGS_LEN];
    snprintf(client_info, sizeof(client_info), "CLIENT %s", username);
    
    request_packet_t reg_packet = create_request_packet(CMD_REGISTER_CLIENT, username, client_info);
    
    if (send_packet(nm_socket, &reg_packet) == -1) {
        fprintf(stderr, "Error: Failed to send registration to Name Server\n");
        exit(EXIT_FAILURE);
    }
    
    // Wait for acknowledgment
    response_packet_t response;
    int result = recv_packet(nm_socket, &response);
    if (result <= 0) {
        fprintf(stderr, "Error: No response from Name Server (result: %d)\n", result);
        exit(EXIT_FAILURE);
    }
    
    if (response.status != STATUS_OK) {
        fprintf(stderr, "Error: Registration failed: %s\n", status_to_string(response.status));
        exit(EXIT_FAILURE);
    }
    
    LOG_INFO_MSG("CLIENT", "Successfully registered with Name Server");
}

void handle_user_commands() {
    char* input;
    
    while ((input = readline("docs++ > ")) != NULL) {
        // Skip empty commands
        if (strlen(input) == 0) {
            free(input);
            continue;
        }
        
        // Add to history
        add_history(input);
        
        // Check for quit command
        if (strcasecmp(input, "QUIT") == 0 || strcasecmp(input, "EXIT") == 0) {
            free(input);
            break;
        }
        
        // Execute command
        execute_command(input);
        free(input);
    }
}

void execute_command(const char* input) {
    char cmd_str[32];
    char args[MAX_ARGS_LEN];
    
    if (parse_command(input, cmd_str, args) != 0) {
        printf("Error: Invalid command format. Type 'HELP' for usage.\n");
        return;
    }
    
    command_t cmd = string_to_command(cmd_str);
    
    LOG_INFO_MSG("CLIENT", "Executing command: %s", cmd_str);
    
        // Handle different commands
    if (strcasecmp(cmd_str, "HELP") == 0) {
        display_help();
    } else if (strcasecmp(cmd_str, "VIEW") == 0) {
        handle_view_command(cmd, args);
    } else if (strcasecmp(cmd_str, "READ") == 0) {
        handle_read_command(cmd, args);
    } else if (strcasecmp(cmd_str, "CREATE") == 0) {
        handle_create_command(cmd, args);
    } else if (strcasecmp(cmd_str, "WRITE") == 0) {
        handle_write_command(cmd, args);
    } else if (strcasecmp(cmd_str, "DELETE") == 0) {
        handle_delete_command(cmd, args);
    } else if (strcasecmp(cmd_str, "INFO") == 0) {
        handle_info_command(cmd, args);
    } else if (strcasecmp(cmd_str, "STREAM") == 0) {
        handle_stream_command(cmd, args);
    } else if (strcasecmp(cmd_str, "LIST") == 0) {
        handle_list_command(cmd, args);
    } else if (strcasecmp(cmd_str, "ADDACCESS") == 0 || 
               strcasecmp(cmd_str, "REMACCESS") == 0) {
        handle_access_command(cmd, args);
    } else if (strcasecmp(cmd_str, "EXEC") == 0) {
        handle_exec_command(cmd, args);
    } else if (strcasecmp(cmd_str, "UNDO") == 0) {
        handle_undo_command(cmd, args);
    } else {
        printf("Error: Unknown command '%s'. Type 'HELP' for available commands.\n", cmd_str);
    }
}

int parse_command(const char* input, char* cmd_str, char* args) {
    // Simple command parsing
    char* input_copy = strdup(input);
    char* token = strtok(input_copy, " ");
    
    if (token == NULL) {
        free(input_copy);
        return -1;
    }
    
    strncpy(cmd_str, token, 31);
    cmd_str[31] = '\0';
    
    // Get the rest as arguments
    char* remaining = strtok(NULL, "");
    if (remaining) {
        strncpy(args, remaining, MAX_ARGS_LEN - 1);
        args[MAX_ARGS_LEN - 1] = '\0';
    } else {
        args[0] = '\0';
    }
    
    free(input_copy);
    return 0;
}

void display_help() {
    printf("\n=== Docs++ Commands ===\n");
    printf("File Operations:\n");
    printf("  VIEW [-a] [-l]           - List files (use -a for all, -l for details)\n");
    printf("  READ <filename>          - Read file content\n");
    printf("  CREATE <filename>        - Create new file\n");
    printf("  WRITE <filename> <sent#> - Edit file sentence\n");
    printf("  DELETE <filename>        - Delete file\n");
    printf("  INFO <filename>          - Show file information\n");
    printf("  STREAM <filename>        - Stream file content\n");
    printf("  UNDO <filename>          - Undo last change\n");
    printf("\n");
    printf("Access Control:\n");
    printf("  ADDACCESS -R <file> <user> - Grant read access\n");
    printf("  ADDACCESS -W <file> <user> - Grant write access\n");
    printf("  REMACCESS <file> <user>    - Remove access\n");
    printf("\n");
    printf("System:\n");
    printf("  LIST                     - List all users\n");
    printf("  EXEC <filename>          - Execute file as commands\n");
    printf("  HELP                     - Show this help\n");
    printf("  QUIT / EXIT              - Exit client\n");
    printf("\n");
}

// Stub implementations for command handlers
// Phase 4: Handle VIEW command
void handle_view_command(command_t cmd, const char* args) { 
    (void)cmd;
    
    // Prepare request packet
    request_packet_t request;
    memset(&request, 0, sizeof(request));
    request.magic = PROTOCOL_MAGIC;
    request.command = CMD_VIEW;
    strncpy(request.username, username, sizeof(request.username) - 1);
    
    // Copy flags if provided
    if (args && strlen(args) > 0) {
        strncpy(request.args, args, sizeof(request.args) - 1);
    } else {
        request.args[0] = '\0';
    }
    
    request.checksum = calculate_checksum(&request, sizeof(request) - sizeof(uint32_t));
    
    // Send to Name Server
    if (send_packet(nm_socket, &request) < 0) {
        printf("Error: Failed to send VIEW request\n");
        return;
    }
    
    // Receive response
    response_packet_t response;
    if (recv_packet(nm_socket, &response) <= 0) {
        printf("Error: No response from Name Server\n");
        return;
    }
    
    // Display result
    if (response.status == STATUS_OK) {
        printf("%s", response.data);
    } else {
        printf("Error: %s\n", response.data);
    }
}

void handle_read_command(command_t cmd, const char* args) { 
    (void)cmd;
    printf("READ command not yet implemented. Args: %s\n", args ? args : "none"); 
}

void handle_create_command(command_t cmd, const char* args) {
    (void)cmd;
    
    // Parse filename from args
    char filename[MAX_FILENAME_LEN];
    if (args == NULL || strlen(args) == 0) {
        printf("Error: CREATE requires a filename\n");
        printf("Usage: CREATE <filename>\n");
        return;
    }
    
    // Extract filename (trim whitespace)
    sscanf(args, "%s", filename);
    
    // Validate filename
    if (!validate_filename(filename)) {
        printf("Error: Invalid filename. Use alphanumeric characters, dots, underscores, and hyphens only.\n");
        return;
    }
    
    LOG_INFO_MSG("CLIENT", "Creating file: %s", filename);
    
    // Create request packet
    request_packet_t request;
    memset(&request, 0, sizeof(request));
    request.magic = PROTOCOL_MAGIC;
    request.command = CMD_CREATE;
    strncpy(request.username, username, sizeof(request.username) - 1);
    snprintf(request.args, sizeof(request.args), "%s", filename);
    request.checksum = calculate_checksum(&request, sizeof(request) - sizeof(uint32_t));
    
    // Send to Name Server
    if (send_packet(nm_socket, &request) < 0) {
        printf("Error: Failed to send CREATE request to Name Server\n");
        LOG_ERROR_MSG("CLIENT", "Failed to send CREATE request for file: %s", filename);
        return;
    }
    
    // Wait for response
    response_packet_t response;
    int result = recv_packet(nm_socket, &response);
    if (result <= 0) {
        printf("Error: No response from Name Server\n");
        LOG_ERROR_MSG("CLIENT", "No response for CREATE request");
        return;
    }
    
    // Handle response
    if (response.status == STATUS_OK) {
        printf("File '%s' created successfully!\n", filename);
        LOG_INFO_MSG("CLIENT", "File created successfully: %s", filename);
    } else {
        printf("Error: %s\n", response.data);
        LOG_ERROR_MSG("CLIENT", "CREATE failed: %s", response.data);
    }
}

void handle_write_command(command_t cmd, const char* args) { 
    (void)cmd;
    printf("WRITE command not yet implemented. Args: %s\n", args ? args : "none"); 
}

void handle_delete_command(command_t cmd, const char* args) {
    (void)cmd;
    
    // Parse filename from args
    char filename[MAX_FILENAME_LEN];
    if (args == NULL || strlen(args) == 0) {
        printf("Error: DELETE requires a filename\n");
        printf("Usage: DELETE <filename>\n");
        return;
    }
    
    // Extract filename (trim whitespace)
    sscanf(args, "%s", filename);
    
    // Validate filename
    if (!validate_filename(filename)) {
        printf("Error: Invalid filename\n");
        return;
    }
    
    LOG_INFO_MSG("CLIENT", "Deleting file: %s", filename);
    
    // Create request packet
    request_packet_t request;
    memset(&request, 0, sizeof(request));
    request.magic = PROTOCOL_MAGIC;
    request.command = CMD_DELETE;
    strncpy(request.username, username, sizeof(request.username) - 1);
    snprintf(request.args, sizeof(request.args), "%s", filename);
    request.checksum = calculate_checksum(&request, sizeof(request) - sizeof(uint32_t));
    
    // Send to Name Server
    if (send_packet(nm_socket, &request) < 0) {
        printf("Error: Failed to send DELETE request to Name Server\n");
        LOG_ERROR_MSG("CLIENT", "Failed to send DELETE request for file: %s", filename);
        return;
    }
    
    // Wait for response
    response_packet_t response;
    int result = recv_packet(nm_socket, &response);
    if (result <= 0) {
        printf("Error: No response from Name Server\n");
        LOG_ERROR_MSG("CLIENT", "No response for DELETE request");
        return;
    }
    
    // Handle response
    if (response.status == STATUS_OK) {
        printf("File '%s' deleted successfully!\n", filename);
        LOG_INFO_MSG("CLIENT", "File deleted successfully: %s", filename);
    } else {
        printf("Error: %s\n", response.data);
        LOG_ERROR_MSG("CLIENT", "DELETE failed: %s", response.data);
    }
}

// Phase 4: Handle INFO command
void handle_info_command(command_t cmd, const char* args) { 
    if (args == NULL || strlen(args) == 0) {
        printf("Error: INFO command requires a filename\n");
        printf("Usage: INFO <filename>\n");
        return;
    }
    
    // Prepare request packet
    request_packet_t request;
    memset(&request, 0, sizeof(request));
    request.magic = PROTOCOL_MAGIC;
    request.command = CMD_INFO;
    strncpy(request.username, username, sizeof(request.username) - 1);
    strncpy(request.args, args, sizeof(request.args) - 1);
    request.checksum = calculate_checksum(&request, sizeof(request) - sizeof(uint32_t));
    
    // Send to Name Server
    if (send_packet(nm_socket, &request) < 0) {
        printf("Error: Failed to send INFO request\n");
        return;
    }
    
    // Receive response
    response_packet_t response;
    if (recv_packet(nm_socket, &response) <= 0) {
        printf("Error: No response from Name Server\n");
        return;
    }
    
    // Display result
    if (response.status == STATUS_OK) {
        printf("%s", response.data);
    } else {
        printf("Error: %s\n", response.data);
    }
}

void handle_stream_command(command_t cmd, const char* args) { 
    (void)cmd;
    printf("STREAM command not yet implemented. Args: %s\n", args ? args : "none"); 
}

// Phase 4: Handle LIST command
void handle_list_command(command_t cmd, const char* args) { 
    (void)args;  // LIST doesn't take arguments
    
    // Prepare request packet
    request_packet_t request;
    memset(&request, 0, sizeof(request));
    request.magic = PROTOCOL_MAGIC;
    request.command = CMD_LIST;
    strncpy(request.username, username, sizeof(request.username) - 1);
    request.checksum = calculate_checksum(&request, sizeof(request) - sizeof(uint32_t));
    
    // Send to Name Server
    if (send_packet(nm_socket, &request) < 0) {
        printf("Error: Failed to send LIST request\n");
        return;
    }
    
    // Receive response
    response_packet_t response;
    if (recv_packet(nm_socket, &response) <= 0) {
        printf("Error: No response from Name Server\n");
        return;
    }
    
    // Display result
    if (response.status == STATUS_OK) {
        printf("Connected Users:\n%s", response.data);
    } else {
        printf("Error: %s\n", response.data);
    }
}

// Phase 4: Handle ADDACCESS and REMACCESS commands
void handle_access_command(command_t cmd, const char* args) { 
    if (args == NULL || strlen(args) == 0) {
        printf("Error: Access command requires arguments\n");
        printf("Usage:\n");
        printf("  ADDACCESS -R <filename> <username>  - Grant read access\n");
        printf("  ADDACCESS -W <filename> <username>  - Grant write access\n");
        printf("  REMACCESS <filename> <username>     - Remove access\n");
        return;
    }
    
    // Prepare request packet
    request_packet_t request;
    memset(&request, 0, sizeof(request));
    request.magic = PROTOCOL_MAGIC;
    strncpy(request.username, username, sizeof(request.username) - 1);
    strncpy(request.args, args, sizeof(request.args) - 1);
    
    // Determine which command based on function call context
    char cmd_str[32];
    sscanf(args, "%s", cmd_str);
    
    // Check if it's ADDACCESS or REMACCESS based on first token being a flag
    if (strcmp(cmd_str, "-R") == 0 || strcmp(cmd_str, "-W") == 0) {
        request.command = CMD_ADDACCESS;
    } else {
        request.command = CMD_REMACCESS;
    }
    
    request.checksum = calculate_checksum(&request, sizeof(request) - sizeof(uint32_t));
    
    // Send to Name Server
    if (send_packet(nm_socket, &request) < 0) {
        printf("Error: Failed to send access control request\n");
        return;
    }
    
    // Receive response
    response_packet_t response;
    if (recv_packet(nm_socket, &response) <= 0) {
        printf("Error: No response from Name Server\n");
        return;
    }
    
    // Display result
    if (response.status == STATUS_OK) {
        printf("%s\n", response.data);
    } else {
        printf("Error: %s\n", response.data);
    }
}

void handle_exec_command(command_t cmd, const char* args) { 
    (void)cmd;
    printf("EXEC command not yet implemented. Args: %s\n", args ? args : "none"); 
}

void handle_undo_command(command_t cmd, const char* args) { 
    (void)cmd;
    printf("UNDO command not yet implemented. Args: %s\n", args ? args : "none"); 
}

// Phase 2: Send CLIENT_INIT packet to Name Server
void send_client_init_packet() {
    request_packet_t init_packet;
    memset(&init_packet, 0, sizeof(init_packet));
    
    init_packet.magic = PROTOCOL_MAGIC;
    init_packet.command = CMD_CLIENT_INIT;
    strncpy(init_packet.username, username, sizeof(init_packet.username) - 1);
    init_packet.username[sizeof(init_packet.username) - 1] = '\0';
    
    // Args can contain client info (for now, just empty)
    snprintf(init_packet.args, sizeof(init_packet.args), "client_info");
    init_packet.checksum = calculate_checksum(&init_packet, sizeof(init_packet) - sizeof(uint32_t));
    
    printf("Sending CLIENT_INIT packet for user '%s'...\n", username);
    
    if (send_packet(nm_socket, &init_packet) < 0) {
        perror("Failed to send CLIENT_INIT packet");
        exit(EXIT_FAILURE);
    }
    
    // Wait for response
    response_packet_t response;
    if (recv_packet(nm_socket, &response) <= 0) {
        printf("No response from Name Server\n");
        exit(EXIT_FAILURE);
    }
    
    if (response.status == STATUS_OK) {
        printf("Client initialization successful: %s\n", response.data);
    } else {
        printf("Client initialization failed: %s\n", response.data);
        exit(EXIT_FAILURE);
    }
}

void cleanup_and_exit(int signal) {
    printf("\nReceived signal %d, shutting down client...\n", signal);
    
    if (nm_socket != -1) {
        close(nm_socket);
    }
    
    exit(EXIT_SUCCESS);
}