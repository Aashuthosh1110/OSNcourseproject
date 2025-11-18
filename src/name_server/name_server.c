/*
 * Name Server - Central coordinator for Docs++ system
 * Handles client and storage server registration, file location management,
 * access control, and request routing.
 */

#include "../../include/common.h"
#include "../../include/protocol.h"
#include "../../include/logging.h"
#include "../../include/errors.h"
#include "../../include/nm_state.h"
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>

// Global state - Phase 2: Use linked lists and hash table
static ss_node_t* storage_servers_list = NULL;
static client_node_t* clients_list = NULL;
static file_hash_table_t file_table;
static int server_socket = -1;
static fd_set* global_master_fds = NULL;  // Global reference to master_fds

// Function prototypes
void handle_client_registration(int client_socket);
void handle_storage_server_registration(int ss_socket);
void handle_client_request(int client_socket);
void route_file_operation(int client_socket, request_packet_t* request);
void cleanup_and_exit(int signal);
void initialize_server(int port);
int find_file_location(const char* filename);
int find_available_storage_server();

// Phase 2: Initialization handlers
void handle_ss_init(int sockfd, request_packet_t* req);
void handle_client_init(int sockfd, request_packet_t* req);
void process_connection_data(int sockfd);
void init_name_server_state();

// Phase 3: File operation handlers
void handle_create_file(int sockfd, request_packet_t* req);
void handle_delete_file(int sockfd, request_packet_t* req);
ss_node_t* select_storage_server_for_create();

// Phase 4: User functionality handlers
void handle_list_users(int sockfd, request_packet_t* req);
void handle_view_files(int sockfd, request_packet_t* req);
void handle_info_file(int sockfd, request_packet_t* req);
void handle_addaccess(int sockfd, request_packet_t* req);
void handle_remaccess(int sockfd, request_packet_t* req);
int check_user_has_access(file_hash_entry_t* entry, const char* username, int access_type);

// Phase 5.2: File operation handlers
void handle_read_file(int sockfd, request_packet_t* req);
void handle_stream_file(int sockfd, request_packet_t* req);

// Phase 5.3: Write and undo handlers
void handle_write_file(int sockfd, request_packet_t* req);
void handle_undo_file(int sockfd, request_packet_t* req);

// Phase 5.4: Exec handler
void handle_exec_command(int sockfd, request_packet_t* req);

// Scan existing files in storage directories and add them to registry
void scan_storage_files() {
    LOG_INFO_MSG("NAME_SERVER", "Scanning for existing files in storage directories");
    
    // For now, we'll scan the main storage directory
    // In a real implementation, this would scan all registered storage server directories
    const char* storage_dir = "storage";
    
    DIR* dir = opendir(storage_dir);
    if (dir == NULL) {
        LOG_WARNING_MSG("NAME_SERVER", "Could not open storage directory: %s", storage_dir);
        return;
    }
    
    struct dirent* entry;
    int files_found = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip directories and hidden files
        if (entry->d_name[0] == '.' || entry->d_type == DT_DIR) {
            continue;
        }
        
        // Skip .meta files, .bak files, and other auxiliary files
        char* ext = strrchr(entry->d_name, '.');
        if (ext && (strcmp(ext, ".meta") == 0 || strcmp(ext, ".bak") == 0)) {
            continue;
        }
        
        // Check if we already have this file in the registry
        if (find_file_in_table(&file_table, entry->d_name) != NULL) {
            continue; // Already registered
        }
        
        // Check if corresponding .meta file exists
        char meta_path[512];
        snprintf(meta_path, sizeof(meta_path), "%s/%s.meta", storage_dir, entry->d_name);
        
        FILE* meta_file = fopen(meta_path, "r");
        if (meta_file == NULL) {
            LOG_WARNING_MSG("NAME_SERVER", "No metadata file found for %s, skipping", entry->d_name);
            continue;
        }
        
        // Create new file entry
        file_hash_entry_t* new_entry = (file_hash_entry_t*)malloc(sizeof(file_hash_entry_t));
        if (new_entry == NULL) {
            LOG_ERROR_MSG("NAME_SERVER", "Memory allocation failed for file entry");
            fclose(meta_file);
            continue;
        }
        
        // Initialize entry
        memset(new_entry, 0, sizeof(file_hash_entry_t));
        strncpy(new_entry->metadata.filename, entry->d_name, MAX_FILENAME_LEN - 1);
        
        // Read metadata from .meta file
        char line[256];
        while (fgets(line, sizeof(line), meta_file) != NULL) {
            if (strncmp(line, "owner=", 6) == 0) {
                sscanf(line + 6, "%s", new_entry->metadata.owner);
            } else if (strncmp(line, "word_count=", 11) == 0) {
                sscanf(line + 11, "%d", &new_entry->metadata.word_count);
            } else if (strncmp(line, "char_count=", 11) == 0) {
                sscanf(line + 11, "%d", &new_entry->metadata.char_count);
            } else if (strncmp(line, "size=", 5) == 0) {
                sscanf(line + 5, "%zu", &new_entry->metadata.size);
            } else if (strncmp(line, "created=", 8) == 0) {
                // Parse timestamp if needed - for now set to current time
                new_entry->metadata.created = time(NULL);
                new_entry->metadata.last_accessed = time(NULL);
            }
        }
        fclose(meta_file);
        
        // Set default values if not found in metadata
        if (new_entry->metadata.owner[0] == '\0') {
            strcpy(new_entry->metadata.owner, "Unknown");
        }
        
        // Add to hash table (use -1 for ss_socket_fd since we don't know the storage server yet)
        if (add_file_to_table(&file_table, entry->d_name, -1, &new_entry->metadata) == 0) {
            files_found++;
            LOG_INFO_MSG("NAME_SERVER", "Added existing file to registry: %s (owner: %s)", 
                        entry->d_name, new_entry->metadata.owner);
            free(new_entry); // The function creates its own copy
        } else {
            LOG_ERROR_MSG("NAME_SERVER", "Failed to add file to registry: %s", entry->d_name);
            free(new_entry);
        }
    }
    
    closedir(dir);
    LOG_INFO_MSG("NAME_SERVER", "Scanned storage: found %d existing files", files_found);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Error: Invalid port number\n");
        exit(EXIT_FAILURE);
    }
    
    printf("Name Server starting on port %d...\n", port);
    
    // Phase 6: Initialize logging
    init_logging("logs/name_server.log", LOG_INFO, 1);
    LOG_INFO_MSG("NAME_SERVER", "Starting Name Server on port %d", port);
    
    // Set up signal handlers
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);
    
    // Phase 2: Initialize state management
    init_name_server_state();
    
    // Scan for existing files in storage
    scan_storage_files();
    
    // Initialize server
    initialize_server(port);
    
    // Main server loop with select()
    fd_set master_fds, read_fds;
    FD_ZERO(&master_fds);
    FD_SET(server_socket, &master_fds);
    int max_fd = server_socket;
    
    // Make master_fds available globally
    global_master_fds = &master_fds;
    
    printf("Name Server listening on port %d, waiting for connections...\n", port);
    
    while (1) {
        read_fds = master_fds;
        
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("Select error");
            continue;
        }
        
        // Check for new connections
        if (FD_ISSET(server_socket, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int new_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
            
            if (new_socket == -1) {
                perror("Accept error");
                continue;
            }
            
            // Add new socket to our set
            FD_SET(new_socket, &master_fds);
            if (new_socket > max_fd) {
                max_fd = new_socket;
            }
            
            // Phase 1: Just print connection received
            printf("New connection received from %s:%d (fd=%d)\n", 
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), new_socket);
        }
        
        // Handle existing connections (Phase 2: process initialization data)
        for (int fd = 0; fd <= max_fd; fd++) {
            if (FD_ISSET(fd, &read_fds) && fd != server_socket) {
                process_connection_data(fd);
                
                // Check if connection is still valid
                if (FD_ISSET(fd, &master_fds)) {
                    // Connection is still active after processing
                } else {
                    // Connection was closed during processing
                    if (fd == max_fd) {
                        // Recalculate max_fd if needed
                        while (max_fd > server_socket && !FD_ISSET(max_fd, &master_fds)) {
                            max_fd--;
                        }
                    }
                }
            }
        }
    }
    
    return 0;
}

void initialize_server(int port) {
    // Create TCP socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
    }
    
    // Set up server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    // Bind socket to address
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Start listening (backlog of 10 connections)
    if (listen(server_socket, 10) == -1) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("TCP socket initialized successfully on port %d\n", port);
}

