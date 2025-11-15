#include "../include/errors.h"
#include <stdio.h>

// Error message strings corresponding to error codes
const char* error_messages[] = {
    [ERR_SUCCESS] = "Operation completed successfully",
    [ERR_FILE_NOT_FOUND] = "File not found",
    [ERR_ACCESS_DENIED] = "Access denied",
    [ERR_FILE_LOCKED] = "File is currently locked by another user",
    [ERR_INVALID_INDEX] = "Invalid index specified",
    [ERR_SERVER_UNAVAILABLE] = "Storage server unavailable",
    [ERR_FILE_EXISTS] = "File already exists",
    [ERR_INVALID_FILENAME] = "Invalid filename",
    [ERR_INVALID_USERNAME] = "Invalid username",
    [ERR_SENTENCE_OUT_OF_RANGE] = "Sentence index out of range",
    [ERR_WORD_OUT_OF_RANGE] = "Word index out of range",
    [ERR_WRITE_PERMISSION] = "Write permission required",
    [ERR_READ_PERMISSION] = "Read permission required",
    [ERR_OWNER_REQUIRED] = "Owner access required",
    [ERR_NETWORK_ERROR] = "Network communication error",
    [ERR_STORAGE_FULL] = "Storage space full",
    [ERR_INVALID_OPERATION] = "Invalid operation",
    [ERR_CONCURRENT_WRITE] = "Concurrent write detected",
    [ERR_INVALID_FORMAT] = "Invalid format",
    [ERR_TIMEOUT] = "Operation timed out",
    [ERR_INTERNAL_ERROR] = "Internal server error",
    [ERR_USER_NOT_FOUND] = "User not found",
    [ERR_ALREADY_CONNECTED] = "Already connected",
    [ERR_NOT_CONNECTED] = "Not connected",
    [ERR_UNDO_NOT_AVAILABLE] = "Undo operation not available",
    [ERR_EXECUTION_FAILED] = "Command execution failed"
};

const char* get_error_message(error_code_t code) {
    if (code >= 0 && code < sizeof(error_messages) / sizeof(error_messages[0])) {
        return error_messages[code];
    }
    return "Unknown error";
}

void print_error(error_code_t code, const char* context) {
    if (context) {
        fprintf(stderr, "ERROR [%d]: %s - %s\n", code, get_error_message(code), context);
    } else {
        fprintf(stderr, "ERROR [%d]: %s\n", code, get_error_message(code));
    }
}