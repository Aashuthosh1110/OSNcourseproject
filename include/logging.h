#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <time.h>

// Log levels
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_CRITICAL
} log_level_t;

// Log configuration
typedef struct {
    FILE* log_file;
    log_level_t min_level;
    int console_output;
    char log_filename[256];
} log_config_t;

// Global log configuration
extern log_config_t log_config;

// Function prototypes
int init_logging(const char* log_filename, log_level_t min_level, int console_output);
void cleanup_logging();
void log_message_with_level(log_level_t level, const char* component, const char* format, ...);
const char* get_log_level_string(log_level_t level);

// Convenience macros
#define LOG_DEBUG_MSG(component, format, ...) log_message_with_level(LOG_DEBUG, component, format, ##__VA_ARGS__)
#define LOG_INFO_MSG(component, format, ...) log_message_with_level(LOG_INFO, component, format, ##__VA_ARGS__)
#define LOG_WARNING_MSG(component, format, ...) log_message_with_level(LOG_WARNING, component, format, ##__VA_ARGS__)
#define LOG_ERROR_MSG(component, format, ...) log_message_with_level(LOG_ERROR, component, format, ##__VA_ARGS__)
#define LOG_CRITICAL_MSG(component, format, ...) log_message_with_level(LOG_CRITICAL, component, format, ##__VA_ARGS__)

// Request/Response logging macros
#define LOG_REQUEST(client_info, request) \
    LOG_INFO_MSG("REQUEST", "Client: %s@%s:%d | Request: %s", \
                 client_info->username, client_info->ip, client_info->nm_port, request)

#define LOG_RESPONSE(client_info, response, status) \
    LOG_INFO_MSG("RESPONSE", "Client: %s@%s:%d | Response: %s | Status: %d", \
                 client_info->username, client_info->ip, client_info->nm_port, response, status)

#define LOG_SS_OPERATION(ss_info, operation, filename) \
    LOG_INFO_MSG("SS_OP", "StorageServer: %s:%d | Operation: %s | File: %s", \
                 ss_info->ip, ss_info->nm_port, operation, filename)

#endif // LOGGING_H