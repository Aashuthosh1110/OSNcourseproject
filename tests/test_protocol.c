/*
 * Protocol Test - Verifies serialization/deserialization of packets
 * Tests the basic protocol functionality before implementing networking
 */

#include "../include/protocol.h"
#include "../include/common.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Test packet creation and validation
void test_packet_creation() {
    printf("Testing packet creation and validation...\n");
    
    // Test request packet creation
    request_packet_t req = create_request_packet(CMD_VIEW, "testuser", "-a -l");
    
    assert(req.magic == PROTOCOL_MAGIC);
    assert(req.command == CMD_VIEW);
    assert(strcmp(req.username, "testuser") == 0);
    assert(strcmp(req.args, "-a -l") == 0);
    
    // Test response packet creation  
    response_packet_t resp = create_response_packet(STATUS_OK, "Test response data");
    
    assert(resp.magic == PROTOCOL_MAGIC);
    assert(resp.status == STATUS_OK);
    assert(strcmp(resp.data, "Test response data") == 0);
    
    printf("✓ Packet creation test passed\n");
}

// Test checksum calculation and validation
void test_checksum() {
    printf("Testing checksum calculation and corruption detection...\n");
    
    // Test 1: Deterministic checksum calculation
    printf("  → Testing deterministic checksums...\n");
    const char* test_data = "Hello, World!";
    size_t data_len = strlen(test_data);
    
    uint32_t checksum1 = calculate_checksum(test_data, data_len);
    uint32_t checksum2 = calculate_checksum(test_data, data_len);
    
    // Same data should produce same checksum
    assert(checksum1 == checksum2);
    
    // Different data should produce different checksum (usually)
    const char* test_data2 = "Hello, World?";
    uint32_t checksum3 = calculate_checksum(test_data2, strlen(test_data2));
    assert(checksum1 != checksum3);
    
    printf("  → Deterministic checksums: ✓\n");
    
    // Test 2: Corruption detection in packets
    printf("  → Testing corruption detection...\n");
    request_packet_t packet = create_request_packet(CMD_VIEW, "testuser", "-a -l");
    
    // Calculate checksum for packet (excluding checksum field)
    packet.checksum = calculate_checksum(&packet, sizeof(packet) - sizeof(uint32_t));
    
    // Verify packet integrity
    assert(validate_packet_integrity(&packet, sizeof(packet)) == 1);
    
    // Simulate corruption by changing a byte
    char* packet_bytes = (char*)&packet;
    char original_byte = packet_bytes[10];
    packet_bytes[10] ^= 0xFF; // Flip all bits
    
    // Should now detect corruption
    assert(validate_packet_integrity(&packet, sizeof(packet)) == 0);
    
    // Restore original byte
    packet_bytes[10] = original_byte;
    
    // Should pass again
    assert(validate_packet_integrity(&packet, sizeof(packet)) == 1);
    
    printf("  → Corruption detection: ✓\n");
    
    printf("✓ Checksum test passed\n");
}

// Test packet serialization (simulated without network)
void test_packet_serialization() {
    printf("Testing packet serialization and deserialization...\n");
    
    // Test 1: Request packet serialization
    printf("  → Testing request packet serialization...\n");
    request_packet_t original_req = create_request_packet(CMD_READ, "alice", "document.txt");
    
    // Simulate serialization by copying to buffer
    char buffer[sizeof(request_packet_t)];
    memcpy(buffer, &original_req, sizeof(original_req));
    
    // Simulate deserialization by copying from buffer
    request_packet_t deserialized_req;
    memcpy(&deserialized_req, buffer, sizeof(deserialized_req));
    
    // Verify all fields are identical
    assert(deserialized_req.magic == original_req.magic);
    assert(deserialized_req.command == original_req.command);
    assert(strcmp(deserialized_req.username, original_req.username) == 0);
    assert(strcmp(deserialized_req.args, original_req.args) == 0);
    assert(deserialized_req.checksum == original_req.checksum);
    
    printf("  → Request packet serialization: ✓\n");
    
    // Test 2: Response packet serialization
    printf("  → Testing response packet serialization...\n");
    response_packet_t original_resp = create_response_packet(STATUS_ERROR_NOT_FOUND, "File not found: test.txt");
    
    // Simulate serialization by copying to buffer
    char resp_buffer[sizeof(response_packet_t)];
    memcpy(resp_buffer, &original_resp, sizeof(original_resp));
    
    // Simulate deserialization by copying from buffer
    response_packet_t deserialized_resp;
    memcpy(&deserialized_resp, resp_buffer, sizeof(deserialized_resp));
    
    // Verify all fields are identical
    assert(deserialized_resp.magic == original_resp.magic);
    assert(deserialized_resp.status == original_resp.status);
    assert(strcmp(deserialized_resp.data, original_resp.data) == 0);
    assert(deserialized_resp.checksum == original_resp.checksum);
    
    printf("  → Response packet serialization: ✓\n");
    
    // Test 3: Multiple round-trip serialization
    printf("  → Testing multiple round-trip serialization...\n");
    for (int i = 0; i < 5; i++) {
        char test_args[64];
        snprintf(test_args, sizeof(test_args), "test_file_%d.txt", i);
        
        request_packet_t req = create_request_packet(CMD_CREATE, "testuser", test_args);
        
        // Serialize
        char temp_buffer[sizeof(request_packet_t)];
        memcpy(temp_buffer, &req, sizeof(req));
        
        // Deserialize
        request_packet_t restored_req;
        memcpy(&restored_req, temp_buffer, sizeof(restored_req));
        
        // Verify
        assert(restored_req.magic == req.magic);
        assert(restored_req.command == req.command);
        assert(strcmp(restored_req.username, req.username) == 0);
        assert(strcmp(restored_req.args, req.args) == 0);
        assert(restored_req.checksum == req.checksum);
    }
    printf("  → Multiple round-trip serialization: ✓\n");
    
    printf("✓ Packet serialization test passed\n");
}

