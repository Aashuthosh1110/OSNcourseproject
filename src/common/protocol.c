/*
 * Protocol implementation for Docs++ system
 * Handles packet serialization, network communication, and protocol utilities
 */

#include "../include/protocol.h"
#include <sys/socket.h>
#include <string.h>
#include <errno.h>

// Simple checksum calculation (XOR-based for simplicity)
uint32_t calculate_checksum(const void* data, size_t len) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t checksum = 0;
    
    for (size_t i = 0; i < len; i++) {
        checksum ^= bytes[i];
        checksum = (checksum << 1) | (checksum >> 31); // Rotate left
    }
    
    return checksum;
}

// Validate packet integrity using checksum
int validate_packet_integrity(const void* packet, size_t size) {
    if (size < sizeof(uint32_t)) {
        return 0; // Too small to contain checksum
    }
    
    // Extract stored checksum (last 4 bytes)
    const uint8_t* bytes = (const uint8_t*)packet;
    uint32_t stored_checksum = *(uint32_t*)(bytes + size - sizeof(uint32_t));
    
    // Calculate checksum of data (excluding stored checksum)
    uint32_t calculated_checksum = calculate_checksum(packet, size - sizeof(uint32_t));
    
    return stored_checksum == calculated_checksum;
}

// Send a request packet over TCP socket
int send_packet(int sockfd, request_packet_t* pkt) {
    if (sockfd < 0 || pkt == NULL) {
        return -1;
    }
    
    // Set magic number
    pkt->magic = PROTOCOL_MAGIC;
    
    // Calculate checksum for the packet (excluding the checksum field itself)
    pkt->checksum = calculate_checksum(pkt, sizeof(request_packet_t) - sizeof(uint32_t));
    
    // Send the entire packet
    ssize_t total_sent = 0;
    ssize_t packet_size = sizeof(request_packet_t);
    
    while (total_sent < packet_size) {
        ssize_t sent = send(sockfd, (char*)pkt + total_sent, packet_size - total_sent, 0);
        if (sent == -1) {
            if (errno == EINTR) continue; // Interrupted, retry
            return -1; // Error
        }
        total_sent += sent;
    }
    
    return total_sent;
}

// Receive a response packet over TCP socket
int recv_packet(int sockfd, response_packet_t* pkt) {
    if (sockfd < 0 || pkt == NULL) {
        return -1;
    }
    
    // Receive the entire packet
    ssize_t total_received = 0;
    ssize_t packet_size = sizeof(response_packet_t);
    
    while (total_received < packet_size) {
        ssize_t received = recv(sockfd, (char*)pkt + total_received, packet_size - total_received, 0);
        if (received == -1) {
            if (errno == EINTR) continue; // Interrupted, retry
            return -1; // Error
        }
        if (received == 0) {
            return 0; // Connection closed
        }
        total_received += received;
    }
    
    // Validate magic number
    if (pkt->magic != PROTOCOL_MAGIC) {
        return -2; // Invalid magic number
    }
    
    // Validate checksum
    if (!validate_packet_integrity(pkt, sizeof(response_packet_t))) {
        return -3; // Checksum mismatch
    }
    
    return total_received;
}

// Send a response packet over TCP socket
int send_response(int sockfd, response_packet_t* pkt) {
    if (sockfd < 0 || pkt == NULL) {
        return -1;
    }
    
    // Set magic number
    pkt->magic = PROTOCOL_MAGIC;
    
    // Calculate checksum
    pkt->checksum = calculate_checksum(pkt, sizeof(response_packet_t) - sizeof(uint32_t));
    
    // Send the entire packet
    ssize_t total_sent = 0;
    ssize_t packet_size = sizeof(response_packet_t);
    
    while (total_sent < packet_size) {
        ssize_t sent = send(sockfd, (char*)pkt + total_sent, packet_size - total_sent, 0);
        if (sent == -1) {
            if (errno == EINTR) continue; // Interrupted, retry
            return -1; // Error
        }
        total_sent += sent;
    }
    
    return total_sent;
}

