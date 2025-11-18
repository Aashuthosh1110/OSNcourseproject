/*
 * Name Server State Management Header
 * Function prototypes for managing storage servers, clients, and file mappings
 */

#ifndef NM_STATE_H
#define NM_STATE_H

#include "common.h"

// Hash table functions
unsigned int hash_filename(const char* filename);
void init_file_hash_table(file_hash_table_t* table);
int add_file_to_table(file_hash_table_t* table, const char* filename, int ss_socket_fd, file_metadata_t* metadata);
file_hash_entry_t* find_file_in_table(file_hash_table_t* table, const char* filename);
int remove_file_from_table(file_hash_table_t* table, const char* filename);
void remove_file_from_lru_cache(const char* filename);

// Storage server linked list functions
ss_node_t* add_storage_server(ss_node_t** head, storage_server_info_t* ss_info, int socket_fd);
ss_node_t* find_storage_server_by_fd(ss_node_t* head, int socket_fd);
int remove_storage_server(ss_node_t** head, int socket_fd);
int count_storage_servers(ss_node_t* head);

// Client linked list functions
client_node_t* add_client(client_node_t** head, user_info_t* user_info);
client_node_t* find_client_by_fd(client_node_t* head, int socket_fd);
client_node_t* find_client_by_username(client_node_t* head, const char* username);
int remove_client(client_node_t** head, int socket_fd);
int count_clients(client_node_t* head);

// Persistent user management functions
client_node_t* register_or_reconnect_user(client_node_t** head, user_info_t* user_info);
void disconnect_user(client_node_t* head, int socket_fd);
int count_all_users(client_node_t* head);
void save_user_registry(client_node_t* head);
void load_user_registry(client_node_t** head);

#endif // NM_STATE_H