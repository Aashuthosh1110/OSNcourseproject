#!/bin/bash

# Phase 5 Interactive Testing Script
# Tests READ, STREAM, WRITE, ETIRW, UNDO, and EXEC operations

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
NM_PORT=8080
SS_PORT=9001
STORAGE_DIR="ss_storage_test"
TEST_USER="alice"
TEST_USER2="bob"

echo -e "${BLUE}╔════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     Phase 5 Interactive Testing Script    ║${NC}"
echo -e "${BLUE}╔════════════════════════════════════════════╗${NC}"
echo ""

# Function to check if servers are running
check_servers() {
    echo -e "${YELLOW}Checking if servers are running...${NC}"
    
    # Check Name Server
    if ! pgrep -f "bin/name_server" > /dev/null; then
        echo -e "${RED}✗ Name Server not running${NC}"
        echo "  Please start: ./bin/name_server $NM_PORT"
        return 1
    else
        echo -e "${GREEN}✓ Name Server running${NC}"
    fi
    
    # Check Storage Server
    if ! pgrep -f "bin/storage_server" > /dev/null; then
        echo -e "${RED}✗ Storage Server not running${NC}"
        echo "  Please start: ./bin/storage_server 127.0.0.1 $NM_PORT $STORAGE_DIR $SS_PORT"
        return 1
    else
        echo -e "${GREEN}✓ Storage Server running${NC}"
    fi
    
    return 0
}

# Setup test environment
setup_test_env() {
    echo -e "\n${YELLOW}Setting up test environment...${NC}"
    
    # Create storage directory
    mkdir -p "$STORAGE_DIR"
    
    # Create test files
    echo "This is a test file for READ operation." > "$STORAGE_DIR/read_test.txt"
    echo "Line 1" >> "$STORAGE_DIR/read_test.txt"
    echo "Line 2" >> "$STORAGE_DIR/read_test.txt"
    echo "Line 3" >> "$STORAGE_DIR/read_test.txt"
    
    echo "Word1 Word2 Word3 Word4 Word5 Word6 Word7 Word8 Word9 Word10" > "$STORAGE_DIR/stream_test.txt"
    
    echo "Original content for UNDO test" > "$STORAGE_DIR/undo_test.txt"
    
    echo "Word0 Word1 Word2 Word3 Word4" > "$STORAGE_DIR/write_test.txt"
    
    # Create test script
    cat > "$STORAGE_DIR/test_script.sh" << 'EOF'
#!/bin/bash
echo "=== EXEC Test Script ==="
echo "Hello from executed script!"
echo "Current date: $(date)"
echo "Current user: $(whoami)"
echo "Script completed successfully"
EOF
    chmod +x "$STORAGE_DIR/test_script.sh"
    
    echo -e "${GREEN}✓ Test files created in $STORAGE_DIR${NC}"
}

# Test READ operation
test_read() {
    echo -e "\n${BLUE}═══════════════════════════════════════${NC}"
    echo -e "${BLUE}        TEST 1: READ Operation         ${NC}"
    echo -e "${BLUE}═══════════════════════════════════════${NC}"
    
    echo ""
    echo "This test will:"
    echo "1. Create metadata for read_test.txt"
    echo "2. READ the file (should display content)"
    echo "3. Test permission denied with another user"
    echo ""
    echo "File content:"
    cat "$STORAGE_DIR/read_test.txt"
    echo ""
    echo -e "${YELLOW}Instructions:${NC}"
    echo "  1. Connect as '$TEST_USER': ./bin/client $TEST_USER 127.0.0.1 $NM_PORT"
    echo "  2. Run: CREATE read_test.txt"
    echo "  3. Run: READ read_test.txt"
    echo "  4. Verify file content displays correctly"
    echo ""
    echo "  5. Open new terminal as '$TEST_USER2': ./bin/client $TEST_USER2 127.0.0.1 $NM_PORT"
    echo "  6. Run: READ read_test.txt (should fail - no permission)"
    echo "  7. From $TEST_USER client: ADDACCESS -R read_test.txt $TEST_USER2"
    echo "  8. From $TEST_USER2 client: READ read_test.txt (should succeed)"
    echo ""
    echo -e "${GREEN}Expected: File content displays, permissions enforced${NC}"
    echo ""
    read -p "Press Enter when test complete..."
}

