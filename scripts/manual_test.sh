#!/bin/bash
# Simple manual test for Phase 3

# Colors
GREEN='\033[0;32m'
NC='\033[0m'

echo "=== Phase 3 Manual Test ==="
echo ""

# Cleanup
pkill -f "bin/name_server" || true
pkill -f "bin/storage_server" || true
rm -rf test_storage/
mkdir -p test_storage/ss1
mkdir -p logs

# Start servers
echo "Starting Name Server..."
./bin/name_server 8080 > logs/nm_manual.log 2>&1 &
sleep 2

echo "Starting Storage Server..."
./bin/storage_server localhost 8080 test_storage/ss1 9001 > logs/ss_manual.log 2>&1 &
sleep 2

echo -e "${GREEN}Servers started!${NC}"
echo ""
echo "You can now run the client:"
echo "  ./bin/client localhost 8080"
echo ""
echo "Try these commands:"
echo "  CREATE test1.txt"
echo "  CREATE test2.txt"
echo "  DELETE test1.txt"
echo "  EXIT"
echo ""
echo "To check files: ls -la test_storage/ss1/"
echo ""
echo "To stop servers: pkill -f bin/name_server; pkill -f bin/storage_server"