// Test command parsing functions
void test_command_parsing() {
    printf("Testing command parsing functions...\n");
    
    // Test VIEW command parsing
    int show_all, show_details;
    assert(parse_view_args("-a", &show_all, &show_details) == 0);
    assert(show_all == 1 && show_details == 0);
    
    assert(parse_view_args("-l", &show_all, &show_details) == 0);
    assert(show_all == 0 && show_details == 1);
    
    assert(parse_view_args("-a -l", &show_all, &show_details) == 0);
    assert(show_all == 1 && show_details == 1);
    
    // Test WRITE command parsing
    char filename[MAX_FILENAME_LEN];
    int sentence_index;
    assert(parse_write_args("doc.txt 5", filename, &sentence_index) == 0);
    assert(strcmp(filename, "doc.txt") == 0);
    assert(sentence_index == 5);
    
    // Test access control parsing
    char target_user[MAX_USERNAME_LEN];
    int access_type;
    assert(parse_access_args("-R file.txt bob", filename, target_user, &access_type) == 0);
    assert(strcmp(filename, "file.txt") == 0);
    assert(strcmp(target_user, "bob") == 0);
    assert(access_type == ACCESS_READ);
    
    assert(parse_access_args("-W file.txt bob", filename, target_user, &access_type) == 0);
    assert(access_type == ACCESS_BOTH);
    
    printf("✓ Command parsing test passed\n");
}

// Test string conversion functions
void test_string_conversions() {
    printf("Testing string conversion functions...\n");
    
    // Test command to string conversion
    assert(strcmp(command_to_string(CMD_VIEW), "VIEW") == 0);
    assert(strcmp(command_to_string(CMD_READ), "READ") == 0);
    assert(strcmp(command_to_string(CMD_CREATE), "CREATE") == 0);
    
    // Test status to string conversion
    assert(strcmp(status_to_string(STATUS_OK), "OK") == 0);
    assert(strcmp(status_to_string(STATUS_ERROR_NOT_FOUND), "File not found") == 0);
    assert(strcmp(status_to_string(STATUS_ERROR_UNAUTHORIZED), "Access denied") == 0);
    
    // Test string to command conversion
    assert(string_to_command("VIEW") == CMD_VIEW);
    assert(string_to_command("READ") == CMD_READ);
    assert(string_to_command("CREATE") == CMD_CREATE);
    
    // Case insensitive test
    assert(string_to_command("view") == CMD_VIEW);
    assert(string_to_command("read") == CMD_READ);
    
    printf("✓ String conversion test passed\n");
}

// Test edge cases and error conditions
void test_edge_cases() {
    printf("Testing edge cases...\n");
    
    // Test NULL parameters
    assert(parse_view_args(NULL, NULL, NULL) == -1);
    assert(parse_write_args(NULL, NULL, NULL) == -1);
    assert(parse_access_args(NULL, NULL, NULL, NULL) == -1);
    
    // Test invalid command strings
    assert(string_to_command(NULL) == 0);
    assert(string_to_command("INVALID") == 0);
    assert(string_to_command("") == 0);
    
    // Test malformed arguments
    char filename[MAX_FILENAME_LEN];
    int sentence_index;
    assert(parse_write_args("only_filename", filename, &sentence_index) == -1);
    assert(parse_write_args("", filename, &sentence_index) == -1);
    
    char target_user[MAX_USERNAME_LEN];
    int access_type;
    assert(parse_access_args("-X file.txt bob", filename, target_user, &access_type) == -1);
    assert(parse_access_args("R file.txt bob", filename, target_user, &access_type) == -1);
    
    printf("✓ Edge cases test passed\n");
}

// Main test function
int main() {
    printf("=== Docs++ Protocol Test Suite ===\n\n");
    
    test_packet_creation();
    test_checksum();
    test_packet_serialization();
    test_command_parsing();
    test_string_conversions();
    test_edge_cases();
    
    printf("\n=== All Protocol Tests Passed! ===\n");
    printf("The protocol implementation is working correctly.\n");
    printf("Ready to proceed with networking implementation.\n");
    
    return 0;
}