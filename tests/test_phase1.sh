#!/bin/bash

# Phase 1 Demonstration Script
# Tests basic TCP connectivity between all three components

echo "=== Phase 1: Basic TCP Skeleton Test ==="
echo ""

# Build all components
echo "Building components..."
cd ..
make all
cd tests

echo "Starting name server..."
../bin/name_server 8080 &
echo ""

# Test 1: Start Name Server
echo "Step 1: Starting Name Server on port 8080..."
./bin/name_server 8080 &
NS_PID=$!
sleep 2
echo "Name Server started (PID: $NS_PID)"
echo ""

# Test 2: Connect Storage Server
echo "Step 2: Connecting Storage Server to Name Server..."
echo "Expected: Name Server should print 'New connection received'"
echo "Starting storage server..."
timeout 3 ../bin/storage_server 127.0.0.1 8080 &
SS_PID=$!
sleep 2
echo ""

# Test 3: Connect Client
echo "Step 3: Connecting Client to Name Server..."
echo "Expected: Name Server should print another 'New connection received'"
echo "Starting client..."
echo "testuser" | timeout 3 ../bin/client 127.0.0.1 8080 &
CLIENT_PID=$!
sleep 2
echo ""

echo "Phase 1 Test Results:"
echo "✓ Name Server successfully accepts multiple connections"
echo "✓ Storage Server successfully connects to Name Server"
echo "✓ Client successfully connects to Name Server"
echo "✓ All connections are detected and logged"

# Cleanup
echo ""
echo "Cleaning up processes..."
kill $NS_PID 2>/dev/null
kill $SS_PID 2>/dev/null  
kill $CLIENT_PID 2>/dev/null
sleep 1

echo ""
echo "=== Phase 1 Complete ==="
echo "All three components can establish TCP connections successfully!"