void handle_client_registration(int client_socket) {
    LOG_INFO_MSG("NAME_SERVER", "Handling client registration for socket %d", client_socket);
    
    // Send success response for now
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    response.status = STATUS_OK;
    snprintf(response.data, sizeof(response.data), "Client registration successful");
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    
    if (send_response(client_socket, &response) < 0) {
        LOG_ERROR_MSG("NAME_SERVER", "Failed to send registration response");
    }
    
    // TODO: Implement actual client registration logic
}

void handle_storage_server_registration(int ss_socket) {
    LOG_INFO_MSG("NAME_SERVER", "Handling storage server registration for socket %d", ss_socket);
    
    // Send success response for now
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    response.status = STATUS_OK;
    snprintf(response.data, sizeof(response.data), "Storage server registration successful");
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    
    if (send_response(ss_socket, &response) < 0) {
        LOG_ERROR_MSG("NAME_SERVER", "Failed to send registration response");
    }
    
    // TODO: Implement actual storage server registration logic
}

void handle_client_request(int client_socket) {
    LOG_INFO_MSG("NAME_SERVER", "Handling client request for socket %d", client_socket);
    
    // Send a default response for now
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    response.status = STATUS_ERROR_INVALID_OPERATION;
    snprintf(response.data, sizeof(response.data), "Request handling not yet implemented");
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    
    if (send_response(client_socket, &response) < 0) {
        LOG_ERROR_MSG("NAME_SERVER", "Failed to send response");
    }
    
    // TODO: Implement actual client request handling logic
}

void route_file_operation(int client_socket, request_packet_t* request) {
    LOG_INFO_MSG("NAME_SERVER", "Routing file operation: command=%d", request->command);
    
    // Send a default response for now
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    response.status = STATUS_ERROR_INVALID_OPERATION;
    snprintf(response.data, sizeof(response.data), "File operation routing not yet implemented");
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    
    if (send_response(client_socket, &response) < 0) {
        LOG_ERROR_MSG("NAME_SERVER", "Failed to send routing response");
    }
    
    // TODO: Implement actual file operation routing logic
}

int find_file_location(const char* filename) {
    // Implementation for finding file location
    // TODO: Implement efficient file location search
    return -1;
}

int find_available_storage_server() {
    // Implementation for finding available storage server
    // TODO: Implement load balancing algorithm
    return -1;
}

// Phase 2: Initialize Name Server state
void init_name_server_state() {
    storage_servers_list = NULL;
    clients_list = NULL;
    init_file_hash_table(&file_table);
    
    // Load persistent user registry
    load_user_registry(&clients_list);
    
    printf("Name Server state initialized:\n");
    printf("  - Storage servers list: ready\n");
    printf("  - Clients list: ready\n");
    printf("  - File hash table: %d buckets\n", HASH_TABLE_SIZE);
    printf("  - Loaded %d users from registry\n", count_all_users(clients_list));
}

// Phase 2: Process data from connected clients/servers
void process_connection_data(int sockfd) {
    request_packet_t request;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char client_ip[INET_ADDRSTRLEN];
    int client_port = 0;
    
    // Get client address information for logging
    if (getpeername(sockfd, (struct sockaddr*)&client_addr, &addr_len) == 0) {
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        client_port = ntohs(client_addr.sin_port);
    } else {
        strcpy(client_ip, "unknown");
        client_port = 0;
    }
    
    int bytes = recv_request(sockfd, &request);
    
    if (bytes <= 0) {
        // Connection closed or error
        printf("[NM] Connection closed: %s:%d (fd=%d)\n", client_ip, client_port, sockfd);
        LOG_INFO_MSG("CONNECTION", "Connection closed: %s:%d (fd=%d)", client_ip, client_port, sockfd);
        
        // Clean up from our data structures
        remove_storage_server(&storage_servers_list, sockfd);
        
        // For clients, mark as disconnected but keep in registry
        disconnect_user(clients_list, sockfd);
        
        close(sockfd);
        FD_CLR(sockfd, global_master_fds);
        return;
    }
    
    // Validate packet integrity
    if (!validate_packet_integrity(&request, sizeof(request))) {
        printf("[NM] Received corrupted packet from %s:%d (fd=%d)\n", client_ip, client_port, sockfd);
        LOG_ERROR_MSG("PACKET", "Corrupted packet from %s:%d (fd=%d)", client_ip, client_port, sockfd);
        return;
    }
    
    // Phase 6: Log incoming request
    const char* cmd_name = "UNKNOWN";
    switch (request.command) {
        case CMD_CREATE: cmd_name = "CREATE"; break;
        case CMD_DELETE: cmd_name = "DELETE"; break;
        case CMD_READ: cmd_name = "READ"; break;
        case CMD_WRITE: cmd_name = "WRITE"; break;
        case CMD_STREAM: cmd_name = "STREAM"; break;
        case CMD_UNDO: cmd_name = "UNDO"; break;
        case CMD_EXEC: cmd_name = "EXEC"; break;
        case CMD_LIST: cmd_name = "LIST"; break;
        case CMD_VIEW: cmd_name = "VIEW"; break;
        case CMD_INFO: cmd_name = "INFO"; break;
        case CMD_ADDACCESS: cmd_name = "ADDACCESS"; break;
        case CMD_REMACCESS: cmd_name = "REMACCESS"; break;
        case CMD_SS_INIT: cmd_name = "SS_INIT"; break;
        case CMD_CLIENT_INIT: cmd_name = "CLIENT_INIT"; break;
        default: break;
    }
    // Log and display the incoming request
    printf("[NM] REQUEST from %s@%s:%d | Command: %s | Args: %s\n", 
           request.username, client_ip, client_port, cmd_name, request.args);
    LOG_INFO_MSG("REQUEST", "From %s@%s:%d (fd=%d) | Command: %s | Args: %s", 
                 request.username, client_ip, client_port, sockfd, cmd_name, request.args);
    
    // Handle different initialization commands
    switch (request.command) {
        case CMD_SS_INIT:
            handle_ss_init(sockfd, &request);
            break;
        case CMD_CLIENT_INIT:
            handle_client_init(sockfd, &request);
            break;
        case CMD_CREATE:
            handle_create_file(sockfd, &request);
            break;
        case CMD_DELETE:
            handle_delete_file(sockfd, &request);
            break;
        case CMD_READ:
            handle_read_file(sockfd, &request);
            break;
        case CMD_STREAM:
            handle_stream_file(sockfd, &request);
            break;
        case CMD_WRITE:
            handle_write_file(sockfd, &request);
            break;
        case CMD_UNDO:
            handle_undo_file(sockfd, &request);
            break;
        case CMD_EXEC:
            handle_exec_command(sockfd, &request);
            break;
        case CMD_LIST:
            handle_list_users(sockfd, &request);
            break;
        case CMD_VIEW:
            handle_view_files(sockfd, &request);
            break;
        case CMD_INFO:
            handle_info_file(sockfd, &request);
            break;
        case CMD_ADDACCESS:
            handle_addaccess(sockfd, &request);
            break;
        case CMD_REMACCESS:
            handle_remaccess(sockfd, &request);
            break;
        case CMD_REGISTER_SS:
        case CMD_REGISTER_CLIENT:
            // Legacy commands - redirect to init handlers
            if (request.command == CMD_REGISTER_SS) {
                request.command = CMD_SS_INIT;
                handle_ss_init(sockfd, &request);
            } else {
                request.command = CMD_CLIENT_INIT;
                handle_client_init(sockfd, &request);
            }
            break;
        default:
            printf("Unknown command %d from fd=%d\n", request.command, sockfd);
            
            // Send error response
            response_packet_t error_response;
            memset(&error_response, 0, sizeof(error_response));
            error_response.magic = PROTOCOL_MAGIC;
            error_response.status = STATUS_ERROR_INVALID_OPERATION;
            snprintf(error_response.data, sizeof(error_response.data), 
                    "Unknown command: %d", request.command);
            error_response.checksum = calculate_checksum(&error_response, 
                                                         sizeof(error_response) - sizeof(uint32_t));
            send_response(sockfd, &error_response);
            break;
    }
}

