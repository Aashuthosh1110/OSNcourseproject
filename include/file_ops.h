#ifndef FILE_OPS_H
#define FILE_OPS_H

#include "common.h"

// File operation types
typedef enum {
    FILE_OP_READ,
    FILE_OP_WRITE,
    FILE_OP_CREATE,
    FILE_OP_DELETE,
    FILE_OP_INFO,
    FILE_OP_STREAM
} file_operation_t;

// Sentence structure
typedef struct {
    char content[MAX_SENTENCE_LEN];
    int word_count;
    int locked;
    char locked_by[MAX_USERNAME_LEN];
    time_t lock_time;
} sentence_t;

// File content structure
typedef struct {
    sentence_t sentences[1000];  // Max 1000 sentences per file
    int sentence_count;
    char backup_content[MAX_CONTENT_LEN];  // For undo functionality
    int has_backup;
} file_content_t;

// File lock structure
typedef struct {
    char filename[MAX_FILENAME_LEN];
    int sentence_index;
    char locked_by[MAX_USERNAME_LEN];
    time_t lock_time;
    struct file_lock* next;
} file_lock_t;

// Function prototypes for file operations
int parse_file_into_sentences(const char* content, file_content_t* file_content);
int serialize_sentences_to_content(const file_content_t* file_content, char* output, size_t max_size);
int insert_word_at_position(sentence_t* sentence, int word_index, const char* word);
int replace_word_at_position(sentence_t* sentence, int word_index, const char* word);
int delete_word_at_position(sentence_t* sentence, int word_index);
int count_words_in_sentence(const char* sentence);
int count_chars_in_sentence(const char* sentence);
int is_sentence_delimiter(char c);
int split_sentence_at_delimiter(const char* input, char sentences[][MAX_SENTENCE_LEN], int max_sentences);

// File locking functions
int lock_sentence(const char* filename, int sentence_index, const char* username);
int unlock_sentence(const char* filename, int sentence_index, const char* username);
int is_sentence_locked(const char* filename, int sentence_index);
char* get_sentence_lock_owner(const char* filename, int sentence_index);

// File metadata functions
int update_file_metadata(file_metadata_t* metadata, const char* operation, const char* username);
int load_file_metadata(const char* filename, file_metadata_t* metadata);
int save_file_metadata(const char* filename, const file_metadata_t* metadata);

// Access control functions
int check_file_access(const file_metadata_t* metadata, const char* username, int required_access);
int add_user_access(file_metadata_t* metadata, const char* username, int access_type);
int remove_user_access(file_metadata_t* metadata, const char* username);
int get_user_access(const file_metadata_t* metadata, const char* username);

// Backup and undo functions
int create_file_backup(const char* filename, const file_content_t* content);
int restore_file_from_backup(const char* filename, file_content_t* content);

#endif // FILE_OPS_H