#!/bin/bash

# Test script for WRITE command with proper indexing

echo "Testing WRITE command with 0-based indexing and proper ranges..."

# Start name server
./bin/name_server 9001 &
NS_PID=$!
sleep 1

# Start storage server
./bin/storage_server 9001 9002 ./storage &
SS_PID=$!
sleep 1

# Create a test file with some content
echo "Creating a test file..."
echo "First sentence. Second sentence. Third sentence." > ./storage/test_write.txt

# Test sentence indexing (0 to n range - allowing append)
echo "Testing sentence indexing:"
echo "1. Should allow index 0,1,2 for editing existing sentences"
echo "2. Should allow index 3 for appending new sentence"

echo
echo "Expected behavior:"
echo "- Sentences 0,1,2: Edit existing sentences" 
echo "- Sentence 3: Append new sentence"

echo
echo "For words in sentence 0 (First sentence):"
echo "- Words 0,1: Edit existing words (First, sentence)"
echo "- Word 2: Insert at end (before period)"

# Clean up
kill $NS_PID $SS_PID 2>/dev/null
wait

echo "Test setup complete. Run manually to test interactively."