// Phase 2: Handle Storage Server initialization
void handle_ss_init(int sockfd, request_packet_t* req) {
    printf("Processing SS_INIT from fd=%d\n", sockfd);
    
    // Parse SS info from request
    storage_server_info_t ss_info;
    memset(&ss_info, 0, sizeof(ss_info));
    
    // Parse IP and port from args (format: "IP:PORT:FILE1,FILE2,FILE3...")
    char args_copy[1024];
    strncpy(args_copy, req->args, sizeof(args_copy) - 1);
    args_copy[sizeof(args_copy) - 1] = '\0';
    
    char* ip_str = strtok(args_copy, ":");
    char* port_str = strtok(NULL, ":");
    char* files_str = strtok(NULL, ":");
    
    if (ip_str && port_str) {
        strncpy(ss_info.ip, ip_str, sizeof(ss_info.ip) - 1);
        ss_info.client_port = atoi(port_str);
        ss_info.active = 1;
        ss_info.last_heartbeat = time(NULL);
        
        // Parse file list
        ss_info.file_count = 0;
        if (files_str && strlen(files_str) > 0) {
            char* file_token = strtok(files_str, ",");
            while (file_token != NULL && ss_info.file_count < MAX_FILES_PER_SERVER) {
                strncpy(ss_info.files[ss_info.file_count], file_token, MAX_FILENAME_LEN - 1);
                ss_info.files[ss_info.file_count][MAX_FILENAME_LEN - 1] = '\0';
                
                // Add to file hash table
                add_file_to_table(&file_table, file_token, sockfd, NULL);
                
                ss_info.file_count++;
                file_token = strtok(NULL, ",");
            }
        }
        
        // Add to storage servers list
        ss_node_t* new_ss = add_storage_server(&storage_servers_list, &ss_info, sockfd);
        if (new_ss) {
            printf("Registered SS from %s:%d with %d files (fd=%d)\n", 
                   ss_info.ip, ss_info.client_port, ss_info.file_count, sockfd);
            
            // Send success response
            response_packet_t response;
            memset(&response, 0, sizeof(response));
            response.magic = PROTOCOL_MAGIC;
            response.status = STATUS_OK;
            snprintf(response.data, sizeof(response.data), 
                    "SS registered: %d files", ss_info.file_count);
            response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
            
            send_response(sockfd, &response);
        } else {
            printf("Failed to register SS from fd=%d\n", sockfd);
        }
    } else {
        printf("Invalid SS_INIT format from fd=%d\n", sockfd);
    }
}

// Phase 2: Handle Client initialization
void handle_client_init(int sockfd, request_packet_t* req) {
    printf("Processing CLIENT_INIT from fd=%d\n", sockfd);
    
    // Parse client info
    user_info_t user_info;
    memset(&user_info, 0, sizeof(user_info));
    
    strncpy(user_info.username, req->username, sizeof(user_info.username) - 1);
    user_info.username[sizeof(user_info.username) - 1] = '\0';
    user_info.socket_fd = sockfd;
    user_info.active = 1;
    user_info.connected_time = time(NULL);
    
    // Get client IP from socket
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    if (getpeername(sockfd, (struct sockaddr*)&client_addr, &addr_len) == 0) {
        inet_ntop(AF_INET, &client_addr.sin_addr, user_info.client_ip, INET_ADDRSTRLEN);
    }
    
    // Add to clients list using persistent user management
    time_t original_time = user_info.connected_time;
    client_node_t* client = register_or_reconnect_user(&clients_list, &user_info);
    if (client) {
        int is_reconnect = (client->data.connected_time != original_time);
        
        printf("[NM] User '%s' %s from %s (fd=%d)\n", 
               user_info.username, 
               is_reconnect ? "reconnected" : "registered",
               user_info.client_ip, sockfd);
        
        // Send success response with appropriate message
        response_packet_t response;
        memset(&response, 0, sizeof(response));
        response.magic = PROTOCOL_MAGIC;
        response.status = STATUS_OK;
        
        if (is_reconnect) {
            snprintf(response.data, sizeof(response.data), 
                    "Welcome back %s! Reconnected to Docs++", user_info.username);
        } else {
            snprintf(response.data, sizeof(response.data), 
                    "Welcome %s! Connected to Docs++", user_info.username);
        }
        
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(sockfd, &response);
    } else {
        printf("[NM] Failed to register/reconnect client from fd=%d\n", sockfd);
    }
}

void cleanup_and_exit(int signal) {
    printf("\nReceived signal %d, shutting down gracefully...\n", signal);
    
    if (server_socket != -1) {
        close(server_socket);
    }
    
    // TODO: Clean up linked lists and hash table
    
    exit(EXIT_SUCCESS);
}

// Phase 3: Handle CREATE file request from client
void handle_create_file(int client_fd, request_packet_t* req) {
    LOG_INFO_MSG("NAME_SERVER", "Handling CREATE request for file: %s by user: %s", 
                 req->args, req->username);
    
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    
    // Extract filename from args
    char filename[MAX_FILENAME_LEN];
    sscanf(req->args, "%s", filename);
    
    // Validate filename
    if (!validate_filename(filename)) {
        response.status = STATUS_ERROR_INVALID_FILENAME;
        snprintf(response.data, sizeof(response.data), 
                "Invalid filename: %s", filename);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "Invalid filename: %s", filename);
        return;
    }
    
    // Step 1: Check if file already exists
    file_hash_entry_t* existing = find_file_in_table(&file_table, filename);
    if (existing != NULL) {
        response.status = STATUS_ERROR_FILE_EXISTS;
        snprintf(response.data, sizeof(response.data), 
                "File '%s' already exists", filename);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        
        // Log response
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        char client_ip[INET_ADDRSTRLEN];
        int client_port = 0;
        
        if (getpeername(client_fd, (struct sockaddr*)&client_addr, &addr_len) == 0) {
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
            client_port = ntohs(client_addr.sin_port);
        } else {
            strcpy(client_ip, "unknown");
        }
        
        printf("[NM] RESPONSE to %s@%s:%d | Command: CREATE | Status: ERROR | File: %s | Message: File already exists\n", 
               req->username, client_ip, client_port, filename);
        LOG_WARNING_MSG("RESPONSE", "To %s@%s:%d (fd=%d) | Command: CREATE | Status: ERROR | File: %s | Message: %s", 
                        req->username, client_ip, client_port, client_fd, filename, response.data);
        
        send_response(client_fd, &response);
        return;
    }
    
    // Step 2: Select a storage server (round-robin/least-loaded)
    ss_node_t* selected_ss = select_storage_server_for_create();
    if (selected_ss == NULL) {
        response.status = STATUS_ERROR_SERVER_UNAVAILABLE;
        snprintf(response.data, sizeof(response.data), 
                "No storage servers available");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "No storage servers available for CREATE");
        return;
    }
    
    LOG_INFO_MSG("NAME_SERVER", "Selected SS at %s:%d for file creation", 
                selected_ss->data.ip, selected_ss->data.client_port);
    
    // Step 3: Forward CREATE request to selected storage server
    request_packet_t ss_request;
    memset(&ss_request, 0, sizeof(ss_request));
    ss_request.magic = PROTOCOL_MAGIC;
    ss_request.command = CMD_CREATE;
    strncpy(ss_request.username, req->username, sizeof(ss_request.username) - 1);
    snprintf(ss_request.args, sizeof(ss_request.args), "%s", filename);
    ss_request.checksum = calculate_checksum(&ss_request, sizeof(ss_request) - sizeof(uint32_t));
    
    if (send_packet(selected_ss->socket_fd, &ss_request) < 0) {
        response.status = STATUS_ERROR_NETWORK;
        snprintf(response.data, sizeof(response.data), 
                "Failed to communicate with storage server");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "Failed to send CREATE to SS");
        return;
    }
    
    // Step 4: Wait for acknowledgment from storage server
    response_packet_t ss_response;
    int result = recv_packet(selected_ss->socket_fd, &ss_response);
    if (result <= 0) {
        response.status = STATUS_ERROR_NETWORK;
        snprintf(response.data, sizeof(response.data), 
                "Storage server did not respond");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "No response from SS for CREATE");
        return;
    }
    
    // Step 5: Check SS response
    if (ss_response.status != STATUS_OK) {
        // Forward SS error to client
        response.status = ss_response.status;
        snprintf(response.data, sizeof(response.data), "%s", ss_response.data);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "SS failed to create file: %s", ss_response.data);
        return;
    }
    
    // Step 6: Add file to Name Server's registry
    file_metadata_t metadata;
    memset(&metadata, 0, sizeof(metadata));
    strncpy(metadata.filename, filename, sizeof(metadata.filename) - 1);
    strncpy(metadata.owner, req->username, sizeof(metadata.owner) - 1);
    metadata.created = time(NULL);
    metadata.last_modified = metadata.created;
    metadata.last_accessed = metadata.created;
    strncpy(metadata.last_accessed_by, req->username, sizeof(metadata.last_accessed_by) - 1);
    metadata.size = 0;
    metadata.word_count = 0;
    metadata.char_count = 0;
    
    // Owner gets read/write access
    metadata.access_count = 1;
    strncpy(metadata.access_list[0], req->username, MAX_USERNAME_LEN - 1);
    metadata.access_permissions[0] = ACCESS_BOTH;
    
    if (add_file_to_table(&file_table, filename, selected_ss->socket_fd, &metadata) < 0) {
        LOG_ERROR_MSG("NAME_SERVER", "Failed to add file to registry");
        // File created on SS but not in registry - warn but continue
    }
    
    LOG_INFO_MSG("NAME_SERVER", "File '%s' created successfully by user '%s'", 
                filename, req->username);
    
    // Step 7: Send success response to client
    response.status = STATUS_OK;
    snprintf(response.data, sizeof(response.data), 
            "File created successfully");
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    
    // Get client address for logging
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char client_ip[INET_ADDRSTRLEN];
    int client_port = 0;
    
    if (getpeername(client_fd, (struct sockaddr*)&client_addr, &addr_len) == 0) {
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        client_port = ntohs(client_addr.sin_port);
    } else {
        strcpy(client_ip, "unknown");
    }
    
    printf("[NM] RESPONSE to %s@%s:%d | Command: CREATE | Status: SUCCESS | File: %s\n", 
           req->username, client_ip, client_port, filename);
    LOG_INFO_MSG("RESPONSE", "To %s@%s:%d (fd=%d) | Command: CREATE | Status: SUCCESS | File: %s | Message: %s", 
                 req->username, client_ip, client_port, client_fd, filename, response.data);
    
    send_response(client_fd, &response);
}

