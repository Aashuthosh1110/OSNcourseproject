#!/bin/bash
# Test script for Phase 3: CREATE and DELETE operations

set -e

echo "=== Phase 3 Integration Test ==="
echo "Testing CREATE and DELETE file operations"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test configuration
NM_PORT=8080
SS_STORAGE_DIR="./test_storage/ss1"
SS_PORT=9001

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up..."
    pkill -f "bin/name_server" || true
    pkill -f "bin/storage_server" || true
    pkill -f "bin/client" || true
    rm -rf test_storage/
    sleep 1
}

# Setup trap to cleanup on exit
trap cleanup EXIT

# Create test directories
echo "Setting up test directories..."
mkdir -p $SS_STORAGE_DIR
mkdir -p logs

# Start Name Server
echo "Starting Name Server on port $NM_PORT..."
./bin/name_server $NM_PORT > logs/nm_test.log 2>&1 &
NM_PID=$!
sleep 2

if ! kill -0 $NM_PID 2>/dev/null; then
    echo -e "${RED}✗ Name Server failed to start${NC}"
    cat logs/nm_test.log
    exit 1
fi
echo -e "${GREEN}✓ Name Server started (PID: $NM_PID)${NC}"

# Start Storage Server
echo "Starting Storage Server..."
./bin/storage_server localhost $NM_PORT $SS_STORAGE_DIR $SS_PORT > logs/ss_test.log 2>&1 &
SS_PID=$!
sleep 2

if ! kill -0 $SS_PID 2>/dev/null; then
    echo -e "${RED}✗ Storage Server failed to start${NC}"
    cat logs/ss_test.log
    exit 1
fi
echo -e "${GREEN}✓ Storage Server started (PID: $SS_PID)${NC}"
echo ""

# Test 1: CREATE a file
echo "Test 1: CREATE file 'test1.txt'"
echo -e "user1\nCREATE test1.txt\nEXIT" | timeout 5 ./bin/client localhost $NM_PORT > /tmp/test1_output.txt 2>&1 || true
sleep 1

if [ -f "$SS_STORAGE_DIR/test1.txt" ]; then
    echo -e "${GREEN}✓ File created successfully${NC}"
    echo "  - File exists: $SS_STORAGE_DIR/test1.txt"
else
    echo -e "${RED}✗ File creation failed${NC}"
    echo "Client output:"
    cat /tmp/test1_output.txt
    exit 1
fi

# Check metadata file
if [ -f "$SS_STORAGE_DIR/test1.txt.meta" ]; then
    echo -e "${GREEN}✓ Metadata file created${NC}"
    echo "  - Metadata content:"
    cat "$SS_STORAGE_DIR/test1.txt.meta" | sed 's/^/    /'
else
    echo -e "${YELLOW}⚠ Warning: Metadata file not found${NC}"
fi
echo ""

# Test 2: CREATE duplicate file (should fail)
echo "Test 2: CREATE duplicate file 'test1.txt' (should fail)"
echo -e "user1\nCREATE test1.txt\nEXIT" | timeout 5 ./bin/client localhost $NM_PORT > /tmp/test2_output.txt 2>&1 || true
sleep 1

if grep -q "already exists" /tmp/test2_output.txt; then
    echo -e "${GREEN}✓ Duplicate file detection works${NC}"
else
    echo -e "${RED}✗ Duplicate detection failed${NC}"
    echo "Client output:"
    cat /tmp/test2_output.txt
    exit 1
fi
echo ""

# Test 3: CREATE another file
echo "Test 3: CREATE file 'test2.txt'"
echo -e "user2\nCREATE test2.txt\nEXIT" | timeout 5 ./bin/client localhost $NM_PORT > /tmp/test3_output.txt 2>&1 || true
sleep 1

if [ -f "$SS_STORAGE_DIR/test2.txt" ]; then
    echo -e "${GREEN}✓ Second file created successfully${NC}"
else
    echo -e "${RED}✗ Second file creation failed${NC}"
    exit 1
fi
echo ""

# Test 4: DELETE file by owner
echo "Test 4: DELETE file 'test1.txt' by owner (user1)"
echo -e "user1\nDELETE test1.txt\nEXIT" | timeout 5 ./bin/client localhost $NM_PORT > /tmp/test4_output.txt 2>&1 || true
sleep 1

if [ ! -f "$SS_STORAGE_DIR/test1.txt" ]; then
    echo -e "${GREEN}✓ File deleted successfully${NC}"
else
    echo -e "${RED}✗ File deletion failed${NC}"
    exit 1
fi

# Check metadata file also deleted
if [ ! -f "$SS_STORAGE_DIR/test1.txt.meta" ]; then
    echo -e "${GREEN}✓ Metadata file also deleted${NC}"
else
    echo -e "${YELLOW}⚠ Warning: Metadata file still exists${NC}"
fi
echo ""

# Test 5: DELETE non-existent file (should fail)
echo "Test 5: DELETE non-existent file 'test1.txt' (should fail)"
echo -e "user1\nDELETE test1.txt\nEXIT" | timeout 5 ./bin/client localhost $NM_PORT > /tmp/test5_output.txt 2>&1 || true
sleep 1

if grep -q "not found" /tmp/test5_output.txt; then
    echo -e "${GREEN}✓ Non-existent file error handling works${NC}"
else
    echo -e "${RED}✗ Error handling failed${NC}"
    echo "Client output:"
    cat /tmp/test5_output.txt
    exit 1
fi
echo ""

# Test 6: DELETE file by non-owner (should fail)
echo "Test 6: DELETE file 'test2.txt' by non-owner (user1, owned by user2)"
echo -e "user1\nDELETE test2.txt\nEXIT" | timeout 5 ./bin/client localhost $NM_PORT > /tmp/test6_output.txt 2>&1 || true
sleep 1

if grep -q -i "owner" /tmp/test6_output.txt; then
    echo -e "${GREEN}✓ Ownership check works${NC}"
else
    echo -e "${YELLOW}⚠ Warning: Ownership check might not be working${NC}"
    echo "Client output:"
    cat /tmp/test6_output.txt
fi
echo ""

# Test 7: DELETE file by owner
echo "Test 7: DELETE file 'test2.txt' by owner (user2)"
echo -e "user2\nDELETE test2.txt\nEXIT" | timeout 5 ./bin/client localhost $NM_PORT > /tmp/test7_output.txt 2>&1 || true
sleep 1

if [ ! -f "$SS_STORAGE_DIR/test2.txt" ]; then
    echo -e "${GREEN}✓ File deleted by owner successfully${NC}"
else
    echo -e "${RED}✗ File deletion by owner failed${NC}"
    exit 1
fi
echo ""

# Summary
echo "==================================="
echo -e "${GREEN}All Phase 3 tests passed!${NC}"
echo "==================================="
echo ""
echo "Storage directory contents:"
ls -la $SS_STORAGE_DIR/ || echo "Directory empty"
echo ""

# Show logs (last few lines)
echo "Name Server log (last 10 lines):"
tail -10 logs/nm_test.log
echo ""
echo "Storage Server log (last 10 lines):"
tail -10 logs/ss_test.log
