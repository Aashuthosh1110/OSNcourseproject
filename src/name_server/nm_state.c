/*
 * Name Server State Management
 * Implements hash table and linked list operations for managing
 * storage servers, clients, and file mappings
 */

#include "common.h"
#include "protocol.h"
#include "logging.h"
#include <string.h>
#include <stdlib.h>

// Phase 6: LRU Cache for efficient file search
#define LRU_CACHE_CAPACITY 10

typedef struct lru_node {
    char filename[MAX_FILENAME_LEN];
    file_hash_entry_t* entry;
    struct lru_node* prev;
    struct lru_node* next;
} lru_node_t;

typedef struct {
    lru_node_t* head;  // Most recently used
    lru_node_t* tail;  // Least recently used
    int count;
    int capacity;
} lru_cache_t;

// Global LRU cache
static lru_cache_t lru_cache = {NULL, NULL, 0, LRU_CACHE_CAPACITY};

// Hash function for filename strings
unsigned int hash_filename(const char* filename) {
    unsigned int hash = 5381;
    int c;
    
    while ((c = *filename++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    
    return hash % HASH_TABLE_SIZE;
}

// Initialize file hash table
void init_file_hash_table(file_hash_table_t* table) {
    memset(table, 0, sizeof(file_hash_table_t));
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        table->buckets[i] = NULL;
    }
    table->total_files = 0;
}

// Add file to hash table
int add_file_to_table(file_hash_table_t* table, const char* filename, int ss_socket_fd, file_metadata_t* metadata) {
    unsigned int index = hash_filename(filename);
    
    // Check if file already exists
    file_hash_entry_t* current = table->buckets[index];
    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            // File already exists, update it
            current->ss_socket_fd = ss_socket_fd;
            if (metadata) {
                current->metadata = *metadata;
            }
            return 1;
        }
        current = current->next;
    }
    
    // Create new entry
    file_hash_entry_t* new_entry = malloc(sizeof(file_hash_entry_t));
    if (new_entry == NULL) {
        return -1;
    }
    
    strncpy(new_entry->filename, filename, sizeof(new_entry->filename) - 1);
    new_entry->filename[sizeof(new_entry->filename) - 1] = '\0';
    new_entry->ss_socket_fd = ss_socket_fd;
    if (metadata) {
        new_entry->metadata = *metadata;
    } else {
        memset(&new_entry->metadata, 0, sizeof(file_metadata_t));
    }
    
    // Insert at beginning of bucket
    new_entry->next = table->buckets[index];
    table->buckets[index] = new_entry;
    table->total_files++;
    
    return 0;
}

// Phase 6: LRU Cache helper functions

// Remove node from LRU list (but don't free it)
static void lru_remove_node(lru_node_t* node) {
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        lru_cache.head = node->next;
    }
    
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        lru_cache.tail = node->prev;
    }
}

// Add node to front of LRU list (mark as most recently used)
static void lru_add_to_front(lru_node_t* node) {
    node->next = lru_cache.head;
    node->prev = NULL;
    
    if (lru_cache.head) {
        lru_cache.head->prev = node;
    }
    
    lru_cache.head = node;
    
    if (lru_cache.tail == NULL) {
        lru_cache.tail = node;
    }
}

// Get entry from LRU cache
static file_hash_entry_t* lru_get(const char* filename) {
    // Search for node in cache
    lru_node_t* current = lru_cache.head;
    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            // Found! Move to front (mark as recently used)
            lru_remove_node(current);
            lru_add_to_front(current);
            return current->entry;
        }
        current = current->next;
    }
    
    return NULL;  // Cache miss
}

// Put entry into LRU cache
static void lru_put(const char* filename, file_hash_entry_t* entry) {
    // Check if already in cache
    lru_node_t* current = lru_cache.head;
    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            // Update and move to front
            current->entry = entry;
            lru_remove_node(current);
            lru_add_to_front(current);
            return;
        }
        current = current->next;
    }
    
    // Not in cache, create new node
    lru_node_t* new_node = malloc(sizeof(lru_node_t));
    if (new_node == NULL) {
        return;  // Failed to allocate
    }
    
    strncpy(new_node->filename, filename, MAX_FILENAME_LEN - 1);
    new_node->filename[MAX_FILENAME_LEN - 1] = '\0';
    new_node->entry = entry;
    new_node->prev = NULL;
    new_node->next = NULL;
    
    // If cache is full, remove least recently used (tail)
    if (lru_cache.count >= lru_cache.capacity) {
        lru_node_t* old_tail = lru_cache.tail;
        lru_remove_node(old_tail);
        free(old_tail);
        lru_cache.count--;
    }
    
    // Add new node to front
    lru_add_to_front(new_node);
    lru_cache.count++;
}