// Phase 3: Handle DELETE file request from client
void handle_delete_file(int client_fd, request_packet_t* req) {
    LOG_INFO_MSG("NAME_SERVER", "Handling DELETE request for file: %s by user: %s", 
                 req->args, req->username);
    
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    
    // Extract filename from args
    char filename[MAX_FILENAME_LEN];
    sscanf(req->args, "%s", filename);
    
    // Step 1: Find file in registry
    file_hash_entry_t* file_entry = find_file_in_table(&file_table, filename);
    if (file_entry == NULL) {
        response.status = STATUS_ERROR_NOT_FOUND;
        snprintf(response.data, sizeof(response.data), 
                "File '%s' not found", filename);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "File not found: %s", filename);
        return;
    }
    
    // Step 2: Get storage server holding the file (ownership check delegated to SS)
    int ss_fd = file_entry->ss_socket_fd;
    ss_node_t* ss = find_storage_server_by_fd(storage_servers_list, ss_fd);
    if (ss == NULL) {
        response.status = STATUS_ERROR_SERVER_UNAVAILABLE;
        snprintf(response.data, sizeof(response.data), 
                "Storage server not available");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "Storage server not found for file: %s", filename);
        return;
    }
    
    // Step 4: Forward DELETE request to storage server
    request_packet_t ss_request;
    memset(&ss_request, 0, sizeof(ss_request));
    ss_request.magic = PROTOCOL_MAGIC;
    ss_request.command = CMD_DELETE;
    strncpy(ss_request.username, req->username, sizeof(ss_request.username) - 1);
    snprintf(ss_request.args, sizeof(ss_request.args), "%s", filename);
    ss_request.checksum = calculate_checksum(&ss_request, sizeof(ss_request) - sizeof(uint32_t));
    
    if (send_packet(ss_fd, &ss_request) < 0) {
        response.status = STATUS_ERROR_NETWORK;
        snprintf(response.data, sizeof(response.data), 
                "Failed to communicate with storage server");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "Failed to send DELETE to SS");
        return;
    }
    
    // Step 5: Wait for acknowledgment from storage server
    response_packet_t ss_response;
    int result = recv_packet(ss_fd, &ss_response);
    if (result <= 0) {
        response.status = STATUS_ERROR_NETWORK;
        snprintf(response.data, sizeof(response.data), 
                "Storage server did not respond");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "No response from SS for DELETE");
        return;
    }
    
    // Step 6: Check SS response
    if (ss_response.status != STATUS_OK) {
        // Forward SS error to client
        response.status = ss_response.status;
        snprintf(response.data, sizeof(response.data), "%s", ss_response.data);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "SS failed to delete file: %s", ss_response.data);
        return;
    }
    
    // Step 7: Remove file from LRU cache FIRST (before freeing hash table entry)
    remove_file_from_lru_cache(filename);
    
    // Step 8: Remove file from Name Server's registry
    if (remove_file_from_table(&file_table, filename) < 0) {
        LOG_ERROR_MSG("NAME_SERVER", "Failed to remove file from registry");
        // File deleted on SS but still in registry - warn but continue
    }
    
    LOG_INFO_MSG("NAME_SERVER", "File '%s' deleted successfully by user '%s'", 
                filename, req->username);
    
    // Step 9: Send success response to client
    response.status = STATUS_OK;
    snprintf(response.data, sizeof(response.data), 
            "File deleted successfully");
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    send_response(client_fd, &response);
}

// Phase 3: Select storage server for file creation (round-robin)
ss_node_t* select_storage_server_for_create() {
    static int last_selected_index = -1;
    
    if (storage_servers_list == NULL) {
        return NULL;
    }
    
    int total_ss = count_storage_servers(storage_servers_list);
    if (total_ss == 0) {
        return NULL;
    }
    
    // Round-robin selection
    last_selected_index = (last_selected_index + 1) % total_ss;
    
    // Find the SS at this index
    ss_node_t* current = storage_servers_list;
    for (int i = 0; i < last_selected_index && current != NULL; i++) {
        current = current->next;
    }
    
    return current;
}

// ===========================
// Phase 4: User Functionality Handlers
// ===========================

// Phase 4: Handle LIST command - List all connected users
void handle_list_users(int client_fd, request_packet_t* req) {
    LOG_INFO_MSG("NAME_SERVER", "Handling LIST request from user: %s", req->username);
    
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    
    char user_list[MAX_RESPONSE_DATA_LEN];
    int offset = 0;
    int count = 0;
    
    // Iterate through all registered users (both active and inactive)
    client_node_t* current = clients_list;
    while (current && offset < MAX_RESPONSE_DATA_LEN - 200) {
        char time_str[64];
        struct tm* timeinfo = localtime(&current->data.connected_time);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);
        
        const char* status = current->data.active ? "ONLINE" : "OFFLINE";
        
        int written = snprintf(user_list + offset, MAX_RESPONSE_DATA_LEN - offset,
                              "%d. %s [%s] (last seen from %s at %s)\n",
                              ++count,
                              current->data.username,
                              status,
                              current->data.client_ip,
                              time_str);
        offset += written;
        current = current->next;
    }
    
    if (count == 0) {
        strcpy(user_list, "No users currently connected.\n");
    }
    
    response.status = STATUS_OK;
    strncpy(response.data, user_list, sizeof(response.data) - 1);
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    send_response(client_fd, &response);
    
    LOG_INFO_MSG("NAME_SERVER", "Sent list of %d users to client", count);
}

// Phase 4: Check if user has access to file
int check_user_has_access(file_hash_entry_t* entry, const char* username, int access_type) {
    // Owner always has full access
    if (strcmp(entry->metadata.owner, username) == 0) {
        return 1;
    }
    
    // Check ACL
    for (int i = 0; i < entry->metadata.access_count; i++) {
        if (strcmp(entry->metadata.access_list[i], username) == 0) {
            int perms = entry->metadata.access_permissions[i];
            if (access_type == ACCESS_READ && (perms & ACCESS_READ)) {
                return 1;
            } else if (access_type == ACCESS_WRITE && (perms & ACCESS_WRITE)) {
                return 1;
            } else if (access_type == ACCESS_BOTH && (perms & ACCESS_BOTH) == ACCESS_BOTH) {
                return 1;
            }
        }
    }
    
    return 0;  // No access
}

