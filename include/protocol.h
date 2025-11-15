#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "common.h"

// Protocol constants
#define PROTOCOL_VERSION "1.0"
#define HEARTBEAT_INTERVAL 30  // seconds
#define CONNECTION_TIMEOUT 60  // seconds

// Client command enumeration - all possible operations
typedef enum {
    CMD_VIEW = 1,
    CMD_READ,
    CMD_CREATE,
    CMD_WRITE,
    CMD_ETIRW,       // End write operation
    CMD_UNDO,
    CMD_INFO,
    CMD_DELETE,
    CMD_STREAM,
    CMD_LIST,
    CMD_ADDACCESS,
    CMD_REMACCESS,
    CMD_UPDATE_ACL,
    CMD_GET_ACL,
    CMD_EXEC,
    CMD_REGISTER_CLIENT,
    CMD_REGISTER_SS,
    CMD_SS_INIT,          // Storage Server initialization
    CMD_CLIENT_INIT,      // Client initialization  
    CMD_HEARTBEAT
} command_t;

// Status codes for responses - all possible return states
typedef enum {
    STATUS_OK = 0,
    STATUS_ERROR_NOT_FOUND = 1001,
    STATUS_ERROR_UNAUTHORIZED = 1002,
    STATUS_ERROR_LOCKED = 1003,
    STATUS_ERROR_INVALID_ARGS = 1004,
    STATUS_ERROR_SERVER_UNAVAILABLE = 1005,
    STATUS_ERROR_FILE_EXISTS = 1006,
    STATUS_ERROR_INVALID_FILENAME = 1007,
    STATUS_ERROR_INVALID_USERNAME = 1008,
    STATUS_ERROR_SENTENCE_OUT_OF_RANGE = 1009,
    STATUS_ERROR_WORD_OUT_OF_RANGE = 1010,
    STATUS_ERROR_WRITE_PERMISSION = 1011,
    STATUS_ERROR_READ_PERMISSION = 1012,
    STATUS_ERROR_OWNER_REQUIRED = 1013,
    STATUS_ERROR_NETWORK = 1014,
    STATUS_ERROR_STORAGE_FULL = 1015,
    STATUS_ERROR_INVALID_OPERATION = 1016,
    STATUS_ERROR_CONCURRENT_WRITE = 1017,
    STATUS_ERROR_INVALID_FORMAT = 1018,
    STATUS_ERROR_TIMEOUT = 1019,
    STATUS_ERROR_INTERNAL = 1020,
    STATUS_ERROR_USER_NOT_FOUND = 1021,
    STATUS_ERROR_ALREADY_CONNECTED = 1022,
    STATUS_ERROR_NOT_CONNECTED = 1023,
    STATUS_ERROR_UNDO_NOT_AVAILABLE = 1024,
    STATUS_ERROR_EXECUTION_FAILED = 1025
} status_t;

// Request packet structure - client to server
typedef struct {
    uint32_t magic;                     // Protocol magic number for validation
    command_t command;                  // Command type
    char username[MAX_USERNAME_LEN];    // Username of requester
    char args[MAX_ARGS_LEN];           // Flexible payload for filenames, content, etc.
    uint32_t checksum;                  // Simple checksum for integrity
} request_packet_t;

// Response packet structure - server to client  
typedef struct {
    uint32_t magic;                         // Protocol magic number for validation
    status_t status;                        // Response status code
    char data[MAX_RESPONSE_DATA_LEN];      // Flexible payload for file content or error messages
    uint32_t checksum;                      // Simple checksum for integrity
} response_packet_t;

// Function prototypes for protocol handling
int send_packet(int sockfd, request_packet_t* pkt);
int recv_packet(int sockfd, response_packet_t* pkt);
int send_response(int sockfd, response_packet_t* pkt);
int recv_request(int sockfd, request_packet_t* pkt);

// Utility functions for packet creation and validation
uint32_t calculate_checksum(const void* data, size_t len);
int validate_packet_integrity(const void* packet, size_t size);
request_packet_t create_request_packet(command_t cmd, const char* username, const char* args);
response_packet_t create_response_packet(status_t status, const char* data);

// Helper functions for parsing commands and responses
int parse_view_args(const char* args, int* show_all, int* show_details);
int parse_write_args(const char* args, char* filename, int* sentence_index);
int parse_access_args(const char* args, char* filename, char* target_user, int* access_type);

// String conversion utilities
const char* command_to_string(command_t cmd);
const char* status_to_string(status_t status);
command_t string_to_command(const char* str);

#endif // PROTOCOL_H