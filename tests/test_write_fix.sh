#!/bin/bash

# Test script for WRITE functionality with 0-based indexing

# Clean up any existing processes
killall name_server storage_server client 2>/dev/null

# Clean up storage
rm -f storage/mouse.txt*

# Create test file
echo "Hello world. This is a test." > storage/mouse.txt

# Start name server
./bin/name_server 8080 &
NS_PID=$!
sleep 1

# Start storage server  
./bin/storage_server 127.0.0.1 8080 storage/ 8081 &
SS_PID=$!
sleep 1

echo "Testing WRITE command with 0-based indexing..."
echo "Original file content:"
cat storage/mouse.txt
echo ""

# Test multi-word content replacement at word 0 (first word)
echo "Testing: Replace word 0 with 'Im just a mouse.'"
echo -e "testuser\nWRITE mouse.txt 0\n0 Im just a mouse.\nEXIT" | ./bin/client 127.0.0.1 8080

echo "File content after WRITE:"
cat storage/mouse.txt
echo ""

# Cleanup
kill $NS_PID $SS_PID 2>/dev/null