# Test STREAM operation
test_stream() {
    echo -e "\n${BLUE}═══════════════════════════════════════${NC}"
    echo -e "${BLUE}       TEST 2: STREAM Operation        ${NC}"
    echo -e "${BLUE}═══════════════════════════════════════${NC}"
    
    echo ""
    echo "This test will:"
    echo "1. Create metadata for stream_test.txt"
    echo "2. STREAM the file (words appear with 0.1s delays)"
    echo ""
    echo "File content:"
    cat "$STORAGE_DIR/stream_test.txt"
    echo ""
    echo -e "${YELLOW}Instructions:${NC}"
    echo "  1. From $TEST_USER client: CREATE stream_test.txt"
    echo "  2. Run: STREAM stream_test.txt"
    echo "  3. Watch words appear one by one with delay"
    echo ""
    echo -e "${GREEN}Expected: Words display sequentially with ~0.1s delay${NC}"
    echo ""
    read -p "Press Enter when test complete..."
}

# Test WRITE operation
test_write() {
    echo -e "\n${BLUE}═══════════════════════════════════════${NC}"
    echo -e "${BLUE}    TEST 3: WRITE Operation (Locking)  ${NC}"
    echo -e "${BLUE}═══════════════════════════════════════${NC}"
    
    echo ""
    echo "This test will:"
    echo "1. Create metadata for write_test.txt"
    echo "2. Acquire lock and modify words"
    echo "3. Test lock contention"
    echo "4. Save with ETIRW"
    echo ""
    echo "Original content:"
    cat "$STORAGE_DIR/write_test.txt"
    echo ""
    echo -e "${YELLOW}Instructions:${NC}"
    echo "  1. From $TEST_USER client: CREATE write_test.txt"
    echo "  2. Run: WRITE write_test.txt 0"
    echo "     Should show: 'Lock acquired for sentence 0'"
    echo ""
    echo "  3. Update some words:"
    echo "     WRITE> 0 Hello"
    echo "     WRITE> 1 Beautiful"
    echo "     WRITE> 2 World"
    echo ""
    echo "  4. From $TEST_USER2 client (grant access first):"
    echo "     From alice: ADDACCESS -W write_test.txt $TEST_USER2"
    echo "     From bob: WRITE write_test.txt 0"
    echo "     Should show: 'Error: Sentence 0 is locked by alice'"
    echo ""
    echo "  5. From $TEST_USER client: WRITE> ETIRW"
    echo "     Should show: 'Changes saved successfully'"
    echo ""
    echo "  6. Verify backup created:"
    echo "     ls -la $STORAGE_DIR/write_test.txt.bak"
    echo ""
    echo -e "${GREEN}Expected: Lock prevents concurrent writes, ETIRW creates backup${NC}"
    echo ""
    read -p "Press Enter when test complete..."
}

# Test UNDO operation
test_undo() {
    echo -e "\n${BLUE}═══════════════════════════════════════${NC}"
    echo -e "${BLUE}        TEST 4: UNDO Operation         ${NC}"
    echo -e "${BLUE}═══════════════════════════════════════${NC}"
    
    echo ""
    echo "This test will:"
    echo "1. Create metadata for undo_test.txt"
    echo "2. Modify the file with WRITE"
    echo "3. Restore original with UNDO"
    echo ""
    echo "Original content:"
    cat "$STORAGE_DIR/undo_test.txt"
    echo ""
    echo -e "${YELLOW}Instructions:${NC}"
    echo "  1. From $TEST_USER client: CREATE undo_test.txt"
    echo "  2. Run: WRITE undo_test.txt 0"
    echo "     WRITE> 0 Modified"
    echo "     WRITE> 1 Content"
    echo "     WRITE> ETIRW"
    echo ""
    echo "  3. Verify changes: READ undo_test.txt"
    echo "     Should show: 'Modified Content ...'"
    echo ""
    echo "  4. Run: UNDO undo_test.txt"
    echo "     Should show: 'File restored from backup'"
    echo ""
    echo "  5. Verify restoration: READ undo_test.txt"
    echo "     Should show original: 'Original content for UNDO test'"
    echo ""
    echo "  6. Try UNDO again (should fail - no backup):"
    echo "     UNDO undo_test.txt"
    echo "     Should show: 'Error: No backup found'"
    echo ""
    echo -e "${GREEN}Expected: File restored to original state${NC}"
    echo ""
    read -p "Press Enter when test complete..."
}

