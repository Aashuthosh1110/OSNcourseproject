#include "../include/file_ops.h"
#include "../include/common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/**
 * Check if a character is a sentence delimiter
 */
int is_sentence_delimiter(char c) {
    return (c == '.' || c == '!' || c == '?');
}

/**
 * Parse file content into sentences
 * Splits content by sentence delimiters (. ! ?)
 */
int parse_file_into_sentences(const char* content, file_content_t* file_content) {
    if (!content || !file_content) {
        return -1;
    }

    memset(file_content, 0, sizeof(file_content_t));
    
    int sentence_idx = 0;
    int char_idx = 0;
    int content_len = strlen(content);
    
    for (int i = 0; i < content_len && sentence_idx < 1000; i++) {
        char c = content[i];
        
        // Add character to current sentence
        if (char_idx < MAX_SENTENCE_LEN - 1) {
            file_content->sentences[sentence_idx].content[char_idx++] = c;
        }
        
        // Check if we hit a sentence delimiter
        if (is_sentence_delimiter(c)) {
            // Skip whitespace after delimiter
            while (i + 1 < content_len && isspace(content[i + 1])) {
                i++;
            }
            
            // Null-terminate current sentence
            file_content->sentences[sentence_idx].content[char_idx] = '\0';
            
            // Count words in this sentence
            file_content->sentences[sentence_idx].word_count = 
                count_words_in_sentence(file_content->sentences[sentence_idx].content);
            
            // Move to next sentence
            sentence_idx++;
            char_idx = 0;
        }
    }
    
    // Handle last sentence if it doesn't end with a delimiter
    if (char_idx > 0 && sentence_idx < 1000) {
        file_content->sentences[sentence_idx].content[char_idx] = '\0';
        file_content->sentences[sentence_idx].word_count = 
            count_words_in_sentence(file_content->sentences[sentence_idx].content);
        sentence_idx++;
    }
    
    file_content->sentence_count = sentence_idx;
    return 0;
}

/**
 * Serialize sentences back into content string
 */
int serialize_sentences_to_content(const file_content_t* file_content, char* output, size_t max_size) {
    if (!file_content || !output) {
        return -1;
    }

    size_t offset = 0;
    for (int i = 0; i < file_content->sentence_count; i++) {
        const char* sentence = file_content->sentences[i].content;
        size_t len = strlen(sentence);
        
        if (offset + len >= max_size) {
            return -1;  // Output buffer too small
        }
        
        strcpy(output + offset, sentence);
        offset += len;
        
        // Add space between sentences (except after last sentence)
        if (i < file_content->sentence_count - 1 && offset < max_size - 1) {
            output[offset++] = ' ';
        }
    }
    
    output[offset] = '\0';
    return 0;
}

/**
 * Count words in a sentence
 * Words are separated by whitespace
 */
int count_words_in_sentence(const char* sentence) {
    if (!sentence) return 0;
    
    int count = 0;
    int in_word = 0;
    
    for (int i = 0; sentence[i] != '\0'; i++) {
        if (isspace(sentence[i])) {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            count++;
        }
    }
    
    return count;
}

/**
 * Count characters in a sentence (excluding null terminator)
 */
int count_chars_in_sentence(const char* sentence) {
    if (!sentence) return 0;
    return strlen(sentence);
}

/**
 * Replace word at specified position in a sentence
 * word_index is 0-based
 */
