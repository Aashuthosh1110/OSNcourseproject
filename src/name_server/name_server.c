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
    
    // Set up signal handlers
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);
    
    // Phase 2: Initialize state management
    init_name_server_state();
    
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
    
    printf("Name Server state initialized:\n");
    printf("  - Storage servers list: ready\n");
    printf("  - Clients list: ready\n");
    printf("  - File hash table: %d buckets\n", HASH_TABLE_SIZE);
}

// Phase 2: Process data from connected clients/servers
void process_connection_data(int sockfd) {
    request_packet_t request;
    int bytes = recv_request(sockfd, &request);
    
    if (bytes <= 0) {
        // Connection closed or error
        printf("Connection closed: fd=%d\n", sockfd);
        
        // Clean up from our data structures
        remove_storage_server(&storage_servers_list, sockfd);
        remove_client(&clients_list, sockfd);
        
        close(sockfd);
        FD_CLR(sockfd, global_master_fds);
        return;
    }
    
    // Validate packet integrity
    if (!validate_packet_integrity(&request, sizeof(request))) {
        printf("Received corrupted packet from fd=%d\n", sockfd);
        return;
    }
    
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
    
    // Add to clients list
    client_node_t* new_client = add_client(&clients_list, &user_info);
    if (new_client) {
        printf("Registered client '%s' from %s (fd=%d)\n", 
               user_info.username, user_info.client_ip, sockfd);
        
        // Send success response
        response_packet_t response;
        memset(&response, 0, sizeof(response));
        response.magic = PROTOCOL_MAGIC;
        response.status = STATUS_OK;
        snprintf(response.data, sizeof(response.data), 
                "Welcome %s! Connected to Docs++", user_info.username);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        
        send_response(sockfd, &response);
    } else {
        printf("Failed to register client from fd=%d\n", sockfd);
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
        send_response(client_fd, &response);
        LOG_WARNING_MSG("NAME_SERVER", "File already exists: %s", filename);
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
    
    // Step 7: Remove file from Name Server's registry
    if (remove_file_from_table(&file_table, filename) < 0) {
        LOG_ERROR_MSG("NAME_SERVER", "Failed to remove file from registry");
        // File deleted on SS but still in registry - warn but continue
    }
    
    LOG_INFO_MSG("NAME_SERVER", "File '%s' deleted successfully by user '%s'", 
                filename, req->username);
    
    // Step 8: Send success response to client
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
    
    // Iterate through connected clients
    client_node_t* current = clients_list;
    while (current && offset < MAX_RESPONSE_DATA_LEN - 200) {
        if (current->data.active) {
            char time_str[64];
            struct tm* timeinfo = localtime(&current->data.connected_time);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);
            
            int written = snprintf(user_list + offset, MAX_RESPONSE_DATA_LEN - offset,
                                  "%d. %s (connected from %s at %s)\n",
                                  ++count,
                                  current->data.username,
                                  current->data.client_ip,
                                  time_str);
            offset += written;
        }
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
    
    // Parse flags from args
    int flag_all = 0;
    int flag_long = 0;
    
    if (strstr(req->args, "-a") != NULL) {
        flag_all = 1;
    }
    if (strstr(req->args, "-l") != NULL) {
        flag_long = 1;
    }
    
    char file_list[MAX_RESPONSE_DATA_LEN];
    int offset = 0;
    int file_count = 0;
    
    // Header
    if (flag_long) {
        offset += snprintf(file_list + offset, MAX_RESPONSE_DATA_LEN - offset,
                          "%-12s %-14s %-30s\n", "Permissions", "Owner", "Filename");
        offset += snprintf(file_list + offset, MAX_RESPONSE_DATA_LEN - offset,
                          "------------------------------------------------------------\n");
    } else {
        offset += snprintf(file_list + offset, MAX_RESPONSE_DATA_LEN - offset,
                          "Files:\n");
    }
    
    // Iterate through all files in hash table
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        file_hash_entry_t* current = file_table.buckets[i];
        while (current && offset < MAX_RESPONSE_DATA_LEN - 100) {
            // Check access
            int has_access = flag_all || check_user_has_access(current, req->username, ACCESS_READ);
            
            if (has_access) {
                if (flag_long) {
                    char perms[4] = "---";
                    if (strcmp(current->metadata.owner, req->username) == 0) {
                        strcpy(perms, "RW");
                    } else {
                        // Check ACL for permissions
                        for (int j = 0; j < current->metadata.access_count; j++) {
                            if (strcmp(current->metadata.access_list[j], req->username) == 0) {
                                int p = current->metadata.access_permissions[j];
                                if (p & ACCESS_READ) perms[0] = 'R';
                                if (p & ACCESS_WRITE) perms[1] = 'W';
                                break;
                            }
                        }
                    }
                    
                    offset += snprintf(file_list + offset, MAX_RESPONSE_DATA_LEN - offset,
                                      "%-12s %-14s %-30s\n",
                                      perms,
                                      current->metadata.owner,
                                      current->metadata.filename);
                } else {
                    offset += snprintf(file_list + offset, MAX_RESPONSE_DATA_LEN - offset,
                                      "  %s\n", current->metadata.filename);
                }
                file_count++;
            }
            
            current = current->next;
        }
    }
    
    if (file_count == 0) {
        strcpy(file_list, "No files available.\n");
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
    
    response.status = STATUS_OK;
    snprintf(response.data, sizeof(response.data), "Access revoked successfully");
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    send_response(client_fd, &response);
    
    LOG_INFO_MSG("NAME_SERVER", "User '%s' revoked access to '%s' from user '%s'",
                 req->username, filename, target_user);
}