// Find file in hash table (with LRU cache)
file_hash_entry_t* find_file_in_table(file_hash_table_t* table, const char* filename) {
    // Phase 6: Check LRU cache first
    file_hash_entry_t* cached = lru_get(filename);
    if (cached != NULL) {
        LOG_INFO_MSG("NAME_SERVER", "LRU Cache HIT for file '%s'", filename);
        return cached;
    }
    
    LOG_INFO_MSG("NAME_SERVER", "LRU Cache MISS for file '%s', searching hash table", filename);
    
    // Cache miss, do hash table lookup
    unsigned int index = hash_filename(filename);
    
    file_hash_entry_t* current = table->buckets[index];
    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            // Found in hash table, add to cache
            lru_put(filename, current);
            return current;
        }
        current = current->next;
    }
    
    return NULL;  // Not found
}

// Remove file from hash table
int remove_file_from_table(file_hash_table_t* table, const char* filename) {
    unsigned int index = hash_filename(filename);
    
    file_hash_entry_t* current = table->buckets[index];
    file_hash_entry_t* prev = NULL;
    
    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            if (prev == NULL) {
                table->buckets[index] = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            table->total_files--;
            return 0;
        }
        prev = current;
        current = current->next;
    }
    
    return -1; // File not found
}

// Add storage server to linked list
ss_node_t* add_storage_server(ss_node_t** head, storage_server_info_t* ss_info, int socket_fd) {
    ss_node_t* new_node = malloc(sizeof(ss_node_t));
    if (new_node == NULL) {
        return NULL;
    }
    
    new_node->data = *ss_info;
    new_node->socket_fd = socket_fd;
    new_node->next = *head;
    *head = new_node;
    
    return new_node;
}

// Find storage server by socket FD
ss_node_t* find_storage_server_by_fd(ss_node_t* head, int socket_fd) {
    ss_node_t* current = head;
    while (current != NULL) {
        if (current->socket_fd == socket_fd) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Remove storage server from linked list
int remove_storage_server(ss_node_t** head, int socket_fd) {
    ss_node_t* current = *head;
    ss_node_t* prev = NULL;
    
    while (current != NULL) {
        if (current->socket_fd == socket_fd) {
            if (prev == NULL) {
                *head = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return 0;
        }
        prev = current;
        current = current->next;
    }
    
    return -1; // Not found
}

// Add client to linked list
client_node_t* add_client(client_node_t** head, user_info_t* user_info) {
    client_node_t* new_node = malloc(sizeof(client_node_t));
    if (new_node == NULL) {
        return NULL;
    }
    
    new_node->data = *user_info;
    new_node->next = *head;
    *head = new_node;
    
    return new_node;
}

// Find client by socket FD
client_node_t* find_client_by_fd(client_node_t* head, int socket_fd) {
    client_node_t* current = head;
    while (current != NULL) {
        if (current->data.socket_fd == socket_fd) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Find client by username
client_node_t* find_client_by_username(client_node_t* head, const char* username) {
    client_node_t* current = head;
    while (current != NULL) {
        if (strcmp(current->data.username, username) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Remove client from linked list
int remove_client(client_node_t** head, int socket_fd) {
    client_node_t* current = *head;
    client_node_t* prev = NULL;
    
    while (current != NULL) {
        if (current->data.socket_fd == socket_fd) {
            if (prev == NULL) {
                *head = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return 0;
        }
        prev = current;
        current = current->next;
    }
    
    return -1; // Not found
}

// Count storage servers
int count_storage_servers(ss_node_t* head) {
    int count = 0;
    ss_node_t* current = head;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

// Count clients
int count_clients(client_node_t* head) {
    int count = 0;
    client_node_t* current = head;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}