// Receive a request packet over TCP socket
int recv_request(int sockfd, request_packet_t* pkt) {
    if (sockfd < 0 || pkt == NULL) {
        return -1;
    }
    
    // Receive the entire packet
    ssize_t total_received = 0;
    ssize_t packet_size = sizeof(request_packet_t);
    
    while (total_received < packet_size) {
        ssize_t received = recv(sockfd, (char*)pkt + total_received, packet_size - total_received, 0);
        if (received == -1) {
            if (errno == EINTR) continue; // Interrupted, retry
            return -1; // Error
        }
        if (received == 0) {
            return 0; // Connection closed
        }
        total_received += received;
    }
    
    // Validate magic number
    if (pkt->magic != PROTOCOL_MAGIC) {
        return -2; // Invalid magic number
    }
    
    // Validate checksum
    if (!validate_packet_integrity(pkt, sizeof(request_packet_t))) {
        return -3; // Checksum mismatch
    }
    
    return total_received;
}

// Create a request packet with given parameters
request_packet_t create_request_packet(command_t cmd, const char* username, const char* args) {
    request_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    
    pkt.magic = PROTOCOL_MAGIC;
    pkt.command = cmd;
    
    if (username) {
        strncpy(pkt.username, username, MAX_USERNAME_LEN - 1);
        pkt.username[MAX_USERNAME_LEN - 1] = '\0';
    }
    
    if (args) {
        strncpy(pkt.args, args, MAX_ARGS_LEN - 1);
        pkt.args[MAX_ARGS_LEN - 1] = '\0';
    }
    
    // Checksum will be calculated in send_packet
    return pkt;
}

// Create a response packet with given parameters
response_packet_t create_response_packet(status_t status, const char* data) {
    response_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    
    pkt.magic = PROTOCOL_MAGIC;
    pkt.status = status;
    
    if (data) {
        strncpy(pkt.data, data, MAX_RESPONSE_DATA_LEN - 1);
        pkt.data[MAX_RESPONSE_DATA_LEN - 1] = '\0';
    }
    
    // Checksum will be calculated in send_response
    return pkt;
}

// Parse VIEW command arguments (-a for all, -l for list)
int parse_view_args(const char* args, int* show_all, int* show_details) {
    if (!args || !show_all || !show_details) {
        return -1;
    }
    
    *show_all = 0;
    *show_details = 0;
    
    if (strlen(args) == 0) {
        return 0; // No flags
    }
    
    // Simple flag parsing
    if (strstr(args, "-a") != NULL) {
        *show_all = 1;
    }
    if (strstr(args, "-l") != NULL) {
        *show_details = 1;
    }
    
    return 0;
}

// Parse WRITE command arguments
int parse_write_args(const char* args, char* filename, int* sentence_index) {
    if (!args || !filename || !sentence_index) {
        return -1;
    }
    
    // Expected format: "filename sentence_index"
    if (sscanf(args, "%255s %d", filename, sentence_index) != 2) {
        return -1;
    }
    
    return 0;
}

// Parse access control command arguments
int parse_access_args(const char* args, char* filename, char* target_user, int* access_type) {
    if (!args || !filename || !target_user || !access_type) {
        return -1;
    }
    
    char flag[4];
    // Expected format: "-R filename username" or "-W filename username"
    if (sscanf(args, "%3s %255s %63s", flag, filename, target_user) != 3) {
        return -1;
    }
    
    if (strcmp(flag, "-R") == 0) {
        *access_type = ACCESS_READ;
    } else if (strcmp(flag, "-W") == 0) {
        *access_type = ACCESS_BOTH; // Write access includes read access
    } else {
        return -1; // Invalid flag
    }
    
    return 0;
}

