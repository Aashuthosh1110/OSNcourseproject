#ifndef ERRORS_H
#define ERRORS_H

// Error codes for the Docs++ system
typedef enum {
    ERR_SUCCESS = 0,
    ERR_FILE_NOT_FOUND = 1001,
    ERR_ACCESS_DENIED = 1002,
    ERR_FILE_LOCKED = 1003,
    ERR_INVALID_INDEX = 1004,
    ERR_SERVER_UNAVAILABLE = 1005,
    ERR_FILE_EXISTS = 1006,
    ERR_INVALID_FILENAME = 1007,
    ERR_INVALID_USERNAME = 1008,
    ERR_SENTENCE_OUT_OF_RANGE = 1009,
    ERR_WORD_OUT_OF_RANGE = 1010,
    ERR_WRITE_PERMISSION = 1011,
    ERR_READ_PERMISSION = 1012,
    ERR_OWNER_REQUIRED = 1013,
    ERR_NETWORK_ERROR = 1014,
    ERR_STORAGE_FULL = 1015,
    ERR_INVALID_OPERATION = 1016,
    ERR_CONCURRENT_WRITE = 1017,
    ERR_INVALID_FORMAT = 1018,
    ERR_TIMEOUT = 1019,
    ERR_INTERNAL_ERROR = 1020,
    ERR_USER_NOT_FOUND = 1021,
    ERR_ALREADY_CONNECTED = 1022,
    ERR_NOT_CONNECTED = 1023,
    ERR_UNDO_NOT_AVAILABLE = 1024,
    ERR_EXECUTION_FAILED = 1025
} error_code_t;

// Error message strings
extern const char* error_messages[];

// Function prototypes
const char* get_error_message(error_code_t code);
void print_error(error_code_t code, const char* context);

#endif // ERRORS_H