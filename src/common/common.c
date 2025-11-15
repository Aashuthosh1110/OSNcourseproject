#include "../include/common.h"
#include "../include/logging.h"
#include <stdarg.h>

// Implementation of common utility functions

void log_message(const char* level, const char* component, const char* message) {
    char timestamp[64];
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    printf("[%s] [%s] [%s] %s\n", timestamp, level, component, message);
    fflush(stdout);
}



char* get_timestamp() {
    static char timestamp[64];
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    return timestamp;
}

int validate_filename(const char* filename) {
    if (!filename || strlen(filename) == 0 || strlen(filename) >= MAX_FILENAME_LEN) {
        return 0;
    }
    
    // Check for invalid characters
    const char* invalid_chars = "<>:\"|?*";
    size_t invalid_len = strlen(invalid_chars);
    for (size_t i = 0; i < invalid_len; i++) {
        if (strchr(filename, invalid_chars[i]) != NULL) {
            return 0;
        }
    }
    
    // Check for reserved names
    const char* reserved[] = {".", "..", "CON", "PRN", "AUX", "NUL"};
    int reserved_count = sizeof(reserved) / sizeof(reserved[0]);
    
    for (int i = 0; i < reserved_count; i++) {
        if (strcasecmp(filename, reserved[i]) == 0) {
            return 0;
        }
    }
    
    return 1;
}

int validate_username(const char* username) {
    if (!username || strlen(username) == 0 || strlen(username) >= MAX_USERNAME_LEN) {
        return 0;
    }
    
    // Username should only contain alphanumeric characters and underscores
    size_t username_len = strlen(username);
    for (size_t i = 0; i < username_len; i++) {
        char c = username[i];
        if (!((c >= 'a' && c <= 'z') || 
              (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') || 
              c == '_')) {
            return 0;
        }
    }
    
    return 1;
}