// Convert command enum to string
const char* command_to_string(command_t cmd) {
    switch (cmd) {
        case CMD_VIEW: return "VIEW";
        case CMD_READ: return "READ";
        case CMD_CREATE: return "CREATE";
        case CMD_WRITE: return "WRITE";
        case CMD_ETIRW: return "ETIRW";
        case CMD_UNDO: return "UNDO";
        case CMD_INFO: return "INFO";
        case CMD_DELETE: return "DELETE";
        case CMD_STREAM: return "STREAM";
        case CMD_LIST: return "LIST";
        case CMD_ADDACCESS: return "ADDACCESS";
        case CMD_REMACCESS: return "REMACCESS";
        case CMD_EXEC: return "EXEC";
        case CMD_REGISTER_CLIENT: return "REGISTER_CLIENT";
        case CMD_REGISTER_SS: return "REGISTER_SS";
        case CMD_HEARTBEAT: return "HEARTBEAT";
        default: return "UNKNOWN";
    }
}

// Convert status enum to string
const char* status_to_string(status_t status) {
    switch (status) {
        case STATUS_OK: return "OK";
        case STATUS_ERROR_NOT_FOUND: return "File not found";
        case STATUS_ERROR_UNAUTHORIZED: return "Access denied";
        case STATUS_ERROR_LOCKED: return "File is locked";
        case STATUS_ERROR_INVALID_ARGS: return "Invalid arguments";
        case STATUS_ERROR_SERVER_UNAVAILABLE: return "Server unavailable";
        case STATUS_ERROR_FILE_EXISTS: return "File already exists";
        case STATUS_ERROR_INVALID_FILENAME: return "Invalid filename";
        case STATUS_ERROR_INVALID_USERNAME: return "Invalid username";
        case STATUS_ERROR_SENTENCE_OUT_OF_RANGE: return "Sentence index out of range";
        case STATUS_ERROR_WORD_OUT_OF_RANGE: return "Word index out of range";
        case STATUS_ERROR_WRITE_PERMISSION: return "Write permission required";
        case STATUS_ERROR_READ_PERMISSION: return "Read permission required";
        case STATUS_ERROR_OWNER_REQUIRED: return "Owner access required";
        case STATUS_ERROR_NETWORK: return "Network error";
        case STATUS_ERROR_STORAGE_FULL: return "Storage full";
        case STATUS_ERROR_INVALID_OPERATION: return "Invalid operation";
        case STATUS_ERROR_CONCURRENT_WRITE: return "Concurrent write detected";
        case STATUS_ERROR_INVALID_FORMAT: return "Invalid format";
        case STATUS_ERROR_TIMEOUT: return "Operation timed out";
        case STATUS_ERROR_INTERNAL: return "Internal server error";
        case STATUS_ERROR_USER_NOT_FOUND: return "User not found";
        case STATUS_ERROR_ALREADY_CONNECTED: return "Already connected";
        case STATUS_ERROR_NOT_CONNECTED: return "Not connected";
        case STATUS_ERROR_UNDO_NOT_AVAILABLE: return "Undo not available";
        case STATUS_ERROR_EXECUTION_FAILED: return "Command execution failed";
        default: return "Unknown error";
    }
}

// Convert string to command enum
command_t string_to_command(const char* str) {
    if (!str) return 0;
    
    if (strcasecmp(str, "VIEW") == 0) return CMD_VIEW;
    if (strcasecmp(str, "READ") == 0) return CMD_READ;
    if (strcasecmp(str, "CREATE") == 0) return CMD_CREATE;
    if (strcasecmp(str, "WRITE") == 0) return CMD_WRITE;
    if (strcasecmp(str, "ETIRW") == 0) return CMD_ETIRW;
    if (strcasecmp(str, "UNDO") == 0) return CMD_UNDO;
    if (strcasecmp(str, "INFO") == 0) return CMD_INFO;
    if (strcasecmp(str, "DELETE") == 0) return CMD_DELETE;
    if (strcasecmp(str, "STREAM") == 0) return CMD_STREAM;
    if (strcasecmp(str, "LIST") == 0) return CMD_LIST;
    if (strcasecmp(str, "ADDACCESS") == 0) return CMD_ADDACCESS;
    if (strcasecmp(str, "REMACCESS") == 0) return CMD_REMACCESS;
    if (strcasecmp(str, "EXEC") == 0) return CMD_EXEC;
    
    return 0; // Unknown command
}