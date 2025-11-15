#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <dirent.h>
#include <assert.h>

// Protocol constants
#define MAX_FILENAME_LEN 256
#define MAX_USERNAME_LEN 64
#define MAX_PATH_LEN 1024
#define MAX_CONTENT_LEN 4096
#define MAX_SENTENCE_LEN 1024
#define MAX_WORD_LEN 256
#define MAX_CLIENTS 100
#define MAX_STORAGE_SERVERS 10
#define MAX_FILES_PER_SERVER 1000
#define DEFAULT_PORT 8080
#define BUFFER_SIZE 4096
#define BACKLOG 10
#define MAX_PACKET_SIZE 8192
#define MAX_ARGS_LEN 1024
#define MAX_RESPONSE_DATA_LEN 4096

// File access permissions
#define ACCESS_NONE 0
#define ACCESS_READ 1
#define ACCESS_WRITE 2
#define ACCESS_BOTH (ACCESS_READ | ACCESS_WRITE)

// Protocol magic number for packet validation (0xD0C5 = 53445 decimal)
#define PROTOCOL_MAGIC 0xD0C5

// File metadata structure
typedef struct {
    char filename[MAX_FILENAME_LEN];
    char owner[MAX_USERNAME_LEN];
    time_t created;
    time_t last_modified;
    time_t last_accessed;
    char last_accessed_by[MAX_USERNAME_LEN];
    size_t size;
    int word_count;
    int char_count;
    char access_list[MAX_CLIENTS][MAX_USERNAME_LEN];
    int access_permissions[MAX_CLIENTS];  // Bitmap of ACCESS_READ/ACCESS_WRITE
    int access_count;
} file_metadata_t;

// Storage server info structure
typedef struct {
    char ip[INET_ADDRSTRLEN];
    int nm_port;        // Port for Name Server communication
    int client_port;    // Port for direct client communication
    int active;         // 1 if active, 0 if inactive
    char files[MAX_FILES_PER_SERVER][MAX_FILENAME_LEN];
    int file_count;
    time_t last_heartbeat;
} storage_server_info_t;

// User info structure
typedef struct {
    char username[MAX_USERNAME_LEN];
    char client_ip[INET_ADDRSTRLEN];
    int socket_fd;
    int active;
    time_t connected_time;
} user_info_t;

// Linked list node for storage servers
typedef struct ss_node {
    storage_server_info_t data;
    int socket_fd;
    struct ss_node* next;
} ss_node_t;

// Linked list node for connected clients
typedef struct client_node {
    user_info_t data;
    struct client_node* next;
} client_node_t;

// Hash table for file-to-storage-server mapping
#define HASH_TABLE_SIZE 1024

typedef struct file_hash_entry {
    char filename[MAX_FILENAME_LEN];
    int ss_socket_fd;  // Socket FD of the storage server containing this file
    file_metadata_t metadata;
    struct file_hash_entry* next;
} file_hash_entry_t;

typedef struct {
    file_hash_entry_t* buckets[HASH_TABLE_SIZE];
    int total_files;
} file_hash_table_t;

// Function prototypes for common utilities
void log_message(const char* level, const char* component, const char* message);
char* get_timestamp();
int validate_filename(const char* filename);
int validate_username(const char* username);

#endif // COMMON_H