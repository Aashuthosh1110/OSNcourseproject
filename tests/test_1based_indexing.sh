#!/bin/bash

# Test script for 1-based indexing and empty file support
echo "Testing 1-based indexing and empty file support for WRITE command"
echo "=================================================================="

# Test 1: Create test file with existing content
echo "TEST 1: 1-based indexing with existing content"
echo "This is sentence one. This is sentence two. This is sentence three." > storage/test_indexing.txt

# Create metadata file (assuming owner is 'testuser')
echo "owner=testuser" > storage/test_indexing.txt.meta
echo "created=2025-11-18T10:00:00" >> storage/test_indexing.txt.meta
echo "word_count=12" >> storage/test_indexing.txt.meta
echo "char_count=66" >> storage/test_indexing.txt.meta
echo "size=66" >> storage/test_indexing.txt.meta

echo "File created with content:"
cat storage/test_indexing.txt
echo ""

echo "Test with client:"
echo "1. Connect as 'testuser': ./bin/client 127.0.0.1 8080"
echo "2. Use command: WRITE test_indexing.txt 1   (sentence 1, 1-based)"
echo "3. Try: 1 HELLO   (word 1, 1-based - should replace 'This')"
echo "4. Try: 4 MODIFIED   (word 4, 1-based - should replace 'one.')"  
echo "5. Use: ETIRW"
echo "Expected result: 'HELLO is sentence MODIFIED' for first sentence"
echo ""

# Test 2: Empty file test  
echo "TEST 2: Empty file support"
echo "Empty file already created: storage/empty_test.txt"
ls -la storage/empty_test.txt*
echo ""

echo "Test with client:"
echo "1. Connect as 'testuser': ./bin/client 127.0.0.1 8080"  
echo "2. Use command: WRITE empty_test.txt 1   (sentence 1, 1-based - should work for empty file)"
echo "3. Try: 1 Hello   (word 1, 1-based - should create first word)"
echo "4. Try: 2 world   (word 2, 1-based - should append second word)"
echo "5. Use: ETIRW"
echo "Expected result: Empty file becomes 'Hello world'"
echo ""

echo "IMPORTANT:"
echo "- All sentence numbers are 1-based (first sentence is 1, not 0)"
echo "- All word indices are 1-based (first word is 1, not 0)" 
echo "- Empty files can now be edited by starting with sentence 1"
echo "- Words can be appended to create content from scratch"