#!/bin/bash

echo "Testing fixes for VIEW -al parsing and VIEW -l output formatting..."

# Create test directory
mkdir -p test_fixes_storage
cd test_fixes_storage

# Start name server in background
echo "Starting Name Server..."
../bin/name_server 8080 > /dev/null 2>&1 &
NM_PID=$!
sleep 2

# Start storage server in background 
echo "Starting Storage Server..."
../bin/storage_server 127.0.0.1 8080 ./test_fixes_storage 9001 > /dev/null 2>&1 &
SS_PID=$!
sleep 2

echo "Servers started successfully."
echo "Name Server PID: $NM_PID"
echo "Storage Server PID: $SS_PID"

echo ""
echo "To test the fixes manually:"
echo "1. Run: ../bin/client 127.0.0.1 8080"
echo "2. Enter a username (e.g., testuser)"
echo "3. Test commands:"
echo "   - CREATE testfile.txt"
echo "   - VIEW -al  (should work now - both flags parsed correctly)"
echo "   - VIEW -l   (should show new format with Size, Words, Chars, etc.)"
echo ""
echo "When done testing, run:"
echo "   kill $NM_PID $SS_PID"
echo ""
echo "Press Ctrl+C to stop the servers and exit this script."

# Wait for user interrupt
trap "echo; echo 'Stopping servers...'; kill $NM_PID $SS_PID 2>/dev/null; cd ..; rm -rf test_fixes_storage; echo 'Test environment cleaned up.'; exit" INT

# Keep script running
while true; do
    sleep 1
done