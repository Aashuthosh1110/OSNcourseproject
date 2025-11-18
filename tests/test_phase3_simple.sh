#!/bin/bash

# Simple Phase 3 Test Script
cd "$(dirname "$0")"

echo "=== Phase 3 Test ==="
echo ""

# Cleanup
echo "Cleaning up..."
pkill -f "bin/name_server" 2>/dev/null || true
pkill -f "bin/storage_server" 2>/dev/null || true
pkill -f "bin/client" 2>/dev/null || true
sleep 1

# Clean directories
rm -rf test_storage/
mkdir -p test_storage/ss1
mkdir -p logs

echo "✓ Environment ready"
echo ""

# Start Name Server
echo "Starting Name Server on port 8080..."
../bin/name_server 8080 &
NM_PID=$!
sleep 2

# Check if NM started
if ! ps -p $NM_PID > /dev/null 2>&1; then
    echo "✗ Name Server failed to start"
    exit 1
fi
echo "✓ Name Server started (PID: $NM_PID)"

# Start Storage Server
echo "Starting Storage Server on port 9001..."
../bin/storage_server localhost 8080 ../test_storage/ss1 9001 &
SS_PID=$!
sleep 2

# Check if SS started
if ! ps -p $SS_PID > /dev/null 2>&1; then
    echo "✗ Storage Server failed to start"
    kill $NM_PID 2>/dev/null
    exit 1
fi
echo "✓ Storage Server started (PID: $SS_PID)"
echo ""

echo "Servers are running!"
echo ""
echo "=== Manual Testing Instructions ==="
echo ""
echo "1. In a NEW terminal, run:"
echo "   cd $(pwd)"
echo "   ../bin/client localhost 8080"
echo ""
echo "2. When prompted for username, enter: testuser"
echo ""
echo "3. Try these commands:"
echo "   CREATE file1.txt"
echo "   CREATE file2.txt"
echo "   CREATE file1.txt    (should fail - already exists)"
echo "   DELETE file1.txt"
echo "   DELETE file1.txt    (should fail - not found)"
echo "   DELETE file2.txt"
echo "   HELP"
echo "   EXIT"
echo ""
echo "4. Verify files were created/deleted:"
echo "   ls -la test_storage/ss1/"
echo ""
echo "5. When done testing, press Ctrl+C here to stop servers"
echo ""

# Wait for user interrupt
trap "echo ''; echo 'Stopping servers...'; kill $NM_PID $SS_PID 2>/dev/null; echo 'Servers stopped.'; exit 0" INT TERM

# Keep script running
while true; do
    sleep 1
done
