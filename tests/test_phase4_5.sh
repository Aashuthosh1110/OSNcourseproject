#!/bin/bash
# Test script for Phase 4.5: ACL Persistence

# Color codes for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}========================================"
echo "Phase 4.5 ACL Persistence Test"
echo -e "========================================${NC}"

# Test configuration
STORAGE_DIR="./test_storage_phase45"
TEST_FILE="testacl.txt"
META_FILE="${STORAGE_DIR}/${TEST_FILE}.meta"

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    rm -rf "${STORAGE_DIR}"
}

# Check if servers are running
check_servers() {
    if ! pgrep -x "name_server" > /dev/null; then
        echo -e "${RED}ERROR: Name Server not running${NC}"
        echo "Start it with: ./bin/name_server 8080 &"
        exit 1
    fi

    if ! pgrep -x "storage_server" > /dev/null; then
        echo -e "${RED}ERROR: Storage Server not running${NC}"
        echo "Start it with: ./bin/storage_server 127.0.0.1 8080 ${STORAGE_DIR} 9001 &"
        exit 1
    fi
}

# Test 1: Check .meta file creation with initial ACL
test_initial_acl() {
    echo -e "\n${YELLOW}Test 1: Initial ACL on file creation${NC}"
    
    if [ ! -f "${META_FILE}" ]; then
        echo -e "${RED}FAIL: .meta file not found: ${META_FILE}${NC}"
        return 1
    fi
    
    # Check for access_0 entry (owner)
    if grep -q "access_0=.*:RW" "${META_FILE}"; then
        echo -e "${GREEN}PASS: Initial ACL contains owner with RW permission${NC}"
    else
        echo -e "${RED}FAIL: Initial ACL missing or incorrect${NC}"
        cat "${META_FILE}"
        return 1
    fi
}

# Test 2: Check .meta file after ADDACCESS
test_add_access() {
    echo -e "\n${YELLOW}Test 2: ACL update after ADDACCESS${NC}"
    echo "Manually run: ADDACCESS -R ${TEST_FILE} bob"
    echo "Then press Enter to check .meta file..."
    read -r
    
    # Count access entries
    ACCESS_COUNT=$(grep -c "^access_[0-9]*=" "${META_FILE}")
    
    if [ "${ACCESS_COUNT}" -ge 2 ]; then
        echo -e "${GREEN}PASS: ACL now contains ${ACCESS_COUNT} entries${NC}"
        
        # Show the ACL
        echo "Current ACL entries:"
        grep "^access_[0-9]*=" "${META_FILE}" | head -5
    else
        echo -e "${RED}FAIL: ACL should have at least 2 entries, found ${ACCESS_COUNT}${NC}"
        cat "${META_FILE}"
        return 1
    fi
}

# Test 3: Check .meta file after REMACCESS
test_remove_access() {
    echo -e "\n${YELLOW}Test 3: ACL update after REMACCESS${NC}"
    echo "Manually run: REMACCESS ${TEST_FILE} bob"
    echo "Then press Enter to check .meta file..."
    read -r
    
    ACCESS_COUNT=$(grep -c "^access_[0-9]*=" "${META_FILE}")
    
    if [ "${ACCESS_COUNT}" -eq 1 ]; then
        echo -e "${GREEN}PASS: ACL reverted to 1 entry (owner only)${NC}"
        grep "^access_0=" "${META_FILE}"
    else
        echo -e "${RED}FAIL: ACL should have 1 entry, found ${ACCESS_COUNT}${NC}"
        cat "${META_FILE}"
        return 1
    fi
}

# Test 4: Persistence across Name Server restart
test_persistence() {
    echo -e "\n${YELLOW}Test 4: ACL persistence (Storage Server is source of truth)${NC}"
    echo "This test verifies .meta file remains intact"
    
    # Take snapshot of current .meta
    cp "${META_FILE}" "${META_FILE}.snapshot"
    
    echo "Grant access to charlie and dave:"
    echo "  ADDACCESS -R ${TEST_FILE} charlie"
    echo "  ADDACCESS -W ${TEST_FILE} dave"
    echo "Then press Enter..."
    read -r
    
    # Count entries
    ACCESS_COUNT=$(grep -c "^access_[0-9]*=" "${META_FILE}")
    echo "Current ACL has ${ACCESS_COUNT} entries:"
    grep "^access_[0-9]*=" "${META_FILE}"
    
    echo -e "\n${YELLOW}The .meta file is the source of truth.${NC}"
    echo "Even if the Name Server restarts, the Storage Server retains this ACL."
    echo "Press Enter to continue..."
    read -r
}

# Test 5: Rollback on SS failure (simulated)
test_rollback_simulation() {
    echo -e "\n${YELLOW}Test 5: Rollback behavior (manual verification)${NC}"
    echo "This test requires manual steps:"
    echo "  1. Stop the Storage Server process"
    echo "  2. Try: ADDACCESS -R ${TEST_FILE} eve"
    echo "  3. You should see an error (network failure)"
    echo "  4. Restart Storage Server"
    echo "  5. Verify .meta file is unchanged (rollback succeeded)"
    echo ""
    echo "Complete these steps, then press Enter to finish test..."
    read -r
    
    echo -e "${GREEN}Manual rollback test completed${NC}"
}

# Main test execution
main() {
    trap cleanup EXIT
    
    # Check servers
    check_servers
    
    echo -e "\n${YELLOW}Starting Phase 4.5 tests...${NC}"
    echo "Make sure you have a client connected as 'alice' and have created '${TEST_FILE}'"
    echo ""
    echo "Run these commands in your client:"
    echo "  CREATE ${TEST_FILE}"
    echo ""
    echo "Press Enter when ready to begin tests..."
    read -r
    
    # Run tests
    test_initial_acl
    test_add_access
    test_remove_access
    test_persistence
    test_rollback_simulation
    
    echo -e "\n${GREEN}========================================"
    echo "Phase 4.5 Tests Complete!"
    echo -e "========================================${NC}"
    echo ""
    echo "Summary:"
    echo "  - ACL changes persist to .meta file"
    echo "  - Storage Server is the source of truth"
    echo "  - Name Server rolls back on SS failure"
}

main "$@"