// Phase 4: Handle VIEW command - List files
void handle_view_files(int client_fd, request_packet_t* req) {
    LOG_INFO_MSG("NAME_SERVER", "Handling VIEW request from user: %s", req->username);
    
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    
    // Parse flags from args using the robust parser
    int flag_all = 0;
    int flag_long = 0;
    
    parse_view_args(req->args, &flag_all, &flag_long);
    
    char file_list[MAX_RESPONSE_DATA_LEN];
    int offset = 0;
    int file_count = 0;
    
    // Header
    if (flag_long) {
        offset += snprintf(file_list + offset, MAX_RESPONSE_DATA_LEN - offset,
                          "-------------------------------------------------------------------------------------------------------------------------\n");
        offset += snprintf(file_list + offset, MAX_RESPONSE_DATA_LEN - offset,
                          "| %-8s | %-6s | %-6s | %-16s | %-8s | %-5s | %-12s |\n",
                          "Size", "Words", "Chars", "Last Access", "Owner", "Perms", "Filename");
        offset += snprintf(file_list + offset, MAX_RESPONSE_DATA_LEN - offset,
                          "|----------|--------|--------|------------------|----------|-------|--------------|\n");
    }
    // No header for regular view - show files directly
    
    // Iterate through all files in hash table
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        file_hash_entry_t* current = file_table.buckets[i];
        while (current && offset < MAX_RESPONSE_DATA_LEN - 100) {
            // Validate entry has proper filename before processing
            if (!current->metadata.filename[0] || strlen(current->metadata.filename) == 0) {
                current = current->next;
                continue;
            }
            
            // Check access
            int has_access = flag_all || check_user_has_access(current, req->username, ACCESS_READ);
            
            if (has_access) {
                if (flag_long) {
                    // Format last access time
                    char time_str[20];
                    if (current->metadata.last_accessed > 0) {
                        struct tm* tm_info = localtime(&current->metadata.last_accessed);
                        if (tm_info) {
                            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", tm_info);
                        } else {
                            strcpy(time_str, "Never");
                        }
                    } else {
                        strcpy(time_str, "Never");
                    }
                    
                    // Ensure owner is not empty
                    const char* owner = (current->metadata.owner[0] != '\0') ? current->metadata.owner : "Unknown";
                    
                    // Determine permissions for current user
                    char perms[6] = "-";
                    if (strcmp(current->metadata.owner, req->username) == 0) {
                        strcpy(perms, "RW");  // Owner has read+write
                    } else {
                        // Check ACL for permissions
                        int user_perms = 0;
                        for (int j = 0; j < current->metadata.access_count; j++) {
                            if (strcmp(current->metadata.access_list[j], req->username) == 0) {
                                user_perms = current->metadata.access_permissions[j];
                                break;
                            }
                        }
                        if (user_perms & ACCESS_WRITE) {
                            strcpy(perms, "RW");
                        } else if (user_perms & ACCESS_READ) {
                            strcpy(perms, "R");
                        } else {
                            strcpy(perms, "-");
                        }
                    }
                    
                    offset += snprintf(file_list + offset, MAX_RESPONSE_DATA_LEN - offset,
                                      "| %8zu | %6d | %6d | %-16s | %-8s | %-5s | %-12s |\n",
                                      current->metadata.size,
                                      current->metadata.word_count,
                                      current->metadata.char_count,
                                      time_str,
                                      owner,
                                      perms,
                                      current->metadata.filename);
                } else {
                    offset += snprintf(file_list + offset, MAX_RESPONSE_DATA_LEN - offset,
                                      "--> %s\n", current->metadata.filename);
                }
                file_count++;
            }
            
            current = current->next;
        }
    }
    
    if (file_count == 0) {
        if (flag_all) {
            strcpy(file_list, "No files exist in the system.\n");
        } else {
            snprintf(file_list, MAX_RESPONSE_DATA_LEN, 
                    "No files accessible to user '%s'.\n", req->username);
        }
    } else if (flag_long) {
        // Add closing line for table
        offset += snprintf(file_list + offset, MAX_RESPONSE_DATA_LEN - offset,
                          "-------------------------------------------------------------------------------------------------------------------------\n");
    }
    
    response.status = STATUS_OK;
    strncpy(response.data, file_list, sizeof(response.data) - 1);
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    send_response(client_fd, &response);
    
    LOG_INFO_MSG("NAME_SERVER", "Sent list of %d files to client", file_count);
}

