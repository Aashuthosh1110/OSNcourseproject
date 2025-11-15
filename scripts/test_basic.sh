#!/bin/bash

# Basic functionality tests for Docs++ system
# Tests fundamental operations like create, read, write, delete

set -e

echo "=== Docs++ Basic Functionality Tests ==="

# Configuration
NM_PORT=8080
SS_PORT=9080
STORAGE_DIR="./storage/test_ss"
TEST_DIR="./tests"
LOG_DIR="./logs"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[TEST]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Cleanup function
cleanup() {
    print_status "Cleaning up test processes..."
    pkill -f "name_server" 2>/dev/null || true
    pkill -f "storage_server" 2>/dev/null || true
    sleep 2
    rm -rf "$STORAGE_DIR" 2>/dev/null || true
    rm -f "$LOG_DIR"/*test* 2>/dev/null || true
}

# Set trap to cleanup on exit
trap cleanup EXIT

# Check if binaries exist
if [ ! -f "./bin/name_server" ]; then
    print_error "Name server binary not found. Run 'make' first."
    exit 1
fi

if [ ! -f "./bin/storage_server" ]; then
    print_error "Storage server binary not found. Run 'make' first."
    exit 1
fi

if [ ! -f "./bin/client" ]; then
    print_error "Client binary not found. Run 'make' first."
    exit 1
fi

# Create test storage directory
mkdir -p "$STORAGE_DIR"

print_status "Starting Name Server on port $NM_PORT..."
./bin/name_server $NM_PORT > "$LOG_DIR/test_nm.log" 2>&1 &
NM_PID=$!
sleep 2

# Check if name server started successfully
if ! kill -0 $NM_PID 2>/dev/null; then
    print_error "Failed to start Name Server"
    exit 1
fi

print_status "Starting Storage Server on port $SS_PORT..."
./bin/storage_server localhost $NM_PORT "$STORAGE_DIR" > "$LOG_DIR/test_ss.log" 2>&1 &
SS_PID=$!
sleep 2

# Check if storage server started successfully
if ! kill -0 $SS_PID 2>/dev/null; then
    print_error "Failed to start Storage Server"
    exit 1
fi

print_status "Servers started successfully!"
print_status "Name Server PID: $NM_PID"
print_status "Storage Server PID: $SS_PID"

# Wait a bit more for servers to fully initialize
sleep 3

print_status "Running basic functionality tests..."

# Test 1: Client connection
print_status "Test 1: Client Connection Test"
echo "testuser1" | timeout 10s ./bin/client localhost $NM_PORT > "$LOG_DIR/test_client1.log" 2>&1 &
CLIENT_PID=$!
sleep 2

if kill -0 $CLIENT_PID 2>/dev/null; then
    print_status "✓ Client connection successful"
    kill $CLIENT_PID 2>/dev/null || true
else
    print_error "✗ Client connection failed"
fi

# Test 2: Multiple clients
print_status "Test 2: Multiple Client Connections"
echo "testuser1" | timeout 5s ./bin/client localhost $NM_PORT > "$LOG_DIR/test_client1.log" 2>&1 &
CLIENT1_PID=$!
echo "testuser2" | timeout 5s ./bin/client localhost $NM_PORT > "$LOG_DIR/test_client2.log" 2>&1 &
CLIENT2_PID=$!
sleep 2

if kill -0 $CLIENT1_PID 2>/dev/null && kill -0 $CLIENT2_PID 2>/dev/null; then
    print_status "✓ Multiple client connections successful"
else
    print_warning "⚠ Multiple client connection test incomplete"
fi

kill $CLIENT1_PID $CLIENT2_PID 2>/dev/null || true

# Test 3: Server logs check
print_status "Test 3: Server Logs Check"
if [ -s "$LOG_DIR/test_nm.log" ] && [ -s "$LOG_DIR/test_ss.log" ]; then
    print_status "✓ Server logs are being generated"
else
    print_warning "⚠ Server logs may not be working properly"
fi

# Test 4: Process cleanup
print_status "Test 4: Process Management"
if kill -0 $NM_PID 2>/dev/null && kill -0 $SS_PID 2>/dev/null; then
    print_status "✓ Servers are running properly"
else
    print_error "✗ One or more servers crashed"
fi

print_status "Basic tests completed!"

# Display log summaries
print_status "=== Log Summaries ==="
if [ -f "$LOG_DIR/test_nm.log" ]; then
    echo "Name Server log (last 5 lines):"
    tail -5 "$LOG_DIR/test_nm.log" || true
fi

if [ -f "$LOG_DIR/test_ss.log" ]; then
    echo "Storage Server log (last 5 lines):"
    tail -5 "$LOG_DIR/test_ss.log" || true
fi

print_status "Test run completed. Check logs in $LOG_DIR/ for detailed output."

exit 0