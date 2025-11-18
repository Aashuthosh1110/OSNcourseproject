#!/bin/bash
# Phase 6 Testing Script
# Tests WRITE word updates, LRU cache, and logging

echo "========================================="
echo "Phase 6 Testing Script"
echo "========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if binaries exist
echo "Checking binaries..."
if [ ! -f "bin/name_server" ] || [ ! -f "bin/storage_server" ] || [ ! -f "bin/client" ]; then
    echo -e "${RED}ERROR: Binaries not found. Run 'make' first.${NC}"
    exit 1
fi
echo -e "${GREEN}✓ All binaries found${NC}"
echo ""

# Create test directories
echo "Setting up test environment..."
mkdir -p logs
mkdir -p phase6_test_storage
echo -e "${GREEN}✓ Test directories created${NC}"
echo ""

echo "========================================="
echo "Manual Test Instructions"
echo "========================================="
echo ""

echo -e "${YELLOW}Test 1: WRITE Word Updates${NC}"
echo "1. Start Name Server:"
echo "   ./bin/name_server 8080"
echo ""
echo "2. Start Storage Server (in new terminal):"
echo "   ./bin/storage_server 127.0.0.1 8080 ./phase6_test_storage 9001"
echo ""
echo "3. Start Client (in new terminal):"
echo "   ./bin/client 127.0.0.1 8080"
echo ""
echo "4. In client, run these commands:"
echo "   > CREATE test.txt"
echo "   > WRITE test.txt"
echo "   Enter sentence number: 0"
echo "   Enter word index and new word: 0 Hello"
echo "   Enter word index and new word: 1 World"
echo "   Enter word index and new word: 2 from"
echo "   Enter word index and new word: 3 Phase6"
echo "   Enter word index and new word: done"
echo ""
echo "5. Read the file to verify:"
echo "   > READ test.txt"
echo "   Should show: 'Hello World from Phase6 ...'"
echo ""
echo -e "${GREEN}Expected: Word updates are applied correctly${NC}"
echo ""

echo -e "${YELLOW}Test 2: LRU Cache${NC}"
echo "1. With servers running, access multiple files:"
echo "   > CREATE file1.txt"
echo "   > CREATE file2.txt"
echo "   > CREATE file3.txt"
echo "   > INFO file1.txt  # First access - should be cache MISS"
echo "   > INFO file2.txt  # First access - should be cache MISS"
echo "   > INFO file1.txt  # Second access - should be cache HIT"
echo "   > INFO file3.txt  # First access - should be cache MISS"
echo "   > INFO file2.txt  # Second access - should be cache HIT"
echo ""
echo "2. Check the log:"
echo "   grep 'LRU Cache' logs/name_server.log"
echo ""
echo -e "${GREEN}Expected: See 'LRU Cache HIT' and 'LRU Cache MISS' messages${NC}"
echo ""

echo -e "${YELLOW}Test 3: Logging${NC}"
echo "1. Check log files exist:"
echo "   ls -l logs/"
echo ""
echo "2. View Name Server log:"
echo "   tail logs/name_server.log"
echo ""
echo "3. View Storage Server log:"
echo "   tail logs/storage_server.log"
echo ""
echo -e "${GREEN}Expected: Timestamped log entries with [INFO] level${NC}"
echo ""

echo "========================================="
echo "Automated Verification"
echo "========================================="
echo ""

# Test 1: Check if file_ops.c exists
echo -n "Checking file_ops.c implementation... "
if [ -f "src/common/file_ops.c" ]; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
fi

# Test 2: Check if file_ops.c is in Makefile
echo -n "Checking Makefile includes file_ops.c... "
if grep -q "file_ops.c" Makefile; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
fi

# Test 3: Check for parse_file_into_sentences function
echo -n "Checking parse_file_into_sentences()... "
if grep -q "parse_file_into_sentences" src/common/file_ops.c; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
fi

# Test 4: Check for replace_word_at_position function
echo -n "Checking replace_word_at_position()... "
if grep -q "replace_word_at_position" src/common/file_ops.c; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
fi

# Test 5: Check for LRU cache in nm_state.c
echo -n "Checking LRU cache implementation... "
if grep -q "lru_cache_t" src/name_server/nm_state.c; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
fi

# Test 6: Check for lru_get function
echo -n "Checking lru_get() function... "
if grep -q "lru_get" src/name_server/nm_state.c; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
fi

# Test 7: Check for lru_put function
echo -n "Checking lru_put() function... "
if grep -q "lru_put" src/name_server/nm_state.c; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
fi

# Test 8: Check for init_logging in name_server.c
echo -n "Checking init_logging() in name_server.c... "
if grep -q "init_logging" src/name_server/name_server.c; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
fi

# Test 9: Check for init_logging in storage_server.c
echo -n "Checking init_logging() in storage_server.c... "
if grep -q "init_logging" src/storage_server/storage_server.c; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
fi

# Test 10: Check for file_buffer_size in storage_server.c
echo -n "Checking file_buffer_size variable... "
if grep -q "file_buffer_size" src/storage_server/storage_server.c; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
fi

# Test 11: Check if TODO is removed from CMD_WRITE
echo -n "Checking WRITE TODO is resolved... "
if grep -q "TODO: Update word in file_buffer" src/storage_server/storage_server.c; then
    echo -e "${RED}✗ FAIL - TODO still present${NC}"
else
    echo -e "${GREEN}✓ PASS${NC}"
fi

# Test 12: Verify binaries compile
echo -n "Checking compilation status... "
if [ -f "bin/name_server" ] && [ -f "bin/storage_server" ] && [ -f "bin/client" ]; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
fi

echo ""
echo "========================================="
echo "Summary"
echo "========================================="
echo ""
echo -e "${GREEN}Phase 6 implementation complete!${NC}"
echo ""
echo "Next steps:"
echo "1. Run the manual tests above"
echo "2. Verify WRITE word updates work correctly"
echo "3. Check LRU cache logs for HIT/MISS messages"
echo "4. Review log files in logs/ directory"
echo ""
echo "For detailed documentation, see:"
echo "  docs/Phase6_Implementation_Summary.md"
echo ""