// Phase 4: Handle INFO command - Show file metadata
void handle_info_file(int client_fd, request_packet_t* req) {
    LOG_INFO_MSG("NAME_SERVER", "Handling INFO request for file: %s by user: %s",
                 req->args, req->username);
    
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    
    // Extract filename from args
    char filename[MAX_FILENAME_LEN];
    sscanf(req->args, "%s", filename);
    
    // Find file in registry
    file_hash_entry_t* file_entry = find_file_in_table(&file_table, filename);
    if (file_entry == NULL) {
        response.status = STATUS_ERROR_NOT_FOUND;
        snprintf(response.data, sizeof(response.data), "File '%s' not found", filename);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "File not found: %s", filename);
        return;
    }
    
    // Check access (must have at least read permission)
    if (!check_user_has_access(file_entry, req->username, ACCESS_READ)) {
        response.status = STATUS_ERROR_READ_PERMISSION;
        snprintf(response.data, sizeof(response.data), "Permission denied");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "User '%s' denied access to file '%s'",
                       req->username, filename);
        return;
    }
    
    // Format file information
    char info[MAX_RESPONSE_DATA_LEN];
    int offset = 0;
    
    char created_str[64], modified_str[64], accessed_str[64];
    struct tm* timeinfo;
    
    timeinfo = localtime(&file_entry->metadata.created);
    strftime(created_str, sizeof(created_str), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    timeinfo = localtime(&file_entry->metadata.last_modified);
    strftime(modified_str, sizeof(modified_str), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    timeinfo = localtime(&file_entry->metadata.last_accessed);
    strftime(accessed_str, sizeof(accessed_str), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    offset += snprintf(info + offset, MAX_RESPONSE_DATA_LEN - offset,
                      "File Information:\n");
    offset += snprintf(info + offset, MAX_RESPONSE_DATA_LEN - offset,
                      "  Name: %s\n", file_entry->metadata.filename);
    offset += snprintf(info + offset, MAX_RESPONSE_DATA_LEN - offset,
                      "  Owner: %s\n", file_entry->metadata.owner);
    offset += snprintf(info + offset, MAX_RESPONSE_DATA_LEN - offset,
                      "  Size: %zu bytes\n", file_entry->metadata.size);
    offset += snprintf(info + offset, MAX_RESPONSE_DATA_LEN - offset,
                      "  Word Count: %d\n", file_entry->metadata.word_count);
    offset += snprintf(info + offset, MAX_RESPONSE_DATA_LEN - offset,
                      "  Character Count: %d\n", file_entry->metadata.char_count);
    offset += snprintf(info + offset, MAX_RESPONSE_DATA_LEN - offset,
                      "  Created: %s\n", created_str);
    offset += snprintf(info + offset, MAX_RESPONSE_DATA_LEN - offset,
                      "  Last Modified: %s\n", modified_str);
    offset += snprintf(info + offset, MAX_RESPONSE_DATA_LEN - offset,
                      "  Last Accessed: %s by %s\n",
                      accessed_str, file_entry->metadata.last_accessed_by);
    
    // Access control list
    offset += snprintf(info + offset, MAX_RESPONSE_DATA_LEN - offset,
                      "  Access Control:\n");
    for (int i = 0; i < file_entry->metadata.access_count && offset < MAX_RESPONSE_DATA_LEN - 100; i++) {
        char perms[4] = "---";
        int p = file_entry->metadata.access_permissions[i];
        if (p & ACCESS_READ) perms[0] = 'R';
        if (p & ACCESS_WRITE) perms[1] = 'W';
        
        offset += snprintf(info + offset, MAX_RESPONSE_DATA_LEN - offset,
                          "    %s: %s\n",
                          file_entry->metadata.access_list[i],
                          perms);
    }
    
    response.status = STATUS_OK;
    strncpy(response.data, info, sizeof(response.data) - 1);
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    send_response(client_fd, &response);
    
    LOG_INFO_MSG("NAME_SERVER", "Sent file info for '%s' to user '%s'", filename, req->username);
}

// Phase 4: Handle ADDACCESS command - Grant file access
void handle_addaccess(int client_fd, request_packet_t* req) {
    LOG_INFO_MSG("NAME_SERVER", "Handling ADDACCESS request by user: %s", req->username);
    
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    
    // Parse args: "permission filename target_user"
    char permission[8], filename[MAX_FILENAME_LEN], target_user[MAX_USERNAME_LEN];
    if (sscanf(req->args, "%s %s %s", permission, filename, target_user) != 3) {
        response.status = STATUS_ERROR_INVALID_ARGS;
        snprintf(response.data, sizeof(response.data), "Invalid arguments");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "Invalid ADDACCESS arguments: %s", req->args);
        return;
    }
    
    // Validate permission flag
    if (strcmp(permission, "-R") != 0 && strcmp(permission, "-W") != 0) {
        response.status = STATUS_ERROR_INVALID_ARGS;
        snprintf(response.data, sizeof(response.data), "Invalid permission flag. Use -R or -W");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        return;
    }
    
    // Find file
    file_hash_entry_t* file_entry = find_file_in_table(&file_table, filename);
    if (file_entry == NULL) {
        response.status = STATUS_ERROR_NOT_FOUND;
        snprintf(response.data, sizeof(response.data), "File '%s' not found", filename);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "File not found: %s", filename);
        return;
    }
    
    // Check ownership
    if (strcmp(file_entry->metadata.owner, req->username) != 0) {
        response.status = STATUS_ERROR_OWNER_REQUIRED;
        snprintf(response.data, sizeof(response.data), "Only the owner can modify access control");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "User '%s' attempted to modify access for file owned by '%s'",
                       req->username, file_entry->metadata.owner);
        return;
    }
    
    // Snapshot old metadata for rollback
    file_metadata_t old_meta;
    memcpy(&old_meta, &file_entry->metadata, sizeof(file_metadata_t));

    // Find or create ACL entry
    int acl_index = -1;
    for (int i = 0; i < file_entry->metadata.access_count; i++) {
        if (strcmp(file_entry->metadata.access_list[i], target_user) == 0) {
            acl_index = i;
            break;
        }
    }
    
    if (acl_index == -1) {
        // Add new entry
        if (file_entry->metadata.access_count >= MAX_CLIENTS) {
            response.status = STATUS_ERROR_INTERNAL;
            snprintf(response.data, sizeof(response.data), "Access control list is full");
            response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
            send_response(client_fd, &response);
            return;
        }
        acl_index = file_entry->metadata.access_count++;
        strncpy(file_entry->metadata.access_list[acl_index], target_user, MAX_USERNAME_LEN - 1);
        file_entry->metadata.access_permissions[acl_index] = ACCESS_NONE;
    }
    
    // Update permissions
    if (strcmp(permission, "-R") == 0) {
        file_entry->metadata.access_permissions[acl_index] |= ACCESS_READ;
    } else if (strcmp(permission, "-W") == 0) {
        file_entry->metadata.access_permissions[acl_index] |= ACCESS_WRITE;
        file_entry->metadata.access_permissions[acl_index] |= ACCESS_READ;  // Write implies read
    }

    // Serialize ACL to send to Storage Server
    char acl_str[MAX_ARGS_LEN];
    memset(acl_str, 0, sizeof(acl_str));
    int off = 0;
    for (int i = 0; i < file_entry->metadata.access_count && off < (int)sizeof(acl_str) - 50; i++) {
        int p = file_entry->metadata.access_permissions[i];
        const char* user = file_entry->metadata.access_list[i];
        const char* perm_s = "-";
        char tmpperm[4] = "";
        if ((p & ACCESS_READ) && (p & ACCESS_WRITE)) strcpy(tmpperm, "RW");
        else if (p & ACCESS_READ) strcpy(tmpperm, "R");
        else if (p & ACCESS_WRITE) strcpy(tmpperm, "RW");
        else strcpy(tmpperm, "-");

        if (i > 0) {
            off += snprintf(acl_str + off, sizeof(acl_str) - off, ",%s:%s", user, tmpperm);
        } else {
            off += snprintf(acl_str + off, sizeof(acl_str) - off, "%s:%s", user, tmpperm);
        }
    }

    // Build request to storage server to persist ACL
    int ss_fd = file_entry->ss_socket_fd;
    request_packet_t ss_request;
    memset(&ss_request, 0, sizeof(ss_request));
    ss_request.magic = PROTOCOL_MAGIC;
    ss_request.command = CMD_UPDATE_ACL;
    // Forward the original user as the actor
    strncpy(ss_request.username, req->username, sizeof(ss_request.username) - 1);
    snprintf(ss_request.args, sizeof(ss_request.args), "%s %s", filename, acl_str);
    ss_request.checksum = calculate_checksum(&ss_request, sizeof(ss_request) - sizeof(uint32_t));

    // Send to SS and wait for response
    if (send_packet(ss_fd, &ss_request) < 0) {
        // rollback
        memcpy(&file_entry->metadata, &old_meta, sizeof(file_metadata_t));
        response.status = STATUS_ERROR_NETWORK;
        snprintf(response.data, sizeof(response.data), "Failed to communicate with storage server");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "Failed to send UPDATE_ACL to SS");
        return;
    }

    response_packet_t ss_response;
    int r = recv_packet(ss_fd, &ss_response);
    if (r <= 0 || ss_response.status != STATUS_OK) {
        // rollback
        memcpy(&file_entry->metadata, &old_meta, sizeof(file_metadata_t));
        response.status = (r <= 0) ? STATUS_ERROR_NETWORK : ss_response.status;
        snprintf(response.data, sizeof(response.data), "%s", (r <= 0) ? "Storage server did not respond" : ss_response.data);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "SS failed to persist ACL change: %s", (r <= 0) ? "no response" : ss_response.data);
        return;
    }

    // Persisted successfully
    response.status = STATUS_OK;
    snprintf(response.data, sizeof(response.data), "Access granted successfully");
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    send_response(client_fd, &response);

    LOG_INFO_MSG("NAME_SERVER", "User '%s' granted %s access to '%s' for user '%s'",
                 req->username, permission, filename, target_user);
}

// Phase 4: Handle REMACCESS command - Revoke file access
void handle_remaccess(int client_fd, request_packet_t* req) {
    LOG_INFO_MSG("NAME_SERVER", "Handling REMACCESS request by user: %s", req->username);
    
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    
    // Parse args: "filename target_user"
    char filename[MAX_FILENAME_LEN], target_user[MAX_USERNAME_LEN];
    if (sscanf(req->args, "%s %s", filename, target_user) != 2) {
        response.status = STATUS_ERROR_INVALID_ARGS;
        snprintf(response.data, sizeof(response.data), "Invalid arguments");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "Invalid REMACCESS arguments: %s", req->args);
        return;
    }
    
    // Find file
    file_hash_entry_t* file_entry = find_file_in_table(&file_table, filename);
    if (file_entry == NULL) {
        response.status = STATUS_ERROR_NOT_FOUND;
        snprintf(response.data, sizeof(response.data), "File '%s' not found", filename);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "File not found: %s", filename);
        return;
    }
    
    // Check ownership
    if (strcmp(file_entry->metadata.owner, req->username) != 0) {
        response.status = STATUS_ERROR_OWNER_REQUIRED;
        snprintf(response.data, sizeof(response.data), "Only the owner can modify access control");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "User '%s' attempted to modify access for file owned by '%s'",
                       req->username, file_entry->metadata.owner);
        return;
    }
    
    // Cannot remove owner's access
    if (strcmp(target_user, req->username) == 0) {
        response.status = STATUS_ERROR_INVALID_OPERATION;
        snprintf(response.data, sizeof(response.data), "Cannot remove owner's access");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        return;
    }
    
    // Snapshot old metadata for rollback
    file_metadata_t old_meta;
    memcpy(&old_meta, &file_entry->metadata, sizeof(file_metadata_t));

    // Find and remove ACL entry
    int found = 0;
    for (int i = 0; i < file_entry->metadata.access_count; i++) {
        if (strcmp(file_entry->metadata.access_list[i], target_user) == 0) {
            // Shift remaining entries
            for (int j = i; j < file_entry->metadata.access_count - 1; j++) {
                strncpy(file_entry->metadata.access_list[j],
                       file_entry->metadata.access_list[j + 1],
                       MAX_USERNAME_LEN);
                file_entry->metadata.access_permissions[j] = 
                    file_entry->metadata.access_permissions[j + 1];
            }
            file_entry->metadata.access_count--;
            found = 1;
            break;
        }
    }
    
    if (!found) {
        response.status = STATUS_ERROR_NOT_FOUND;
        snprintf(response.data, sizeof(response.data), 
                "User '%s' does not have access to this file", target_user);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        return;
    }
    
    // At this point ACL in memory has been updated; persist to storage server
    // Build ACL serialization
    char acl_str[MAX_ARGS_LEN];
    memset(acl_str, 0, sizeof(acl_str));
    int off = 0;
    for (int i = 0; i < file_entry->metadata.access_count && off < (int)sizeof(acl_str) - 50; i++) {
        int p = file_entry->metadata.access_permissions[i];
        const char* user = file_entry->metadata.access_list[i];
        char tmpperm[4] = "";
        if ((p & ACCESS_READ) && (p & ACCESS_WRITE)) strcpy(tmpperm, "RW");
        else if (p & ACCESS_READ) strcpy(tmpperm, "R");
        else if (p & ACCESS_WRITE) strcpy(tmpperm, "RW");
        else strcpy(tmpperm, "-");

        if (i > 0) off += snprintf(acl_str + off, sizeof(acl_str) - off, ",%s:%s", user, tmpperm);
        else off += snprintf(acl_str + off, sizeof(acl_str) - off, "%s:%s", user, tmpperm);
    }

    int ss_fd = file_entry->ss_socket_fd;
    request_packet_t ss_request;
    memset(&ss_request, 0, sizeof(ss_request));
    ss_request.magic = PROTOCOL_MAGIC;
    ss_request.command = CMD_UPDATE_ACL;
    strncpy(ss_request.username, req->username, sizeof(ss_request.username) - 1);
    snprintf(ss_request.args, sizeof(ss_request.args), "%s %s", filename, acl_str);
    ss_request.checksum = calculate_checksum(&ss_request, sizeof(ss_request) - sizeof(uint32_t));

    if (send_packet(ss_fd, &ss_request) < 0) {
        // rollback: restore old metadata
        memcpy(&file_entry->metadata, &old_meta, sizeof(file_metadata_t));
        response.status = STATUS_ERROR_NETWORK;
        snprintf(response.data, sizeof(response.data), "Failed to communicate with storage server");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "Failed to send UPDATE_ACL to SS");
        return;
    }

    response_packet_t ss_response;
    int r = recv_packet(ss_fd, &ss_response);
    if (r <= 0 || ss_response.status != STATUS_OK) {
        // rollback
        memcpy(&file_entry->metadata, &old_meta, sizeof(file_metadata_t));
        response.status = (r <= 0) ? STATUS_ERROR_NETWORK : ss_response.status;
        snprintf(response.data, sizeof(response.data), "%s", (r <= 0) ? "Storage server did not respond" : ss_response.data);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "SS failed to persist ACL removal: %s", (r <= 0) ? "no response" : ss_response.data);
        return;
    }

    response.status = STATUS_OK;
    snprintf(response.data, sizeof(response.data), "Access revoked successfully");
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    send_response(client_fd, &response);

    LOG_INFO_MSG("NAME_SERVER", "User '%s' revoked access to '%s' from user '%s'",
                 req->username, filename, target_user);
}

