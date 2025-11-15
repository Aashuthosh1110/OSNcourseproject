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
int create_file_metadata(const char* filename, const char* owner);

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
                
                // Handle client in a separate thread or process
                // For now, handle synchronously
                handle_client_connections();
                close(client_socket);
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

void handle_client_connections() {
    LOG_INFO_MSG("STORAGE_SERVER", "Handling client connection");
    // TODO: Implement client connection handling
    // READ, WRITE, STREAM operations
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