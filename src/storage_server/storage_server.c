/*
 * Storage Server - File storage and retrieval for Docs++ system
 * Handles physical file storage, sentence-level locking, streaming,
 * and direct client communications.
 */

#include "../../include/common.h"
#include "../../include/protocol.h"
#include "../../include/logging.h"
#include "../../include/errors.h"
#include "../../include/file_ops.h"
#include <signal.h>
#include <dirent.h>
#include <sys/select.h>
#include <netdb.h>

// Global state
static char storage_path[MAX_PATH_LEN];
static char nm_ip[INET_ADDRSTRLEN];
static int nm_port;
static int client_port;
static int nm_socket = -1;
static int client_server_socket = -1;
// static file_lock_t* active_locks = NULL;  // TODO: Implement in Phase 1

// Phase 2: File discovery
static char discovered_files[MAX_FILES_PER_SERVER][MAX_FILENAME_LEN];
static int discovered_file_count = 0;

// Phase 5.3: Sentence-level locking for WRITE operations
static sentence_lock_t* global_lock_list = NULL;
static pthread_mutex_t lock_list_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
void register_with_name_server();
void initialize_storage_server(const char* path, int c_port);
void handle_nm_commands();
void handle_client_connections();
void scan_existing_files();
void cleanup_and_exit(int signal);
int create_file_on_disk(const char* filename, const char* username);
int read_file_from_disk(const char* filename, char* content, size_t max_size);
int write_file_to_disk(const char* filename, const char* content);
int delete_file_from_disk(const char* filename);
void stream_file_to_client(int client_socket, const char* filename);

// Phase 2: New functions
void discover_local_files();
void send_ss_init_packet();

// Phase 3: File operation handlers
void handle_create_request(request_packet_t* req);
void handle_delete_request(request_packet_t* req);
void handle_update_acl_request(request_packet_t* req);
int create_file_metadata(const char* filename, const char* owner);

// ACL helpers
char* serialize_acl_from_meta(const file_metadata_t* meta);
int parse_acl_into_meta(file_metadata_t* meta, const char* acl_str);

// Phase 5.3: Sentence lock management
int acquire_lock(const char* file, int index, const char* user);
void release_lock(const char* file, int index, const char* user);

// Phase 5.1: Thread-based client handler
void* client_connection_thread(void* arg);

int main(int argc, char* argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <nm_ip> <nm_port> <storage_path> <client_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    strncpy(nm_ip, argv[1], sizeof(nm_ip) - 1);
    nm_port = atoi(argv[2]);
    strncpy(storage_path, argv[3], sizeof(storage_path) - 1);
    client_port = atoi(argv[4]); // Get client port from 4th argument

    // Add validation for the new port
    if (client_port <= 0 || client_port > 65535) {
        fprintf(stderr, "Error: Invalid Client Port\n");
        exit(EXIT_FAILURE);
    }
    
    // Validate arguments
    if (nm_port <= 0 || nm_port > 65535) {
        fprintf(stderr, "Error: Invalid Name Server port\n");
        exit(EXIT_FAILURE);
    }
    
    printf("Storage Server starting...\n");
    printf("Name Server: %s:%d\n", nm_ip, nm_port);
    printf("Storage Path: %s\n", storage_path);
    printf("Client Port: %d\n", client_port);
    
    // Set up signal handlers
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);
    
    // Phase 2: Initialize storage and discover files
    initialize_storage_server(storage_path, client_port);
    discover_local_files();
    
    // Connect to Name Server and send initialization
    register_with_name_server();
    
    // Main server loop using select
    fd_set master_fds, read_fds;
    FD_ZERO(&master_fds);
    FD_SET(nm_socket, &master_fds);
    FD_SET(client_server_socket, &master_fds);
    
    int max_fd = (nm_socket > client_server_socket) ? nm_socket : client_server_socket;
    
    LOG_INFO_MSG("STORAGE_SERVER", "Server initialized, waiting for connections...");
    
    while (1) {
        read_fds = master_fds;
        
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
            LOG_ERROR_MSG("STORAGE_SERVER", "Select error: %s", strerror(errno));
            continue;
        }
        
        // Handle Name Server communications
        if (FD_ISSET(nm_socket, &read_fds)) {
            handle_nm_commands();
        }
        
        // Handle new client connections
        if (FD_ISSET(client_server_socket, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int client_socket = accept(client_server_socket, (struct sockaddr*)&client_addr, &addr_len);
            
            if (client_socket != -1) {
                LOG_INFO_MSG("STORAGE_SERVER", "New client connection from %s:%d",
                            inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                
                // Phase 5.1: Spawn a detached thread to handle this client
                int* sock_ptr = malloc(sizeof(int));
                if (sock_ptr == NULL) {
                    LOG_ERROR_MSG("STORAGE_SERVER", "Failed to allocate memory for client socket");
                    close(client_socket);
                } else {
                    *sock_ptr = client_socket;
                    pthread_t tid;
                    if (pthread_create(&tid, NULL, client_connection_thread, (void*)sock_ptr) != 0) {
                        LOG_ERROR_MSG("STORAGE_SERVER", "Failed to create thread for client");
                        free(sock_ptr);
                        close(client_socket);
                    }
                    // Thread created successfully; it will handle the client
                }
            }
        }
    }
    
    return 0;
}