int replace_word_at_position(sentence_t* sentence, int word_index, const char* word) {
    if (!sentence || !word || word_index < 0) {
        return -1;
    }

    char temp[MAX_SENTENCE_LEN];
    char* words[100];  // Max 100 words per sentence
    int word_count = 0;
    
    // Make a copy of the sentence content
    strncpy(temp, sentence->content, MAX_SENTENCE_LEN - 1);
    temp[MAX_SENTENCE_LEN - 1] = '\0';
    
    // Tokenize the sentence into words
    char* token = strtok(temp, " \t\n\r");
    while (token != NULL && word_count < 100) {
        words[word_count++] = token;
        token = strtok(NULL, " \t\n\r");
    }
    
    // Handle empty sentence: allow adding first word at index 0
    // Allow appending words beyond current word count (extend sentence)
    if (word_index < 0) {
        return -1;
    }
    
    // For empty sentences, allow adding word at index 0
    if (word_count == 0 && word_index == 0) {
        // Simply set the word as the entire sentence content
        strncpy(sentence->content, word, MAX_SENTENCE_LEN - 1);
        sentence->content[MAX_SENTENCE_LEN - 1] = '\0';
        sentence->word_count = 1;
        return 0;
    }
    
    // Allow extending sentence by adding words at the end
    if (word_index > word_count) {
        return -1;  // Don't allow gaps
    }
    
    // Build new sentence with replaced/appended word
    char new_content[MAX_SENTENCE_LEN];
    int offset = 0;
    int target_word_count = (word_index == word_count) ? word_count + 1 : word_count;
    
    for (int i = 0; i < target_word_count; i++) {
        const char* current_word;
        if (i == word_index) {
            current_word = word;  // Use new word
        } else if (i < word_count) {
            current_word = words[i];  // Use existing word
        } else {
            continue;  // Skip this iteration for gaps
        }
        
        int len = strlen(current_word);
        
        if (offset + len + 1 >= MAX_SENTENCE_LEN) {
            return -1;  // New sentence too long
        }
        
        strcpy(new_content + offset, current_word);
        offset += len;
        
        // Add space after word (except for last word)
        if (i < target_word_count - 1) {
            new_content[offset++] = ' ';
        }
    }
    
    new_content[offset] = '\0';
    
    // Copy back to sentence
    strncpy(sentence->content, new_content, MAX_SENTENCE_LEN);
    sentence->content[MAX_SENTENCE_LEN - 1] = '\0';
    
    // Update word count
    sentence->word_count = target_word_count;
    
    // Update word count
    sentence->word_count = count_words_in_sentence(sentence->content);
    
    return 0;
}

/**
 * Insert word at specified position in a sentence
 */
int insert_word_at_position(sentence_t* sentence, int word_index, const char* word) {
    if (!sentence || !word || word_index < 0) {
        return -1;
    }

    char temp[MAX_SENTENCE_LEN];
    char* words[100];
    int word_count = 0;
    
    strncpy(temp, sentence->content, MAX_SENTENCE_LEN - 1);
    temp[MAX_SENTENCE_LEN - 1] = '\0';
    
    char* token = strtok(temp, " \t\n\r");
    while (token != NULL && word_count < 100) {
        words[word_count++] = token;
        token = strtok(NULL, " \t\n\r");
    }
    
    if (word_index > word_count) {
        return -1;
    }
    
    char new_content[MAX_SENTENCE_LEN];
    int offset = 0;
    
    for (int i = 0; i <= word_count; i++) {
        const char* current_word;
        
        if (i == word_index) {
            current_word = word;
        } else if (i < word_index) {
            current_word = words[i];
        } else {
            current_word = words[i - 1];
        }
        
        int len = strlen(current_word);
        
        if (offset + len + 1 >= MAX_SENTENCE_LEN) {
            return -1;
        }
        
        strcpy(new_content + offset, current_word);
        offset += len;
        
        if (i < word_count) {
            new_content[offset++] = ' ';
        }
    }
    
    new_content[offset] = '\0';
    strncpy(sentence->content, new_content, MAX_SENTENCE_LEN);
    sentence->content[MAX_SENTENCE_LEN - 1] = '\0';
    sentence->word_count = count_words_in_sentence(sentence->content);
    
    return 0;
}

/**
 * Delete word at specified position in a sentence
 */