// Phase 5.2: Handle READ file request - return Storage Server location
void handle_read_file(int client_fd, request_packet_t* req) {
    LOG_INFO_MSG("NAME_SERVER", "Handling READ request for file: %s by user: %s",
                 req->args, req->username);
    
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    
    // Extract filename from args
    char filename[MAX_FILENAME_LEN];
    sscanf(req->args, "%s", filename);
    
    // Find file in registry
    file_hash_entry_t* file_entry = find_file_in_table(&file_table, filename);
    if (file_entry == NULL) {
        response.status = STATUS_ERROR_NOT_FOUND;
        snprintf(response.data, sizeof(response.data), "File '%s' not found", filename);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "File not found: %s", filename);
        return;
    }
    
    // Check read access
    if (!check_user_has_access(file_entry, req->username, ACCESS_READ)) {
        response.status = STATUS_ERROR_READ_PERMISSION;
        snprintf(response.data, sizeof(response.data), 
                "Permission denied: You do not have read access to this file");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "User '%s' denied read access to file '%s'",
                       req->username, filename);
        return;
    }
    
    // Get storage server info
    int ss_fd = file_entry->ss_socket_fd;
    ss_node_t* ss = find_storage_server_by_fd(storage_servers_list, ss_fd);
    if (ss == NULL) {
        response.status = STATUS_ERROR_SERVER_UNAVAILABLE;
        snprintf(response.data, sizeof(response.data), 
                "Storage server not available");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "Storage server not found for file: %s", filename);
        return;
    }
    
    // Send storage server location to client
    char location[128];
    snprintf(location, sizeof(location), "%s:%d", ss->data.ip, ss->data.client_port);
    
    response.status = STATUS_OK;
    snprintf(response.data, sizeof(response.data), "%s", location);
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    send_response(client_fd, &response);
    
    LOG_INFO_MSG("NAME_SERVER", "Directed user '%s' to read '%s' from SS at %s",
                 req->username, filename, location);
}

// Phase 5.2: Handle STREAM file request - same as READ (return SS location)
void handle_stream_file(int client_fd, request_packet_t* req) {
    LOG_INFO_MSG("NAME_SERVER", "Handling STREAM request for file: %s by user: %s",
                 req->args, req->username);
    
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    
    // Extract filename from args
    char filename[MAX_FILENAME_LEN];
    sscanf(req->args, "%s", filename);
    
    // Find file in registry
    file_hash_entry_t* file_entry = find_file_in_table(&file_table, filename);
    if (file_entry == NULL) {
        response.status = STATUS_ERROR_NOT_FOUND;
        snprintf(response.data, sizeof(response.data), "File '%s' not found", filename);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "File not found: %s", filename);
        return;
    }
    
    // Check read access (streaming requires read permission)
    if (!check_user_has_access(file_entry, req->username, ACCESS_READ)) {
        response.status = STATUS_ERROR_READ_PERMISSION;
        snprintf(response.data, sizeof(response.data), 
                "Permission denied: You do not have read access to this file");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "User '%s' denied stream access to file '%s'",
                       req->username, filename);
        return;
    }
    
    // Get storage server info
    int ss_fd = file_entry->ss_socket_fd;
    ss_node_t* ss = find_storage_server_by_fd(storage_servers_list, ss_fd);
    if (ss == NULL) {
        response.status = STATUS_ERROR_SERVER_UNAVAILABLE;
        snprintf(response.data, sizeof(response.data), 
                "Storage server not available");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "Storage server not found for file: %s", filename);
        return;
    }
    
    // Send storage server location to client
    char location[128];
    snprintf(location, sizeof(location), "%s:%d", ss->data.ip, ss->data.client_port);
    
    response.status = STATUS_OK;
    snprintf(response.data, sizeof(response.data), "%s", location);
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    send_response(client_fd, &response);
    
    LOG_INFO_MSG("NAME_SERVER", "Directed user '%s' to stream '%s' from SS at %s",
                 req->username, filename, location);
}

// Phase 5.3: Handle WRITE file request - check write permission and return SS location
void handle_write_file(int client_fd, request_packet_t* req) {
    LOG_INFO_MSG("NAME_SERVER", "Handling WRITE request for file: %s by user: %s",
                 req->args, req->username);
    
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    
    // Extract filename from args (format: "filename sentence_num")
    char filename[MAX_FILENAME_LEN];
    int sentence_num = 0;
    sscanf(req->args, "%s %d", filename, &sentence_num);
    
    // Find file in registry
    file_hash_entry_t* file_entry = find_file_in_table(&file_table, filename);
    if (file_entry == NULL) {
        response.status = STATUS_ERROR_NOT_FOUND;
        snprintf(response.data, sizeof(response.data), "File '%s' not found", filename);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "File not found: %s", filename);
        return;
    }
    
    // Check write access
    if (!check_user_has_access(file_entry, req->username, ACCESS_WRITE)) {
        response.status = STATUS_ERROR_WRITE_PERMISSION;
        snprintf(response.data, sizeof(response.data), 
                "Permission denied: You do not have write access to this file");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "User '%s' denied write access to file '%s'",
                       req->username, filename);
        return;
    }
    
    // Get storage server info
    int ss_fd = file_entry->ss_socket_fd;
    ss_node_t* ss = find_storage_server_by_fd(storage_servers_list, ss_fd);
    if (ss == NULL) {
        response.status = STATUS_ERROR_SERVER_UNAVAILABLE;
        snprintf(response.data, sizeof(response.data), 
                "Storage server not available");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "Storage server not found for file: %s", filename);
        return;
    }
    
    // Send storage server location to client
    char location[128];
    snprintf(location, sizeof(location), "%s:%d", ss->data.ip, ss->data.client_port);
    
    response.status = STATUS_OK;
    snprintf(response.data, sizeof(response.data), "%s", location);
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    send_response(client_fd, &response);
    
    LOG_INFO_MSG("NAME_SERVER", "Directed user '%s' to write to '%s' sentence %d at SS %s",
                 req->username, filename, sentence_num, location);
}

