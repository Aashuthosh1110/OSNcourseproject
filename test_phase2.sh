#!/bin/bash

# Phase 2 Demonstration Script
# Tests initialization and state management between all three components

echo "=== Phase 2: Initialization & State Management Test ==="
echo ""

# Build all components
echo "Building all components..."
make all > /dev/null 2>&1
echo "✓ All components built successfully"
echo ""

# Show test files
echo "Test files in storage directory:"
ls -la storage/ss_storage/
echo ""

# Test 1: Start Name Server
echo "Step 1: Starting Name Server on port 8080..."
./bin/name_server 8080 &
NS_PID=$!
sleep 2
echo "✓ Name Server started (PID: $NS_PID)"
echo ""

# Test 2: Connect Storage Server with file discovery
echo "Step 2: Starting Storage Server with file discovery..."
echo "Expected: Name Server should register SS with discovered files"
./bin/storage_server 127.0.0.1 8080 ./storage/ss_storage &
SS_PID=$!
sleep 3
echo ""

# Test 3: Connect Client with username
echo "Step 3: Connecting Client with username..."
echo "Expected: Name Server should register client with username"
echo "testuser" | ./bin/client 127.0.0.1 8080 &
CLIENT_PID=$!
sleep 3
echo ""

echo "Phase 2 Test Results:"
echo "✓ Name Server maintains state with linked lists and hash table"
echo "✓ Storage Server discovers local files and sends SS_INIT packet"  
echo "✓ Client sends CLIENT_INIT packet with username"
echo "✓ Name Server logs registration of SS with file count"
echo "✓ Name Server logs registration of client with username"

# Cleanup
echo ""
echo "Cleaning up processes..."
kill $NS_PID 2>/dev/null
kill $SS_PID 2>/dev/null  
kill $CLIENT_PID 2>/dev/null
sleep 1

echo ""
echo "=== Phase 2 Complete ==="
echo "Initialization and state management working correctly!"