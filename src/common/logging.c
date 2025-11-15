/*
 * Logging implementation for Docs++ system
 * Provides logging functionality with different log levels
 */

#include "../include/logging.h"
#include "../include/common.h"
#include <stdarg.h>

// Global log configuration
log_config_t log_config = {
    .log_file = NULL,
    .min_level = LOG_INFO,
    .console_output = 1,
    .log_filename = ""
};

// Initialize logging system
int init_logging(const char* log_filename, log_level_t min_level, int console_output) {
    log_config.min_level = min_level;
    log_config.console_output = console_output;
    
    if (log_filename) {
        strncpy(log_config.log_filename, log_filename, sizeof(log_config.log_filename) - 1);
        log_config.log_filename[sizeof(log_config.log_filename) - 1] = '\0';
        
        log_config.log_file = fopen(log_filename, "a");
        if (!log_config.log_file) {
            fprintf(stderr, "Warning: Could not open log file %s\n", log_filename);
            return -1;
        }
    }
    
    return 0;
}

// Cleanup logging system
void cleanup_logging() {
    if (log_config.log_file) {
        fclose(log_config.log_file);
        log_config.log_file = NULL;
    }
}

// Get log level string
const char* get_log_level_string(log_level_t level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO: return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR: return "ERROR";
        case LOG_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

// Log message with specified level
void log_message_with_level(log_level_t level, const char* component, const char* format, ...) {
    if (level < log_config.min_level) {
        return; // Skip logging if below minimum level
    }
    
    char timestamp[64];
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    va_list args;
    va_start(args, format);
    
    char message[1024];
    vsnprintf(message, sizeof(message), format, args);
    
    const char* level_str = get_log_level_string(level);
    
    // Log to console if enabled
    if (log_config.console_output) {
        printf("[%s] [%s] [%s] %s\n", timestamp, level_str, component, message);
    }
    
    // Log to file if available
    if (log_config.log_file) {
        fprintf(log_config.log_file, "[%s] [%s] [%s] %s\n", timestamp, level_str, component, message);
        fflush(log_config.log_file);
    }
    
    va_end(args);
}