int delete_word_at_position(sentence_t* sentence, int word_index) {
    if (!sentence || word_index < 0) {
        return -1;
    }

    char temp[MAX_SENTENCE_LEN];
    char* words[100];
    int word_count = 0;
    
    strncpy(temp, sentence->content, MAX_SENTENCE_LEN - 1);
    temp[MAX_SENTENCE_LEN - 1] = '\0';
    
    char* token = strtok(temp, " \t\n\r");
    while (token != NULL && word_count < 100) {
        words[word_count++] = token;
        token = strtok(NULL, " \t\n\r");
    }
    
    if (word_index >= word_count) {
        return -1;
    }
    
    char new_content[MAX_SENTENCE_LEN];
    int offset = 0;
    
    for (int i = 0; i < word_count; i++) {
        if (i == word_index) {
            continue;  // Skip this word
        }
        
        const char* current_word = words[i];
        int len = strlen(current_word);
        
        if (offset + len + 1 >= MAX_SENTENCE_LEN) {
            return -1;
        }
        
        if (offset > 0) {
            new_content[offset++] = ' ';
        }
        
        strcpy(new_content + offset, current_word);
        offset += len;
    }
    
    new_content[offset] = '\0';
    strncpy(sentence->content, new_content, MAX_SENTENCE_LEN);
    sentence->content[MAX_SENTENCE_LEN - 1] = '\0';
    sentence->word_count = count_words_in_sentence(sentence->content);
    
    return 0;
}

/**
 * Split sentence at delimiter (helper function)
 */
int split_sentence_at_delimiter(const char* input, char sentences[][MAX_SENTENCE_LEN], int max_sentences) {
    if (!input || !sentences) {
        return -1;
    }

    int sentence_idx = 0;
    int char_idx = 0;
    int input_len = strlen(input);
    
    for (int i = 0; i < input_len && sentence_idx < max_sentences; i++) {
        char c = input[i];
        
        if (char_idx < MAX_SENTENCE_LEN - 1) {
            sentences[sentence_idx][char_idx++] = c;
        }
        
        if (is_sentence_delimiter(c)) {
            while (i + 1 < input_len && isspace(input[i + 1])) {
                i++;
            }
            
            sentences[sentence_idx][char_idx] = '\0';
            sentence_idx++;
            char_idx = 0;
        }
    }
    
    if (char_idx > 0 && sentence_idx < max_sentences) {
        sentences[sentence_idx][char_idx] = '\0';
        sentence_idx++;
    }
    
    return sentence_idx;
}

// Stub implementations for functions not needed in Phase 6
// These would be fully implemented if needed

int lock_sentence(const char* filename, int sentence_index, const char* username) {
    // Stub: Actual implementation would use global lock list
    return 0;
}

int unlock_sentence(const char* filename, int sentence_index, const char* username) {
    // Stub: Actual implementation would use global lock list
    return 0;
}

int is_sentence_locked(const char* filename, int sentence_index) {
    // Stub: Actual implementation would check global lock list
    return 0;
}

char* get_sentence_lock_owner(const char* filename, int sentence_index) {
    // Stub: Actual implementation would return username from lock list
    return NULL;
}

int update_file_metadata(file_metadata_t* metadata, const char* operation, const char* username) {
    // Stub: Update last_modified, last_accessed_by
    return 0;
}

int load_file_metadata(const char* filename, file_metadata_t* metadata) {
    // Stub: Load from .meta file
    return 0;
}

int save_file_metadata(const char* filename, const file_metadata_t* metadata) {
    // Stub: Save to .meta file
    return 0;
}

int check_file_access(const file_metadata_t* metadata, const char* username, int required_access) {
    // Stub: Check ACL permissions
    return 1;
}

int add_user_access(file_metadata_t* metadata, const char* username, int access_type) {
    // Stub: Add user to ACL
    return 0;
}

int remove_user_access(file_metadata_t* metadata, const char* username) {
    // Stub: Remove user from ACL
    return 0;
}

int get_user_access(const file_metadata_t* metadata, const char* username) {
    // Stub: Get user's access level
    return 0;
}

int create_file_backup(const char* filename, const file_content_t* content) {
    // Stub: Create .bak file
    return 0;
}

int restore_file_from_backup(const char* filename, file_content_t* content) {
    // Stub: Restore from .bak file
    return 0;
}
