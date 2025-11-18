#!/bin/bash

# Clean test for direct WRITE functionality
cd /home/arpit-mahtele/Desktop/sem-3/OSN/proj/course-project-alphaq

# Clean up
killall name_server storage_server client 2>/dev/null
sleep 1

# Create test content
rm -f storage/mouse.txt*
echo "Hello world. This is a test." > storage/mouse.txt

echo "Original content:"
cat storage/mouse.txt
echo ""

# Start name server
./bin/name_server 8080 &
NS_PID=$!
sleep 1

# Start storage server  
./bin/storage_server 127.0.0.1 8080 storage/ 8081 &
SS_PID=$!
sleep 2

# Now create mouse.txt through the system to get it registered
echo "Creating mouse.txt through the system..."

# Connect client to create mouse.txt first
(echo "testuser"; echo "CREATE mouse.txt"; echo "EXIT") | ./bin/client 127.0.0.1 8080
sleep 1

# Then test WRITE  
echo "Testing WRITE command..."
(echo "testuser"; echo "WRITE mouse.txt 0"; echo "0 Im just a mouse."; echo "EXIT") | ./bin/client 127.0.0.1 8080

sleep 1

echo ""
echo "Final content:"
cat storage/mouse.txt

# Cleanup
kill $NS_PID $SS_PID 2>/dev/null