# Test EXEC operation
test_exec() {
    echo -e "\n${BLUE}═══════════════════════════════════════${NC}"
    echo -e "${BLUE}        TEST 5: EXEC Operation         ${NC}"
    echo -e "${BLUE}═══════════════════════════════════════${NC}"
    
    echo ""
    echo "This test will:"
    echo "1. Create metadata for test_script.sh"
    echo "2. Execute the script on Name Server"
    echo "3. Capture and display output"
    echo ""
    echo "Script content:"
    cat "$STORAGE_DIR/test_script.sh"
    echo ""
    echo -e "${YELLOW}Instructions:${NC}"
    echo "  1. From $TEST_USER client: CREATE test_script.sh"
    echo "  2. Run: EXEC test_script.sh"
    echo "  3. Should display script output:"
    echo "     - 'Hello from executed script!'"
    echo "     - Current date and user"
    echo "     - 'Script completed successfully'"
    echo ""
    echo "  4. Test permissions:"
    echo "     From $TEST_USER2: EXEC test_script.sh"
    echo "     Should fail: 'Permission denied'"
    echo ""
    echo "  5. Grant access and retry:"
    echo "     From alice: ADDACCESS -R test_script.sh $TEST_USER2"
    echo "     From bob: EXEC test_script.sh"
    echo "     Should succeed and show output"
    echo ""
    echo -e "${GREEN}Expected: Script executes on NM, output captured${NC}"
    echo ""
    read -p "Press Enter when test complete..."
}

# Test concurrent operations
test_concurrent() {
    echo -e "\n${BLUE}═══════════════════════════════════════${NC}"
    echo -e "${BLUE}    TEST 6: Concurrent Operations      ${NC}"
    echo -e "${BLUE}═══════════════════════════════════════${NC}"
    
    echo ""
    echo "This test will:"
    echo "1. Multiple clients READ simultaneously"
    echo "2. Test lock contention with WRITE"
    echo ""
    echo -e "${YELLOW}Instructions:${NC}"
    echo "  1. Open 3 client terminals:"
    echo "     Terminal 1: ./bin/client alice 127.0.0.1 $NM_PORT"
    echo "     Terminal 2: ./bin/client bob 127.0.0.1 $NM_PORT"
    echo "     Terminal 3: ./bin/client charlie 127.0.0.1 $NM_PORT"
    echo ""
    echo "  2. From all 3, run simultaneously:"
    echo "     READ read_test.txt"
    echo "     (all should succeed)"
    echo ""
    echo "  3. From Terminal 1 (alice):"
    echo "     WRITE write_test.txt 0"
    echo "     (don't type ETIRW - keep lock held)"
    echo ""
    echo "  4. From Terminal 2 (bob):"
    echo "     WRITE write_test.txt 0"
    echo "     Should show: 'Sentence 0 is locked by alice'"
    echo ""
    echo "  5. From Terminal 1: WRITE> ETIRW"
    echo ""
    echo "  6. From Terminal 2 (bob): WRITE write_test.txt 0"
    echo "     Should now acquire lock successfully"
    echo ""
    echo -e "${GREEN}Expected: Concurrent READs work, locks prevent concurrent WRITEs${NC}"
    echo ""
    read -p "Press Enter when test complete..."
}

# Main menu
main_menu() {
    while true; do
        echo -e "\n${BLUE}═══════════════════════════════════════${NC}"
        echo -e "${BLUE}         Phase 5 Test Menu            ${NC}"
        echo -e "${BLUE}═══════════════════════════════════════${NC}"
        echo ""
        echo "1. Setup test environment"
        echo "2. Test READ operation"
        echo "3. Test STREAM operation"
        echo "4. Test WRITE operation"
        echo "5. Test UNDO operation"
        echo "6. Test EXEC operation"
        echo "7. Test concurrent operations"
        echo "8. Run all tests"
        echo "9. Clean up"
        echo "0. Exit"
        echo ""
        read -p "Select test (0-9): " choice
        
        case $choice in
            1) setup_test_env ;;
            2) test_read ;;
            3) test_stream ;;
            4) test_write ;;
            5) test_undo ;;
            6) test_exec ;;
            7) test_concurrent ;;
            8)
                setup_test_env
                test_read
                test_stream
                test_write
                test_undo
                test_exec
                test_concurrent
                echo -e "\n${GREEN}✓ All tests complete!${NC}"
                ;;
            9)
                echo -e "\n${YELLOW}Cleaning up...${NC}"
                rm -rf "$STORAGE_DIR"
                echo -e "${GREEN}✓ Test files removed${NC}"
                ;;
            0)
                echo -e "\n${GREEN}Testing complete!${NC}"
                exit 0
                ;;
            *)
                echo -e "${RED}Invalid choice${NC}"
                ;;
        esac
    done
}

# Check if servers are running
if ! check_servers; then
    echo ""
    echo -e "${YELLOW}Please start the servers first:${NC}"
    echo ""
    echo "  Terminal 1: ./bin/name_server $NM_PORT"
    echo "  Terminal 2: ./bin/storage_server 127.0.0.1 $NM_PORT $STORAGE_DIR $SS_PORT"
    echo ""
    exit 1
fi

# Run main menu
main_menu