void initialize_storage_server(const char* path, int c_port) {
    // Create storage directory if it doesn't exist
    struct stat st;
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) == -1) {
            LOG_CRITICAL_MSG("STORAGE_SERVER", "Failed to create storage directory: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    
    // Initialize client server socket
    client_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_server_socket == -1) {
        LOG_CRITICAL_MSG("STORAGE_SERVER", "Client socket creation failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    setsockopt(client_server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(c_port);
    
    if (bind(client_server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        LOG_CRITICAL_MSG("STORAGE_SERVER", "Client socket bind failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    if (listen(client_server_socket, BACKLOG) == -1) {
        LOG_CRITICAL_MSG("STORAGE_SERVER", "Client socket listen failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Scan existing files
    scan_existing_files();
}

void register_with_name_server() {
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
    
    printf("Connected to Name Server at %s:%d\n", nm_ip, nm_port);
    
    // Phase 2: Send SS_INIT packet
    send_ss_init_packet();
    
    printf("Storage Server registered successfully.\n");
}

void scan_existing_files() {
    DIR* dir = opendir(storage_path);
    if (dir == NULL) {
        LOG_ERROR_MSG("STORAGE_SERVER", "Failed to open storage directory: %s", strerror(errno));
        return;
    }
    
    struct dirent* entry;
    int file_count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and .. directories, process regular files
        if (entry->d_name[0] != '.') {
            LOG_INFO_MSG("STORAGE_SERVER", "Found existing file: %s", entry->d_name);
            file_count++;
        }
    }
    
    closedir(dir);
    LOG_INFO_MSG("STORAGE_SERVER", "Scanned %d existing files", file_count);
}

void handle_nm_commands() {
    request_packet_t request;
    if (recv_request(nm_socket, &request) <= 0) {
        LOG_WARNING_MSG("STORAGE_SERVER", "Lost connection to Name Server");
        return;
    }
    
    // Validate packet integrity
    if (!validate_packet_integrity(&request, sizeof(request))) {
        LOG_ERROR_MSG("STORAGE_SERVER", "Received corrupted packet from Name Server");
        return;
    }
    
    LOG_INFO_MSG("STORAGE_SERVER", "Received command from Name Server: command=%d", request.command);
    
    // Handle different command types from Name Server
    switch (request.command) {
        case CMD_CREATE:
            handle_create_request(&request);
            break;
        case CMD_DELETE:
            handle_delete_request(&request);
            break;
        case CMD_UPDATE_ACL:
            handle_update_acl_request(&request);
            break;
        case CMD_READ:
            // Phase 5.4: Handle READ request from Name Server (for EXEC)
            // This is different from client CMD_READ - NM has already checked permissions
            {
                char filename[MAX_FILENAME_LEN];
                sscanf(request.args, "%s", filename);
                
                char filepath[MAX_PATH_LEN];
                snprintf(filepath, sizeof(filepath), "%s/%s", storage_path, filename);
                
                response_packet_t response;
                memset(&response, 0, sizeof(response));
                response.magic = PROTOCOL_MAGIC;
                
                // Open and read the file
                FILE* fp = fopen(filepath, "r");
                if (fp == NULL) {
                    response.status = STATUS_ERROR_NOT_FOUND;
                    snprintf(response.data, sizeof(response.data),
                            "File not found: %s", filename);
                    LOG_ERROR_MSG("STORAGE_SERVER", "NM requested file not found: %s", filename);
                } else {
                    // Read entire file content into response.data
                    size_t bytes_read = fread(response.data, 1, 
                                             sizeof(response.data) - 1, fp);
                    response.data[bytes_read] = '\0';
                    fclose(fp);
                    
                    response.status = STATUS_OK;
                    LOG_INFO_MSG("STORAGE_SERVER", 
                                "Sent file '%s' content to NM (%zu bytes)", 
                                filename, bytes_read);
                }
                
                response.checksum = calculate_checksum(&response,
                                                       sizeof(response) - sizeof(uint32_t));
                send_response(nm_socket, &response);
            }
            break;
        case CMD_UNDO:
            // Handle UNDO: restore file from backup
            {
                char filename[MAX_FILENAME_LEN];
                sscanf(request.args, "%s", filename);
                
                char filepath[MAX_PATH_LEN];
                char backup_filepath[MAX_PATH_LEN];
                snprintf(filepath, sizeof(filepath), "%s/%s", storage_path, filename);
                snprintf(backup_filepath, sizeof(backup_filepath), "%s/%s.bak", storage_path, filename);
                
                response_packet_t response;
                memset(&response, 0, sizeof(response));
                response.magic = PROTOCOL_MAGIC;
                
                // Check if backup exists
                if (access(backup_filepath, F_OK) != 0) {
                    response.status = STATUS_ERROR_NOT_FOUND;
                    snprintf(response.data, sizeof(response.data), 
                            "No backup found for '%s'", filename);
                    LOG_WARNING_MSG("STORAGE_SERVER", "UNDO failed: no backup for '%s'", filename);
                } else {
                    // Rename backup to original file
                    if (rename(backup_filepath, filepath) == 0) {
                        response.status = STATUS_OK;
                        snprintf(response.data, sizeof(response.data), 
                                "File '%s' restored from backup", filename);
                        LOG_INFO_MSG("STORAGE_SERVER", "UNDO successful: restored '%s'", filename);
                    } else {
                        response.status = STATUS_ERROR_INTERNAL;
                        snprintf(response.data, sizeof(response.data), 
                                "Failed to restore '%s': %s", filename, strerror(errno));
                        LOG_ERROR_MSG("STORAGE_SERVER", "UNDO failed: rename error for '%s': %s",
                                     filename, strerror(errno));
                    }
                }
                
                response.checksum = calculate_checksum(&response, 
                                                       sizeof(response) - sizeof(uint32_t));
                send_response(nm_socket, &response);
            }
            break;
        default:
            LOG_WARNING_MSG("STORAGE_SERVER", "Unknown command from NM: %d", request.command);
            
            // Send error response
            response_packet_t error_response;
            memset(&error_response, 0, sizeof(error_response));
            error_response.magic = PROTOCOL_MAGIC;
            error_response.status = STATUS_ERROR_INVALID_OPERATION;
            snprintf(error_response.data, sizeof(error_response.data), 
                    "Unknown command: %d", request.command);
            error_response.checksum = calculate_checksum(&error_response, 
                                                         sizeof(error_response) - sizeof(uint32_t));
            send_response(nm_socket, &error_response);
            break;
    }
}

// Phase 5.1: Thread handler for client connections
void* client_connection_thread(void* arg) {
    // Get the socket from the argument and free the allocated memory
    int sock = *(int*)arg;
    free(arg);
    
    // Make this thread detached so it cleans up automatically
    pthread_detach(pthread_self());
    
    LOG_INFO_MSG("STORAGE_SERVER", "Client thread started for socket %d", sock);
    
    // Phase 5.3: Thread-local state for WRITE sessions
    char* file_buffer = NULL;
    char session_filename[MAX_FILENAME_LEN] = "";
    int session_sentence = -1;
    char session_user[MAX_USERNAME_LEN] = "";
    
    // Main client handling loop
    while (1) {
        request_packet_t request;
        int bytes = recv_request(sock, &request);
        
        if (bytes <= 0) {
            // Client disconnected or error
            LOG_INFO_MSG("STORAGE_SERVER", "Client disconnected from socket %d", sock);
            close(sock);
            return NULL;
        }
        
        // Validate packet integrity
        if (!validate_packet_integrity(&request, sizeof(request))) {
            LOG_ERROR_MSG("STORAGE_SERVER", "Received corrupted packet from client socket %d", sock);
            continue;
        }
        
        LOG_INFO_MSG("STORAGE_SERVER", "Received command %d from client socket %d", 
                     request.command, sock);
        
        // Handle different client-facing commands
        switch (request.command) {
            case CMD_READ:
                // Phase 5.2: Read file and send content to client
                LOG_INFO_MSG("STORAGE_SERVER", "Processing READ request for '%s' by user '%s'",
                            request.args, request.username);
                {
                    char filename[MAX_FILENAME_LEN];
                    sscanf(request.args, "%s", filename);
                    
                    // Build file paths
                    char filepath[MAX_PATH_LEN];
                    char metapath[MAX_PATH_LEN];
                    snprintf(filepath, sizeof(filepath), "%s/%s", storage_path, filename);
                    snprintf(metapath, sizeof(metapath), "%s/%s.meta", storage_path, filename);
                    
                    // Check if .meta file exists and verify permissions
                    FILE* meta_fp = fopen(metapath, "r");
                    if (meta_fp == NULL) {
                        LOG_ERROR_MSG("STORAGE_SERVER", "Metadata file not found: %s", metapath);
                        response_packet_t response;
                        memset(&response, 0, sizeof(response));
                        response.magic = PROTOCOL_MAGIC;
                        response.status = STATUS_ERROR_NOT_FOUND;
                        snprintf(response.data, sizeof(response.data), 
                                "File metadata not found");
                        response.checksum = calculate_checksum(&response, 
                                                               sizeof(response) - sizeof(uint32_t));
                        send_response(sock, &response);
                        close(sock);
                        return NULL;
                    }
                    
                    // Parse metadata to check access permissions
                    int has_access = 0;
                    char owner[MAX_USERNAME_LEN] = "";
                    char line[512];
                    
                    while (fgets(line, sizeof(line), meta_fp) != NULL) {
                        if (strncmp(line, "owner=", 6) == 0) {
                            sscanf(line + 6, "%s", owner);
                            if (strcmp(owner, request.username) == 0) {
                                has_access = 1;
                            }
                        } else if (strstr(line, "access_") == line) {
                            // Parse ACL entry: access_N=user:PERM
                            char acl_user[MAX_USERNAME_LEN];
                            char perms[8];
                            if (sscanf(line, "access_%*d=%[^:]:%s", acl_user, perms) == 2) {
                                if (strcmp(acl_user, request.username) == 0) {
                                    // Check if user has read permission
                                    if (strchr(perms, 'R') != NULL) {
                                        has_access = 1;
                                    }
                                }
                            }
                        }
                    }
                    fclose(meta_fp);
                    
                    if (!has_access) {
                        LOG_WARNING_MSG("STORAGE_SERVER", "User '%s' denied read access to '%s'",
                                       request.username, filename);
                        response_packet_t response;
                        memset(&response, 0, sizeof(response));
                        response.magic = PROTOCOL_MAGIC;
                        response.status = STATUS_ERROR_READ_PERMISSION;
                        snprintf(response.data, sizeof(response.data), 
                                "Permission denied");
                        response.checksum = calculate_checksum(&response, 
                                                               sizeof(response) - sizeof(uint32_t));
                        send_response(sock, &response);
                        close(sock);
                        return NULL;
                    }
                    
                    // Open and read the file
                    FILE* fp = fopen(filepath, "r");
                    if (fp == NULL) {
                        LOG_ERROR_MSG("STORAGE_SERVER", "Failed to open file: %s", filepath);
                        response_packet_t response;
                        memset(&response, 0, sizeof(response));
                        response.magic = PROTOCOL_MAGIC;
                        response.status = STATUS_ERROR_NOT_FOUND;
                        snprintf(response.data, sizeof(response.data), 
                                "File not found");
                        response.checksum = calculate_checksum(&response, 
                                                               sizeof(response) - sizeof(uint32_t));
                        send_response(sock, &response);
                        close(sock);
                        return NULL;
                    }
                    
                    // Send file content in chunks
                    char buffer[4096];
                    size_t bytes_read;
                    
                    LOG_INFO_MSG("STORAGE_SERVER", "Sending file '%s' to client", filename);
                    
                    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
                        ssize_t sent = send(sock, buffer, bytes_read, 0);
                        if (sent < 0) {
                            LOG_ERROR_MSG("STORAGE_SERVER", "Failed to send data to client");
                            break;
                        }
                    }
                    
                    fclose(fp);
                    close(sock);
                    LOG_INFO_MSG("STORAGE_SERVER", "Finished sending file '%s'", filename);
                    return NULL;
                }
                break;
                
            case CMD_WRITE:
                // Phase 5.3: Stateful WRITE handler with locking
                LOG_INFO_MSG("STORAGE_SERVER", "Processing WRITE request: '%s' by user '%s'",
                            request.args, request.username);
                {
                    
                    response_packet_t response;
                    memset(&response, 0, sizeof(response));
                    response.magic = PROTOCOL_MAGIC;
                    
                    // Check if this is initial WRITE or word update
                    char filename[MAX_FILENAME_LEN];
                    int sentence_num = -1;
                    int word_index = -1;
                    char word_content[MAX_WORD_LEN];
                    
                    // Try parsing as initial request (filename sentence_num)
                    if (sscanf(request.args, "%s %d", filename, &sentence_num) == 2) {
                        // This is initial WRITE request - acquire lock
                        
                        // Check if already have active session
                        if (file_buffer != NULL) {
                            response.status = STATUS_ERROR_INTERNAL;
                            snprintf(response.data, sizeof(response.data),
                                    "Session already active for %s", session_filename);
                            response.checksum = calculate_checksum(&response,
                                                                   sizeof(response) - sizeof(uint32_t));
                            send_response(sock, &response);
                            break;
                        }
                        
                        // Build file paths
                        char filepath[MAX_PATH_LEN];
                        char metapath[MAX_PATH_LEN];
                        snprintf(filepath, sizeof(filepath), "%s/%s", storage_path, filename);
                        snprintf(metapath, sizeof(metapath), "%s/%s.meta", storage_path, filename);
                        
                        // Check metadata for write permissions
                        FILE* meta_fp = fopen(metapath, "r");
                        if (meta_fp == NULL) {
                            response.status = STATUS_ERROR_NOT_FOUND;
                            snprintf(response.data, sizeof(response.data),
                                    "File metadata not found");
                            response.checksum = calculate_checksum(&response,
                                                                   sizeof(response) - sizeof(uint32_t));
                            send_response(sock, &response);
                            break;
                        }
                        
                        int has_write_access = 0;
                        char owner[MAX_USERNAME_LEN] = "";
                        char line[512];
                        
                        while (fgets(line, sizeof(line), meta_fp) != NULL) {
                            if (strncmp(line, "owner=", 6) == 0) {
                                sscanf(line + 6, "%s", owner);
                                if (strcmp(owner, request.username) == 0) {
                                    has_write_access = 1;
                                }
                            } else if (strstr(line, "access_") == line) {
                                char acl_user[MAX_USERNAME_LEN];
                                char perms[8];
                                if (sscanf(line, "access_%*d=%[^:]:%s", acl_user, perms) == 2) {
                                    if (strcmp(acl_user, request.username) == 0) {
                                        if (strchr(perms, 'W') != NULL) {
                                            has_write_access = 1;
                                        }
                                    }
                                }
                            }
                        }
                        fclose(meta_fp);
                        
                        if (!has_write_access) {
                            response.status = STATUS_ERROR_WRITE_PERMISSION;
                            snprintf(response.data, sizeof(response.data),
                                    "Permission denied");
                            response.checksum = calculate_checksum(&response,
                                                                   sizeof(response) - sizeof(uint32_t));
                            send_response(sock, &response);
                            LOG_WARNING_MSG("STORAGE_SERVER", "User '%s' denied write access to '%s'",
                                           request.username, filename);
                            break;
                        }
                        
                        // Try to acquire lock
                        if (!acquire_lock(filename, sentence_num, request.username)) {
                            response.status = STATUS_ERROR_LOCKED;
                            snprintf(response.data, sizeof(response.data),
                                    "Sentence %d is locked by another user", sentence_num);
                            response.checksum = calculate_checksum(&response,
                                                                   sizeof(response) - sizeof(uint32_t));
                            send_response(sock, &response);
                            LOG_WARNING_MSG("STORAGE_SERVER", "Lock denied for '%s' sentence %d",
                                           filename, sentence_num);
                            break;
                        }
                        
                        // Load file into memory
                        FILE* fp = fopen(filepath, "r");
                        if (fp == NULL) {
                            release_lock(filename, sentence_num, request.username);
                            response.status = STATUS_ERROR_NOT_FOUND;
                            snprintf(response.data, sizeof(response.data),
                                    "File not found");
                            response.checksum = calculate_checksum(&response,
                                                                   sizeof(response) - sizeof(uint32_t));
                            send_response(sock, &response);
                            break;
                        }
                        
                        // Get file size
                        fseek(fp, 0, SEEK_END);
                        long file_size = ftell(fp);
                        fseek(fp, 0, SEEK_SET);
                        
                        // Allocate buffer and read file
                        file_buffer = (char*)malloc(file_size + 1);
                        if (file_buffer == NULL) {
                            fclose(fp);
                            release_lock(filename, sentence_num, request.username);
                            response.status = STATUS_ERROR_INTERNAL;
                            snprintf(response.data, sizeof(response.data),
                                    "Memory allocation failed");
                            response.checksum = calculate_checksum(&response,
                                                                   sizeof(response) - sizeof(uint32_t));
                            send_response(sock, &response);
                            break;
                        }
                        
                        size_t bytes_read = fread(file_buffer, 1, file_size, fp);
                        file_buffer[bytes_read] = '\0';
                        fclose(fp);
                        
                        // Store session info
                        strncpy(session_filename, filename, MAX_FILENAME_LEN - 1);
                        session_sentence = sentence_num;
                        strncpy(session_user, request.username, MAX_USERNAME_LEN - 1);
                        
                        // Send success response
                        response.status = STATUS_OK;
                        snprintf(response.data, sizeof(response.data),
                                "Lock acquired for sentence %d", sentence_num);
                        response.checksum = calculate_checksum(&response,
                                                               sizeof(response) - sizeof(uint32_t));
                        send_response(sock, &response);
                        
                        LOG_INFO_MSG("STORAGE_SERVER", "WRITE session started: '%s' sentence %d by '%s'",
                                    filename, sentence_num, request.username);
                        
                    } else if (sscanf(request.args, "%d %s", &word_index, word_content) == 2) {
                        // This is word update request
                        
                        if (file_buffer == NULL) {
                            response.status = STATUS_ERROR_INTERNAL;
                            snprintf(response.data, sizeof(response.data),
                                    "No active WRITE session");
                            response.checksum = calculate_checksum(&response,
                                                                   sizeof(response) - sizeof(uint32_t));
                            send_response(sock, &response);
                            break;
                        }
                        
                        // TODO: Update word in file_buffer using file_ops functions
                        // For now, just acknowledge the update
                        response.status = STATUS_OK;
                        snprintf(response.data, sizeof(response.data),
                                "Word %d updated to '%s'", word_index, word_content);
                        response.checksum = calculate_checksum(&response,
                                                               sizeof(response) - sizeof(uint32_t));
                        send_response(sock, &response);
                        
                        LOG_INFO_MSG("STORAGE_SERVER", "Word %d updated in '%s'",
                                    word_index, session_filename);
                        
                    } else {
                        response.status = STATUS_ERROR_INVALID_OPERATION;
                        snprintf(response.data, sizeof(response.data),
                                "Invalid WRITE args format");
                        response.checksum = calculate_checksum(&response,
                                                               sizeof(response) - sizeof(uint32_t));
                        send_response(sock, &response);
                    }
                }
                break;
                
            case CMD_ETIRW:
                // Phase 5.3: Finalize WRITE session
                LOG_INFO_MSG("STORAGE_SERVER", "Processing ETIRW request");
                {
                    response_packet_t response;
                    memset(&response, 0, sizeof(response));
                    response.magic = PROTOCOL_MAGIC;
                    
                    if (file_buffer == NULL) {
                        response.status = STATUS_ERROR_INTERNAL;
                        snprintf(response.data, sizeof(response.data),
                                "No active WRITE session");
                        response.checksum = calculate_checksum(&response,
                                                               sizeof(response) - sizeof(uint32_t));
                        send_response(sock, &response);
                        break;
                    }
                    
                    // Build file paths
                    char filepath[MAX_PATH_LEN];
                    char backup_path[MAX_PATH_LEN];
                    snprintf(filepath, sizeof(filepath), "%s/%s", storage_path, session_filename);
                    snprintf(backup_path, sizeof(backup_path), "%s/%s.bak", storage_path, session_filename);
                    
                    // Create backup
                    if (rename(filepath, backup_path) != 0) {
                        response.status = STATUS_ERROR_INTERNAL;
                        snprintf(response.data, sizeof(response.data),
                                "Failed to create backup: %s", strerror(errno));
                        response.checksum = calculate_checksum(&response,
                                                               sizeof(response) - sizeof(uint32_t));
                        send_response(sock, &response);
                        LOG_ERROR_MSG("STORAGE_SERVER", "Backup creation failed for '%s'",
                                     session_filename);
                        break;
                    }
                    
                    // Write buffer to file
                    FILE* fp = fopen(filepath, "w");
                    if (fp == NULL) {
                        // Restore from backup
                        rename(backup_path, filepath);
                        response.status = STATUS_ERROR_INTERNAL;
                        snprintf(response.data, sizeof(response.data),
                                "Failed to open file for writing");
                        response.checksum = calculate_checksum(&response,
                                                               sizeof(response) - sizeof(uint32_t));
                        send_response(sock, &response);
                        break;
                    }
                    
                    size_t written = fwrite(file_buffer, 1, strlen(file_buffer), fp);
                    fclose(fp);
                    
                    if (written != strlen(file_buffer)) {
                        // Restore from backup
                        rename(backup_path, filepath);
                        response.status = STATUS_ERROR_INTERNAL;
                        snprintf(response.data, sizeof(response.data),
                                "Failed to write file completely");
                        response.checksum = calculate_checksum(&response,
                                                               sizeof(response) - sizeof(uint32_t));
                        send_response(sock, &response);
                        break;
                    }
                    
                    // Release lock
                    release_lock(session_filename, session_sentence, session_user);
                    
                    // Free buffer and reset session
                    free(file_buffer);
                    file_buffer = NULL;
                    session_filename[0] = '\0';
                    session_sentence = -1;
                    session_user[0] = '\0';
                    
                    // Send success response
                    response.status = STATUS_OK;
                    snprintf(response.data, sizeof(response.data),
                            "File saved successfully");
                    response.checksum = calculate_checksum(&response,
                                                           sizeof(response) - sizeof(uint32_t));
                    send_response(sock, &response);
                    
                    LOG_INFO_MSG("STORAGE_SERVER", "ETIRW completed for '%s'",
                                filepath);
                    
                    // Close connection after ETIRW
                    close(sock);
                    return NULL;
                }
                break;
                
            case CMD_STREAM:
                // Phase 5.2: Stream file (same as READ, permissions checked from .meta)
                LOG_INFO_MSG("STORAGE_SERVER", "Processing STREAM request for '%s' by user '%s'",
                            request.args, request.username);
                {
                    char filename[MAX_FILENAME_LEN];
                    sscanf(request.args, "%s", filename);
                    
                    // Build file paths
                    char filepath[MAX_PATH_LEN];
                    char metapath[MAX_PATH_LEN];
                    snprintf(filepath, sizeof(filepath), "%s/%s", storage_path, filename);
                    snprintf(metapath, sizeof(metapath), "%s/%s.meta", storage_path, filename);
                    
                    // Check if .meta file exists and verify permissions
                    FILE* meta_fp = fopen(metapath, "r");
                    if (meta_fp == NULL) {
                        LOG_ERROR_MSG("STORAGE_SERVER", "Metadata file not found: %s", metapath);
                        response_packet_t response;
                        memset(&response, 0, sizeof(response));
                        response.magic = PROTOCOL_MAGIC;
                        response.status = STATUS_ERROR_NOT_FOUND;
                        snprintf(response.data, sizeof(response.data), 
                                "File metadata not found");
                        response.checksum = calculate_checksum(&response, 
                                                               sizeof(response) - sizeof(uint32_t));
                        send_response(sock, &response);
                        close(sock);
                        return NULL;
                    }
                    
                    // Parse metadata to check access permissions
                    int has_access = 0;
                    char owner[MAX_USERNAME_LEN] = "";
                    char line[512];
                    
                    while (fgets(line, sizeof(line), meta_fp) != NULL) {
                        if (strncmp(line, "owner=", 6) == 0) {
                            sscanf(line + 6, "%s", owner);
                            if (strcmp(owner, request.username) == 0) {
                                has_access = 1;
                            }
                        } else if (strstr(line, "access_") == line) {
                            // Parse ACL entry: access_N=user:PERM
                            char acl_user[MAX_USERNAME_LEN];
                            char perms[8];
                            if (sscanf(line, "access_%*d=%[^:]:%s", acl_user, perms) == 2) {
                                if (strcmp(acl_user, request.username) == 0) {
                                    // Check if user has read permission
                                    if (strchr(perms, 'R') != NULL) {
                                        has_access = 1;
                                    }
                                }
                            }
                        }
                    }
                    fclose(meta_fp);
                    
                    if (!has_access) {
                        LOG_WARNING_MSG("STORAGE_SERVER", "User '%s' denied stream access to '%s'",
                                       request.username, filename);
                        response_packet_t response;
                        memset(&response, 0, sizeof(response));
                        response.magic = PROTOCOL_MAGIC;
                        response.status = STATUS_ERROR_READ_PERMISSION;
                        snprintf(response.data, sizeof(response.data), 
                                "Permission denied");
                        response.checksum = calculate_checksum(&response, 
                                                               sizeof(response) - sizeof(uint32_t));
                        send_response(sock, &response);
                        close(sock);
                        return NULL;
                    }
                    
                    // Open and stream the file
                    FILE* fp = fopen(filepath, "r");
                    if (fp == NULL) {
                        LOG_ERROR_MSG("STORAGE_SERVER", "Failed to open file: %s", filepath);
                        response_packet_t response;
                        memset(&response, 0, sizeof(response));
                        response.magic = PROTOCOL_MAGIC;
                        response.status = STATUS_ERROR_NOT_FOUND;
                        snprintf(response.data, sizeof(response.data), 
                                "File not found");
                        response.checksum = calculate_checksum(&response, 
                                                               sizeof(response) - sizeof(uint32_t));
                        send_response(sock, &response);
                        close(sock);
                        return NULL;
                    }
                    
                    // Send file content in chunks (same as READ)
                    char buffer[4096];
                    size_t bytes_read;
                    
                    LOG_INFO_MSG("STORAGE_SERVER", "Streaming file '%s' to client", filename);
                    
                    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
                        ssize_t sent = send(sock, buffer, bytes_read, 0);
                        if (sent < 0) {
                            LOG_ERROR_MSG("STORAGE_SERVER", "Failed to send data to client");
                            break;
                        }
                    }
                    
                    fclose(fp);
                    close(sock);
                    LOG_INFO_MSG("STORAGE_SERVER", "Finished streaming file '%s'", filename);
                    return NULL;
                }
                break;
                
            default:
                LOG_WARNING_MSG("STORAGE_SERVER", "Unknown command %d from client socket %d", 
                               request.command, sock);
                {
                    response_packet_t error_response;
                    memset(&error_response, 0, sizeof(error_response));
                    error_response.magic = PROTOCOL_MAGIC;
                    error_response.status = STATUS_ERROR_INVALID_OPERATION;
                    snprintf(error_response.data, sizeof(error_response.data), 
                            "Unknown command: %d", request.command);
                    error_response.checksum = calculate_checksum(&error_response, 
                                                                 sizeof(error_response) - sizeof(uint32_t));
                    send_response(sock, &error_response);
                }
                break;
        }
    }
    
    return NULL;
}

void stream_file_to_client(int client_socket, const char* filename) {
    char filepath[MAX_PATH_LEN + 256]; // Extra space to prevent truncation
    int ret = snprintf(filepath, sizeof(filepath), "%s/%s", storage_path, filename);
    
    // Check for truncation
    if (ret >= (int)sizeof(filepath)) {
        LOG_ERROR_MSG("STORAGE_SERVER", "File path too long: %s/%s", storage_path, filename);
        return;
    }
    
    FILE* file = fopen(filepath, "r");
    if (file == NULL) {
        LOG_ERROR_MSG("STORAGE_SERVER", "Failed to open file for streaming: %s", filename);
        return;
    }
    
    char word[MAX_WORD_LEN];
    while (fscanf(file, "%s", word) == 1) {
        // Send word to client as response
        response_packet_t word_response;
        memset(&word_response, 0, sizeof(word_response));
        word_response.magic = PROTOCOL_MAGIC;
        word_response.status = STATUS_OK;
        strncpy(word_response.data, word, sizeof(word_response.data) - 1);
        word_response.checksum = calculate_checksum(&word_response, sizeof(word_response) - sizeof(uint32_t));
        
        send_response(client_socket, &word_response);
        usleep(100000); // 0.1 second delay
    }
    
    // Send stop message
    response_packet_t stop_response;
    memset(&stop_response, 0, sizeof(stop_response));
    stop_response.magic = PROTOCOL_MAGIC;
    stop_response.status = STATUS_OK;
    strncpy(stop_response.data, "STREAM_END", sizeof(stop_response.data) - 1);
    stop_response.checksum = calculate_checksum(&stop_response, sizeof(stop_response) - sizeof(uint32_t));
    send_response(client_socket, &stop_response);
    
    fclose(file);
    LOG_INFO_MSG("STORAGE_SERVER", "Finished streaming file: %s", filename);
}

// Phase 2: Discover local files in storage directory
void discover_local_files() {
    discovered_file_count = 0;
    
    DIR* dir = opendir(storage_path);
    if (dir == NULL) {
        printf("Failed to open storage directory: %s\n", storage_path);
        return;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && discovered_file_count < MAX_FILES_PER_SERVER) {
        // Skip . and .. directories, process regular files
        if (entry->d_name[0] != '.') {
            strncpy(discovered_files[discovered_file_count], entry->d_name, MAX_FILENAME_LEN - 1);
            discovered_files[discovered_file_count][MAX_FILENAME_LEN - 1] = '\0';
            discovered_file_count++;
            printf("Discovered file: %s\n", entry->d_name);
        }
    }
    
    closedir(dir);
    printf("Total files discovered: %d\n", discovered_file_count);
}

// Phase 2: Send SS_INIT packet to Name Server
void send_ss_init_packet() {
    request_packet_t init_packet;
    memset(&init_packet, 0, sizeof(init_packet));
    
    init_packet.magic = PROTOCOL_MAGIC;
    init_packet.command = CMD_SS_INIT;
    snprintf(init_packet.username, sizeof(init_packet.username), "storage_server_%d", client_port);
    
    // Format: "IP:PORT:FILE1,FILE2,FILE3..."
    char files_list[512] = "";
    for (int i = 0; i < discovered_file_count && i < MAX_FILES_PER_SERVER; i++) {
        if (i > 0) {
            strncat(files_list, ",", sizeof(files_list) - strlen(files_list) - 1);
        }
        strncat(files_list, discovered_files[i], sizeof(files_list) - strlen(files_list) - 1);
    }
    
    snprintf(init_packet.args, sizeof(init_packet.args), "%s:%d:%s", nm_ip, client_port, files_list);
    init_packet.checksum = calculate_checksum(&init_packet, sizeof(init_packet) - sizeof(uint32_t));
    
    printf("Sending SS_INIT packet with %d files...\n", discovered_file_count);
    
    if (send_packet(nm_socket, &init_packet) < 0) {
        perror("Failed to send SS_INIT packet");
        exit(EXIT_FAILURE);
    }
    
    // Wait for response
    response_packet_t response;
    if (recv_packet(nm_socket, &response) <= 0) {
        printf("No response from Name Server\n");
        exit(EXIT_FAILURE);
    }
    
    if (response.status == STATUS_OK) {
        printf("SS initialization successful: %s\n", response.data);
    } else {
        printf("SS initialization failed: %s\n", response.data);
        exit(EXIT_FAILURE);
    }
}

void cleanup_and_exit(int signal) {
    printf("\nReceived signal %d, shutting down gracefully...\n", signal);
    
    if (nm_socket != -1) {
        close(nm_socket);
    }
    if (client_server_socket != -1) {
        close(client_server_socket);
    }
    
    exit(EXIT_SUCCESS);
}

// Phase 3: Handle CREATE file request from Name Server
void handle_create_request(request_packet_t* req) {
    LOG_INFO_MSG("STORAGE_SERVER", "Handling CREATE request for file: %s by user: %s", 
                 req->args, req->username);
    
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    
    // Extract filename from args
    char filename[MAX_FILENAME_LEN];
    sscanf(req->args, "%s", filename);
    
    // Construct file paths
    char filepath[MAX_PATH_LEN];
    char metapath[MAX_PATH_LEN];
    snprintf(filepath, sizeof(filepath), "%s/%s", storage_path, filename);
    snprintf(metapath, sizeof(metapath), "%s/%s.meta", storage_path, filename);
    
    // Check if file already exists
    if (access(filepath, F_OK) == 0) {
        LOG_WARNING_MSG("STORAGE_SERVER", "File already exists: %s", filepath);
        response.status = STATUS_ERROR_FILE_EXISTS;
        snprintf(response.data, sizeof(response.data), 
                "File already exists on storage");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(nm_socket, &response);
        return;
    }
    
    // Create empty file
    FILE* fp = fopen(filepath, "w");
    if (fp == NULL) {
        LOG_ERROR_MSG("STORAGE_SERVER", "Failed to create file: %s (errno: %d - %s)", 
                     filepath, errno, strerror(errno));
        response.status = STATUS_ERROR_INTERNAL;
        snprintf(response.data, sizeof(response.data), 
                "Failed to create file: %s", strerror(errno));
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(nm_socket, &response);
        return;
    }
    fclose(fp);
    
    LOG_INFO_MSG("STORAGE_SERVER", "Created file: %s", filepath);
    
    // Create metadata file
    if (create_file_metadata(filename, req->username) < 0) {
        LOG_ERROR_MSG("STORAGE_SERVER", "Failed to create metadata file: %s", metapath);
        unlink(filepath);  // Rollback: delete the data file
        response.status = STATUS_ERROR_INTERNAL;
        snprintf(response.data, sizeof(response.data), 
                "Failed to create metadata file");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(nm_socket, &response);
        return;
    }
    
    LOG_INFO_MSG("STORAGE_SERVER", "Successfully created file and metadata: %s", filename);
    
    // Send success response
    response.status = STATUS_OK;
    snprintf(response.data, sizeof(response.data), 
            "File created on storage");
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    send_response(nm_socket, &response);
}

// Phase 3: Handle DELETE file request from Name Server
void handle_delete_request(request_packet_t* req) {
    LOG_INFO_MSG("STORAGE_SERVER", "Handling DELETE request for file: %s by user: %s", 
                 req->args, req->username);
    
    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;
    
    // Extract filename from args
    char filename[MAX_FILENAME_LEN];
    sscanf(req->args, "%s", filename);
    
    // Construct file paths
    char filepath[MAX_PATH_LEN];
    char metapath[MAX_PATH_LEN];
    char backuppath[MAX_PATH_LEN];
    snprintf(filepath, sizeof(filepath), "%s/%s", storage_path, filename);
    snprintf(metapath, sizeof(metapath), "%s/%s.meta", storage_path, filename);
    snprintf(backuppath, sizeof(backuppath), "%s/%s.bak", storage_path, filename);
    
    // Check if file exists
    if (access(filepath, F_OK) != 0) {
        LOG_WARNING_MSG("STORAGE_SERVER", "File does not exist: %s", filepath);
        response.status = STATUS_ERROR_NOT_FOUND;
        snprintf(response.data, sizeof(response.data), 
                "File not found on storage");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(nm_socket, &response);
        return;
    }
    
    // Check ownership by reading metadata file
    if (access(metapath, F_OK) == 0) {
        FILE* meta_fp = fopen(metapath, "r");
        if (meta_fp != NULL) {
            char owner[MAX_USERNAME_LEN] = {0};
            char line[256];
            
            // Read metadata file line by line
            while (fgets(line, sizeof(line), meta_fp) != NULL) {
                if (strncmp(line, "owner=", 6) == 0) {
                    sscanf(line, "owner=%s", owner);
                    break;
                }
            }
            fclose(meta_fp);
            
            // Verify ownership
            if (strlen(owner) > 0 && strcmp(owner, req->username) != 0) {
                LOG_WARNING_MSG("STORAGE_SERVER", "User '%s' attempted to delete file owned by '%s'", 
                               req->username, owner);
                response.status = STATUS_ERROR_OWNER_REQUIRED;
                snprintf(response.data, sizeof(response.data), 
                        "Only the owner can delete this file");
                response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
                send_response(nm_socket, &response);
                return;
            }
            
            LOG_INFO_MSG("STORAGE_SERVER", "Ownership verified: user '%s' owns file '%s'", 
                        req->username, filename);
        }
    }
    
    // Delete main file
    if (unlink(filepath) != 0) {
        LOG_ERROR_MSG("STORAGE_SERVER", "Failed to delete file: %s (errno: %d - %s)", 
                     filepath, errno, strerror(errno));
        response.status = STATUS_ERROR_INTERNAL;
        snprintf(response.data, sizeof(response.data), 
                "Failed to delete file: %s", strerror(errno));
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(nm_socket, &response);
        return;
    }
    
    LOG_INFO_MSG("STORAGE_SERVER", "Deleted file: %s", filepath);
    
    // Delete metadata file (ignore errors if it doesn't exist)
    if (access(metapath, F_OK) == 0) {
        if (unlink(metapath) != 0) {
            LOG_WARNING_MSG("STORAGE_SERVER", "Failed to delete metadata file: %s", metapath);
        } else {
            LOG_INFO_MSG("STORAGE_SERVER", "Deleted metadata file: %s", metapath);
        }
    }
    
    // Delete backup file if exists (ignore errors)
    if (access(backuppath, F_OK) == 0) {
        if (unlink(backuppath) != 0) {
            LOG_WARNING_MSG("STORAGE_SERVER", "Failed to delete backup file: %s", backuppath);
        } else {
            LOG_INFO_MSG("STORAGE_SERVER", "Deleted backup file: %s", backuppath);
        }
    }
    
    LOG_INFO_MSG("STORAGE_SERVER", "Successfully deleted file: %s", filename);
    
    // Send success response
    response.status = STATUS_OK;
    snprintf(response.data, sizeof(response.data), 
            "File deleted from storage");
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    send_response(nm_socket, &response);
}

// Phase 3: Create file metadata in .meta file
int create_file_metadata(const char* filename, const char* owner) {
    char metapath[MAX_PATH_LEN];
    snprintf(metapath, sizeof(metapath), "%s/%s.meta", storage_path, filename);
    
    FILE* meta_fp = fopen(metapath, "w");
    if (meta_fp == NULL) {
        LOG_ERROR_MSG("STORAGE_SERVER", "Failed to create metadata file: %s", metapath);
        return -1;
    }
    
    time_t now = time(NULL);
    
    // Write metadata in key=value format
    fprintf(meta_fp, "owner=%s\n", owner);
    fprintf(meta_fp, "created=%ld\n", now);
    fprintf(meta_fp, "modified=%ld\n", now);
    fprintf(meta_fp, "accessed=%ld\n", now);
    fprintf(meta_fp, "accessed_by=%s\n", owner);
    fprintf(meta_fp, "size=0\n");
    fprintf(meta_fp, "word_count=0\n");
    fprintf(meta_fp, "char_count=0\n");
    fprintf(meta_fp, "access_count=1\n");
    fprintf(meta_fp, "access_0=%s:RW\n", owner);  // Owner has read/write
    
    fclose(meta_fp);
    LOG_INFO_MSG("STORAGE_SERVER", "Created metadata file: %s", metapath);
    
    return 0;
}

// Serialize ACL from metadata into a compact string: "user1:RW,user2:R"
char* serialize_acl_from_meta(const file_metadata_t* meta) {
    if (meta == NULL) return NULL;
    // Allocate buffer on heap; caller must free
    size_t buf_sz = MAX_ARGS_LEN;
    char* buf = (char*)calloc(1, buf_sz);
    if (!buf) return NULL;

    size_t offset = 0;
    for (int i = 0; i < meta->access_count; i++) {
        const char* user = meta->access_list[i];
        int perms = meta->access_permissions[i];
        char perm_str[4] = "";
        if ((perms & ACCESS_READ) && (perms & ACCESS_WRITE)) {
            strcpy(perm_str, "RW");
        } else if (perms & ACCESS_READ) {
            strcpy(perm_str, "R");
        } else if (perms & ACCESS_WRITE) {
            strcpy(perm_str, "RW"); // write implies read
        } else {
            strcpy(perm_str, "-");
        }

        int written = snprintf(buf + offset, buf_sz - offset, "%s:%s", user, perm_str);
        if (written < 0 || (size_t)written >= buf_sz - offset) {
            // buffer full
            break;
        }
        offset += written;
        if (i < meta->access_count - 1) {
            if (offset < buf_sz - 1) {
                buf[offset++] = ',';
                buf[offset] = '\0';
            }
        }
    }

    return buf;
}

// Parse acl string "user1:RW,user2:R" into metadata acl arrays
int parse_acl_into_meta(file_metadata_t* meta, const char* acl_str) {
    if (meta == NULL || acl_str == NULL) return -1;

    // Reset existing ACLs
    meta->access_count = 0;

    char buf[MAX_ARGS_LEN];
    strncpy(buf, acl_str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* saveptr1 = NULL;
    char* token = strtok_r(buf, ",", &saveptr1);
    while (token != NULL && meta->access_count < MAX_CLIENTS) {
        // token format: user:PERM
        char* colon = strchr(token, ':');
        if (colon == NULL) {
            token = strtok_r(NULL, ",", &saveptr1);
            continue;
        }
        *colon = '\0';
        const char* user = token;
        const char* perms = colon + 1;

        strncpy(meta->access_list[meta->access_count], user, MAX_USERNAME_LEN - 1);
        meta->access_list[meta->access_count][MAX_USERNAME_LEN - 1] = '\0';

        int perm_bits = ACCESS_NONE;
        if (strstr(perms, "R") != NULL) perm_bits |= ACCESS_READ;
        if (strstr(perms, "W") != NULL) perm_bits |= ACCESS_WRITE;
        if (perm_bits == ACCESS_NONE && strcmp(perms, "-") == 0) {
            perm_bits = ACCESS_NONE;
        }
        // write implies read
        if ((perm_bits & ACCESS_WRITE) && !(perm_bits & ACCESS_READ)) {
            perm_bits |= ACCESS_READ;
        }

        meta->access_permissions[meta->access_count] = perm_bits;
        meta->access_count++;

        token = strtok_r(NULL, ",", &saveptr1);
    }

    return 0;
}

// Handle CMD_UPDATE_ACL from Name Server: args = "<filename> <acl_string>"
void handle_update_acl_request(request_packet_t* req) {
    LOG_INFO_MSG("STORAGE_SERVER", "Handling UPDATE_ACL request from NM: %s by %s", req->args, req->username);

    response_packet_t response;
    memset(&response, 0, sizeof(response));
    response.magic = PROTOCOL_MAGIC;

    // Parse filename and acl string
    char args_copy[MAX_ARGS_LEN];
    strncpy(args_copy, req->args, sizeof(args_copy) - 1);
    args_copy[sizeof(args_copy) - 1] = '\0';

    char* first_space = strchr(args_copy, ' ');
    if (first_space == NULL) {
        response.status = STATUS_ERROR_INVALID_ARGS;
        snprintf(response.data, sizeof(response.data), "Invalid args for UPDATE_ACL");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(nm_socket, &response);
        return;
    }

    *first_space = '\0';
    char* filename = args_copy;
    char* acl_str = first_space + 1;

    // Build meta path
    char metapath[MAX_PATH_LEN];
    snprintf(metapath, sizeof(metapath), "%s/%s.meta", storage_path, filename);

    // Check meta exists
    if (access(metapath, F_OK) != 0) {
        response.status = STATUS_ERROR_NOT_FOUND;
        snprintf(response.data, sizeof(response.data), "Metadata for '%s' not found", filename);
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(nm_socket, &response);
        return;
    }

    // Read current metadata fields (non-ACL) to preserve them
    char owner[MAX_USERNAME_LEN] = "";
    long created = 0, modified = 0, accessed = 0;
    char accessed_by[MAX_USERNAME_LEN] = "";
    size_t size_val = 0;
    int word_count = 0, char_count = 0;

    FILE* mf = fopen(metapath, "r");
    if (mf != NULL) {
        char line[512];
        while (fgets(line, sizeof(line), mf) != NULL) {
            if (strncmp(line, "owner=", 6) == 0) {
                sscanf(line + 6, "%63s", owner);
            } else if (strncmp(line, "created=", 8) == 0) {
                sscanf(line + 8, "%ld", &created);
            } else if (strncmp(line, "modified=", 9) == 0) {
                sscanf(line + 9, "%ld", &modified);
            } else if (strncmp(line, "accessed=", 9) == 0) {
                sscanf(line + 9, "%ld", &accessed);
            } else if (strncmp(line, "accessed_by=", 12) == 0) {
                sscanf(line + 12, "%63s", accessed_by);
            } else if (strncmp(line, "size=", 5) == 0) {
                sscanf(line + 5, "%zu", &size_val);
            } else if (strncmp(line, "word_count=", 11) == 0) {
                sscanf(line + 11, "%d", &word_count);
            } else if (strncmp(line, "char_count=", 11) == 0) {
                sscanf(line + 11, "%d", &char_count);
            }
        }
        fclose(mf);
    }

    // Parse the new ACL into a metadata struct
    file_metadata_t tmp_meta;
    memset(&tmp_meta, 0, sizeof(tmp_meta));
    // Keep owner in metadata too
    if (strlen(owner) > 0) strncpy(tmp_meta.owner, owner, sizeof(tmp_meta.owner) - 1);
    parse_acl_into_meta(&tmp_meta, acl_str);

    // Write the metadata file (overwrite)
    FILE* out = fopen(metapath, "w");
    if (out == NULL) {
        LOG_ERROR_MSG("STORAGE_SERVER", "Failed to open metadata for writing: %s", metapath);
        response.status = STATUS_ERROR_INTERNAL;
        snprintf(response.data, sizeof(response.data), "Failed to write metadata");
        response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
        send_response(nm_socket, &response);
        return;
    }

    fprintf(out, "owner=%s\n", owner);
    fprintf(out, "created=%ld\n", created > 0 ? created : time(NULL));
    fprintf(out, "modified=%ld\n", time(NULL));
    fprintf(out, "accessed=%ld\n", accessed > 0 ? accessed : time(NULL));
    fprintf(out, "accessed_by=%s\n", accessed_by[0] ? accessed_by : owner);
    fprintf(out, "size=%zu\n", size_val);
    fprintf(out, "word_count=%d\n", word_count);
    fprintf(out, "char_count=%d\n", char_count);
    fprintf(out, "access_count=%d\n", tmp_meta.access_count);
    for (int i = 0; i < tmp_meta.access_count; i++) {
        int p = tmp_meta.access_permissions[i];
        if ((p & ACCESS_READ) && (p & ACCESS_WRITE)) {
            fprintf(out, "access_%d=%s:RW\n", i, tmp_meta.access_list[i]);
        } else if (p & ACCESS_READ) {
            fprintf(out, "access_%d=%s:R\n", i, tmp_meta.access_list[i]);
        } else if (p & ACCESS_WRITE) {
            fprintf(out, "access_%d=%s:RW\n", i, tmp_meta.access_list[i]);
        } else {
            fprintf(out, "access_%d=%s:-\n", i, tmp_meta.access_list[i]);
        }
    }

    fclose(out);

    // Send success response back to Name Server
    response.status = STATUS_OK;
    snprintf(response.data, sizeof(response.data), "ACL updated on storage");
    response.checksum = calculate_checksum(&response, sizeof(response) - sizeof(uint32_t));
    send_response(nm_socket, &response);

    LOG_INFO_MSG("STORAGE_SERVER", "Updated ACL for file '%s' successfully", filename);
}

/*
 * Phase 5.3: Sentence lock management functions
 * These functions manage sentence-level locks for concurrent WRITE operations
 */

/**
 * acquire_lock - Attempt to acquire a sentence lock for a file
 * @file: The filename to lock
 * @index: The sentence index to lock
 * @user: The username acquiring the lock
 *
 * Returns: 1 if lock acquired, 0 if lock is already held by someone else
 */
int acquire_lock(const char* file, int index, const char* user) {
    pthread_mutex_lock(&lock_list_mutex);
    
    // Check if lock already exists for this file and sentence
    sentence_lock_t* current = global_lock_list;
    while (current != NULL) {
        if (strcmp(current->filename, file) == 0 && 
            current->sentence_index == index) {
            // Lock exists - check if it's held by a different user
            if (strcmp(current->username, user) != 0) {
                pthread_mutex_unlock(&lock_list_mutex);
                LOG_INFO_MSG("STORAGE_SERVER", 
                    "Lock denied: sentence %d of '%s' held by '%s'", 
                    index, file, current->username);
                return 0;  // Lock held by someone else
            }
            // Same user already holds the lock
            pthread_mutex_unlock(&lock_list_mutex);
            return 1;
        }
        current = current->next;
    }
    
    // Lock is available - create new lock entry
    sentence_lock_t* new_lock = (sentence_lock_t*)malloc(sizeof(sentence_lock_t));
    if (new_lock == NULL) {
        pthread_mutex_unlock(&lock_list_mutex);
        LOG_ERROR_MSG("STORAGE_SERVER", "Failed to allocate memory for lock");
        return 0;
    }
    
    strncpy(new_lock->filename, file, MAX_FILENAME_LEN - 1);
    new_lock->filename[MAX_FILENAME_LEN - 1] = '\0';
    new_lock->sentence_index = index;
    strncpy(new_lock->username, user, MAX_USERNAME_LEN - 1);
    new_lock->username[MAX_USERNAME_LEN - 1] = '\0';
    new_lock->next = global_lock_list;
    global_lock_list = new_lock;
    
    pthread_mutex_unlock(&lock_list_mutex);
    LOG_INFO_MSG("STORAGE_SERVER", 
        "Lock acquired: sentence %d of '%s' by '%s'", 
        index, file, user);
    return 1;
}

/**
 * release_lock - Release a sentence lock held by a user
 * @file: The filename to unlock
 * @index: The sentence index to unlock
 * @user: The username releasing the lock
 */
void release_lock(const char* file, int index, const char* user) {
    pthread_mutex_lock(&lock_list_mutex);
    
    sentence_lock_t* current = global_lock_list;
    sentence_lock_t* prev = NULL;
    
    while (current != NULL) {
        if (strcmp(current->filename, file) == 0 && 
            current->sentence_index == index &&
            strcmp(current->username, user) == 0) {
            // Found the lock - remove it
            if (prev == NULL) {
                global_lock_list = current->next;
            } else {
                prev->next = current->next;
            }
            
            LOG_INFO_MSG("STORAGE_SERVER", 
                "Lock released: sentence %d of '%s' by '%s'", 
                index, file, user);
            free(current);
            pthread_mutex_unlock(&lock_list_mutex);
            return;
        }
        prev = current;
        current = current->next;
    }
    
    // Lock not found
    pthread_mutex_unlock(&lock_list_mutex);
    LOG_WARNING_MSG("STORAGE_SERVER", 
        "Attempted to release non-existent lock: sentence %d of '%s' by '%s'", 
        index, file, user);
}