// Phase 5.3: Handle UNDO file request - forward to storage server
void handle_undo_file(int client_fd, request_packet_t* req) {
    LOG_INFO_MSG("NAME_SERVER", "Handling UNDO request for file: %s by user: %s",
                 req->args, req->username);
    
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    
    // Extract filename from args
    char filename[MAX_FILENAME_LEN];
    sscanf(req->args, "%s", filename);
    
    // Find file in registry
    file_hash_entry_t* file_entry = find_file_in_table(&file_table, filename);
    if (file_entry == NULL) {
        response.status = STATUS_ERROR_NOT_FOUND;
        snprintf(response.data, sizeof(response.data), "File '%s' not found", filename);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "File not found: %s", filename);
        return;
    }
    
    // Check write access (undo requires write permission)
    if (!check_user_has_access(file_entry, req->username, ACCESS_WRITE)) {
        response.status = STATUS_ERROR_WRITE_PERMISSION;
        snprintf(response.data, sizeof(response.data), 
                "Permission denied: You do not have write access to this file");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "User '%s' denied undo access to file '%s'",
                       req->username, filename);
        return;
    }
    
    // Get storage server
    int ss_fd = file_entry->ss_socket_fd;
    ss_node_t* ss = find_storage_server_by_fd(storage_servers_list, ss_fd);
    if (ss == NULL) {
        response.status = STATUS_ERROR_SERVER_UNAVAILABLE;
        snprintf(response.data, sizeof(response.data), 
                "Storage server not available");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "Storage server not found for file: %s", filename);
        return;
    }
    
    // Forward CMD_UNDO request to storage server
    request_packet_t ss_request;
    memset(&ss_request, 0, sizeof(ss_request));
    ss_request.magic = PROTOCOL_MAGIC;
    ss_request.command = CMD_UNDO;
    strncpy(ss_request.username, req->username, sizeof(ss_request.username) - 1);
    strncpy(ss_request.args, filename, sizeof(ss_request.args) - 1);
    ss_request.checksum = calculate_checksum(&ss_request, sizeof(ss_request) - sizeof(uint32_t));
    
    if (send_packet(ss_fd, &ss_request) < 0) {
        response.status = STATUS_ERROR_INTERNAL;
        snprintf(response.data, sizeof(response.data), 
                "Failed to communicate with storage server");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "Failed to forward UNDO to storage server");
        return;
    }
    
    // Receive response from storage server
    response_packet_t ss_response;
    if (recv_packet(ss_fd, &ss_response) <= 0) {
        response.status = STATUS_ERROR_INTERNAL;
        snprintf(response.data, sizeof(response.data), 
                "Failed to receive response from storage server");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "Failed to receive UNDO response from storage server");
        return;
    }
    
    // Forward storage server's response to client
    send_response(client_fd, &ss_response);
    
    LOG_INFO_MSG("NAME_SERVER", "UNDO operation for '%s' by '%s': %s",
                 filename, req->username, 
                 ss_response.status == STATUS_OK ? "SUCCESS" : "FAILED");
}

// Phase 5.4: Handle EXEC command - execute script file and return output
void handle_exec_command(int client_fd, request_packet_t* req) {
    LOG_INFO_MSG("NAME_SERVER", "Handling EXEC request for file: %s by user: %s",
                 req->args, req->username);
    
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    
    // Extract filename from args
    char filename[MAX_FILENAME_LEN];
    sscanf(req->args, "%s", filename);
    
    // Find file in registry
    file_hash_entry_t* file_entry = find_file_in_table(&file_table, filename);
    if (file_entry == NULL) {
        response.status = STATUS_ERROR_NOT_FOUND;
        snprintf(response.data, sizeof(response.data), "File '%s' not found", filename);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "File not found: %s", filename);
        return;
    }
    
    // Check read access (exec requires read permission)
    if (!check_user_has_access(file_entry, req->username, ACCESS_READ)) {
        response.status = STATUS_ERROR_READ_PERMISSION;
        snprintf(response.data, sizeof(response.data), 
                "Permission denied: You do not have read access to this file");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "User '%s' denied exec access to file '%s'",
                       req->username, filename);
        return;
    }
    
    // Get storage server
    int ss_fd = file_entry->ss_socket_fd;
    ss_node_t* ss = find_storage_server_by_fd(storage_servers_list, ss_fd);
    if (ss == NULL) {
        response.status = STATUS_ERROR_SERVER_UNAVAILABLE;
        snprintf(response.data, sizeof(response.data), 
                "Storage server not available");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "Storage server not found for file: %s", filename);
        return;
    }
    
    // Request file content from storage server
    request_packet_t ss_request;
    memset(&ss_request, 0, sizeof(ss_request));
    ss_request.magic = PROTOCOL_MAGIC;
    ss_request.command = CMD_READ;
    strncpy(ss_request.username, req->username, sizeof(ss_request.username) - 1);
    strncpy(ss_request.args, filename, sizeof(ss_request.args) - 1);
    ss_request.checksum = calculate_checksum(&ss_request, sizeof(ss_request) - sizeof(uint32_t));
    
    if (send_packet(ss_fd, &ss_request) < 0) {
        response.status = STATUS_ERROR_INTERNAL;
        snprintf(response.data, sizeof(response.data), 
                "Failed to communicate with storage server");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "Failed to request file from storage server");
        return;
    }
    
    // Receive file content from storage server
    response_packet_t ss_response;
    if (recv_packet(ss_fd, &ss_response) <= 0) {
        response.status = STATUS_ERROR_INTERNAL;
        snprintf(response.data, sizeof(response.data), 
                "Failed to receive file from storage server");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "Failed to receive file from storage server");
        return;
    }
    
    if (ss_response.status != STATUS_OK) {
        // Forward error from storage server
        send_response(client_fd, &ss_response);
        LOG_ERROR_MSG("NAME_SERVER", "Storage server error: %s", ss_response.data);
        return;
    }
    
    // Now execute the script using fork/exec with pipes to capture output
    int pipe_fd[2];
    if (pipe(pipe_fd) < 0) {
        response.status = STATUS_ERROR_INTERNAL;
        snprintf(response.data, sizeof(response.data), 
                "Failed to create pipe: %s", strerror(errno));
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "pipe() failed: %s", strerror(errno));
        return;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        response.status = STATUS_ERROR_INTERNAL;
        snprintf(response.data, sizeof(response.data), 
                "Failed to fork: %s", strerror(errno));
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        LOG_ERROR_MSG("NAME_SERVER", "fork() failed: %s", strerror(errno));
        return;
    }
    
    if (pid == 0) {
        // Child process
        close(pipe_fd[0]); // Close read end
        
        // Redirect stdout and stderr to pipe
        dup2(pipe_fd[1], STDOUT_FILENO);
        dup2(pipe_fd[1], STDERR_FILENO);
        close(pipe_fd[1]);
        
        // Write script to temporary file
        char temp_script[64];
        snprintf(temp_script, sizeof(temp_script), "/tmp/exec_%d.sh", getpid());
        FILE* script_fp = fopen(temp_script, "w");
        if (script_fp == NULL) {
            fprintf(stderr, "Failed to create temp script\n");
            exit(1);
        }
        
        fwrite(ss_response.data, 1, strlen(ss_response.data), script_fp);
        fclose(script_fp);
        
        // Make script executable
        chmod(temp_script, 0700);
        
        // Execute the script
        execlp("/bin/sh", "sh", temp_script, NULL);
        
        // If exec fails
        fprintf(stderr, "Failed to execute script: %s\n", strerror(errno));
        exit(1);
    } else {
        // Parent process
        close(pipe_fd[1]); // Close write end
        
        // Read output from pipe
        char output_buffer[MAX_RESPONSE_DATA_LEN];
        ssize_t total_read = 0;
        ssize_t bytes_read;
        
        while ((bytes_read = read(pipe_fd[0], 
                                  output_buffer + total_read,
                                  sizeof(output_buffer) - total_read - 1)) > 0) {
            total_read += bytes_read;
            if (total_read >= (ssize_t)(sizeof(output_buffer) - 1)) {
                break; // Buffer full
            }
        }
        output_buffer[total_read] = '\0';
        close(pipe_fd[0]);
        
        // Wait for child to complete
        int status;
        waitpid(pid, &status, 0);
        
        // Clean up temporary script
        char temp_script[64];
        snprintf(temp_script, sizeof(temp_script), "/tmp/exec_%d.sh", pid);
        unlink(temp_script);
        
        // Send output to client
        response.status = STATUS_OK;
        strncpy(response.data, output_buffer, sizeof(response.data) - 1);
        response.data[sizeof(response.data) - 1] = '\0';
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(client_fd, &response);
        
        LOG_INFO_MSG("NAME_SERVER", "EXEC completed for '%s' by '%s' (exit status: %d)",
                     filename, req->username, WEXITSTATUS(status));
    }
}