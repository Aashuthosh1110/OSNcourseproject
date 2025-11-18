#!/bin/bash

# Phase 4 Testing Script for Docs++
# Tests: LIST, VIEW, INFO, ADDACCESS, REMACCESS

set -e  # Exit on error

echo "========================================="
echo "Phase 4 Testing Script"
echo "========================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Function to print test result
test_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✓ PASS${NC}: $2"
        ((TESTS_PASSED++))
    else
        echo -e "${RED}✗ FAIL${NC}: $2"
        ((TESTS_FAILED++))
    fi
}

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up..."
    pkill -f "bin/name_server" 2>/dev/null || true
    pkill -f "bin/storage_server" 2>/dev/null || true
    pkill -f "bin/client" 2>/dev/null || true
    sleep 1
    echo "Cleanup complete"
}

# Set trap to cleanup on exit
trap cleanup EXIT

echo "Step 1: Starting Name Server..."
./bin/name_server 8080 > logs/test_nm.log 2>&1 &
NM_PID=$!
sleep 1

if ps -p $NM_PID > /dev/null; then
    echo -e "${GREEN}✓${NC} Name Server started (PID: $NM_PID)"
else
    echo -e "${RED}✗${NC} Name Server failed to start"
    exit 1
fi

echo ""
echo "Step 2: Starting Storage Server..."
mkdir -p test_storage/ss1
./bin/storage_server localhost 8080 test_storage/ss1 9001 > logs/test_ss.log 2>&1 &
SS_PID=$!
sleep 2

if ps -p $SS_PID > /dev/null; then
    echo -e "${GREEN}✓${NC} Storage Server started (PID: $SS_PID)"
else
    echo -e "${RED}✗${NC} Storage Server failed to start"
    exit 1
fi

echo ""
echo "========================================="
echo "Phase 4: Interactive Testing"
echo "========================================="
echo ""
echo "The servers are now running. Please open 3 terminal windows and run:"
echo ""
echo "Terminal 1 (Alice - Owner):"
echo "  ./bin/client"
echo "  Username: alice"
echo ""
echo "Terminal 2 (Bob - Collaborator):"
echo "  ./bin/client"
echo "  Username: bob"
echo ""
echo "Terminal 3 (Charlie - Observer):"
echo "  ./bin/client"
echo "  Username: charlie"
echo ""
echo "========================================="
echo "Test Sequence:"
echo "========================================="
echo ""
echo "1. In Alice's terminal:"
echo "   CREATE test1.txt"
echo "   CREATE test2.txt"
echo ""
echo "2. In all terminals, test LIST command:"
echo "   LIST"
echo "   Expected: See alice, bob, charlie connected"
echo ""
echo "3. In all terminals, test VIEW command:"
echo "   VIEW"
echo "   Alice: Should see test1.txt, test2.txt"
echo "   Bob: Should see nothing (no access)"
echo "   Charlie: Should see nothing (no access)"
echo ""
echo "4. Test VIEW with -a flag (all files):"
echo "   VIEW -a"
echo "   All users: Should see test1.txt, test2.txt"
echo ""
echo "5. Test VIEW with -l flag (long format):"
echo "   VIEW -l"
echo "   Should show permissions and owner"
echo ""
echo "6. Test INFO command:"
echo "   In Alice's terminal: INFO test1.txt"
echo "   Should show file metadata with owner=alice, access_list"
echo ""
echo "   In Bob's terminal: INFO test1.txt"
echo "   Should show 'Permission denied'"
echo ""
echo "7. Grant access to Bob:"
echo "   In Alice's terminal: ADDACCESS -R test1.txt bob"
echo "   Expected: 'Access granted successfully'"
echo ""
echo "8. Verify Bob now has access:"
echo "   In Bob's terminal: INFO test1.txt"
echo "   Should now work and show file metadata"
echo ""
echo "   In Bob's terminal: VIEW"
echo "   Should now show test1.txt"
echo ""
echo "9. Grant write access to Bob:"
echo "   In Alice's terminal: ADDACCESS -W test1.txt bob"
echo "   Expected: 'Access granted successfully'"
echo ""
echo "10. Test INFO to see updated permissions:"
echo "    In Alice's terminal: INFO test1.txt"
echo "    Should show bob with RW permissions"
echo ""
echo "11. Test access control enforcement:"
echo "    In Bob's terminal: ADDACCESS -R test1.txt charlie"
echo "    Expected: 'Only the owner can modify access control'"
echo ""
echo "12. Grant access to Charlie:"
echo "    In Alice's terminal: ADDACCESS -R test1.txt charlie"
echo "    Expected: 'Access granted successfully'"
echo ""
echo "13. Verify Charlie has access:"
echo "    In Charlie's terminal: VIEW"
echo "    Should show test1.txt"
echo ""
echo "14. Revoke Bob's access:"
echo "    In Alice's terminal: REMACCESS test1.txt bob"
echo "    Expected: 'Access revoked successfully'"
echo ""
echo "15. Verify Bob lost access:"
echo "    In Bob's terminal: INFO test1.txt"
echo "    Should show 'Permission denied'"
echo ""
echo "16. Test VIEW -al (all files, long format):"
echo "    In all terminals: VIEW -al"
echo "    Should show all files with permissions"
echo ""
echo "17. Test error cases:"
echo "    ADDACCESS -X test1.txt bob  (invalid flag)"
echo "    REMACCESS test1.txt alice   (cannot remove owner)"
echo "    INFO nonexistent.txt        (file not found)"
echo ""
echo "========================================="
echo "Expected Access Matrix (after all steps):"
echo "========================================="
echo ""
echo "File: test1.txt"
echo "  alice: RW (owner)"
echo "  charlie: R"
echo ""
echo "File: test2.txt"
echo "  alice: RW (owner)"
echo ""
echo "========================================="
echo ""
echo "Press Ctrl+C when done testing to stop servers"
echo ""

# Wait for user to finish testing
wait
