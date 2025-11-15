# Makefile for Docs++ Distributed Document System
# Builds Name Server, Storage Server, and Client components

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_GNU_SOURCE -g
INCLUDES = -Iinclude
LIBS = -pthread -lreadline

# Directories
SRCDIR = src
INCDIR = include
BINDIR = bin
LOGDIR = logs
STORAGEDIR = storage

# Common source files
COMMON_SRCS = $(SRCDIR)/common/common.c $(SRCDIR)/common/errors.c $(SRCDIR)/common/logging.c $(SRCDIR)/common/protocol.c

# Target executables
TARGETS = $(BINDIR)/name_server $(BINDIR)/storage_server $(BINDIR)/client

# Test executables
TEST_TARGETS = $(BINDIR)/test_protocol

# Default target
all: directories $(TARGETS)

# Create necessary directories
directories:
	@mkdir -p $(BINDIR)
	@mkdir -p $(LOGDIR)
	@mkdir -p $(STORAGEDIR)

# Name Server
$(BINDIR)/name_server: $(SRCDIR)/name_server/name_server.c $(SRCDIR)/name_server/nm_state.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

# Storage Server  
$(BINDIR)/storage_server: $(SRCDIR)/storage_server/storage_server.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

# Client
$(BINDIR)/client: $(SRCDIR)/client/client.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

# Test Protocol
$(BINDIR)/test_protocol: tests/test_protocol.c $(SRCDIR)/common/protocol.c $(SRCDIR)/common/common.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

# Individual component targets
name_server: directories $(BINDIR)/name_server

storage_server: directories $(BINDIR)/storage_server

client: directories $(BINDIR)/client

# Install dependencies (Ubuntu/Debian)
deps:
	sudo apt-get update
	sudo apt-get install -y build-essential libreadline-dev

# Clean build artifacts and temporary files
clean:
	rm -rf $(BINDIR)
	rm -f $(LOGDIR)/*.log
	rm -rf ss_storage/
	rm -rf test_storage/
	rm -rf .tmp/
	rm -f *.tmp
	rm -f core
	rm -f a.out
	find . -name "*.o" -type f -delete
	find . -name "*.gch" -type f -delete
	find . -name "*~" -type f -delete
	find . -name "*.swp" -type f -delete
	find . -name ".DS_Store" -type f -delete

# Clean everything including all storage data
clean-all: clean
	rm -rf $(STORAGEDIR)/*

# Clean only storage data (preserve directory structure)
clean-storage:
	rm -rf $(STORAGEDIR)/*/

# Run components (for testing)
run-name-server: $(BINDIR)/name_server
	./$(BINDIR)/name_server 8080

run-storage-server: $(BINDIR)/storage_server
	./$(BINDIR)/storage_server localhost 8080 $(STORAGEDIR)/ss_storage

run-client: $(BINDIR)/client
	./$(BINDIR)/client localhost 8080

# Development targets
debug: CFLAGS += -DDEBUG -O0
debug: all

release: CFLAGS += -DNDEBUG -O2
release: all

# Format code (requires clang-format)
format:
	find $(SRCDIR) $(INCDIR) -name "*.c" -o -name "*.h" | xargs clang-format -i

# Static analysis (requires cppcheck)
analyze:
	cppcheck --enable=all --inconclusive --std=c99 $(SRCDIR)

# Test targets
test-protocol: $(BINDIR)/test_protocol
	@echo "Running protocol tests..."
	./$(BINDIR)/test_protocol

test: all test-protocol
	@echo "Running basic functionality tests..."
	@bash scripts/test_basic.sh

test-concurrent: all
	@echo "Running concurrency tests..."  
	@bash scripts/test_concurrent.sh

# Docker targets (optional)
docker-build:
	docker build -t docs-plus-plus .

docker-run: docker-build
	docker run -it --rm -p 8080:8080 docs-plus-plus

# Help target
help:
	@echo "Docs++ Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all               - Build all components"
	@echo "  name_server       - Build name server only"
	@echo "  storage_server    - Build storage server only" 
	@echo "  client            - Build client only"
	@echo "  deps              - Install development dependencies"
	@echo "  clean             - Clean build artifacts and temp files"
	@echo "  clean-all         - Clean everything including storage"
	@echo "  clean-storage     - Clean only storage data (preserve structure)"
	@echo "  debug             - Build with debug flags"
	@echo "  release           - Build optimized release"
	@echo "  format            - Format source code"
	@echo "  analyze           - Run static analysis"
	@echo "  test-protocol     - Run protocol unit tests"
	@echo "  test              - Run basic tests"
	@echo "  test-concurrent   - Run concurrency tests"
	@echo "  help              - Show this help"
	@echo ""
	@echo "Run targets:"
	@echo "  run-name-server   - Start name server on port 8080"
	@echo "  run-storage-server - Start storage server"
	@echo "  run-client        - Start client"

.PHONY: all directories clean clean-all clean-storage debug release format analyze test test-protocol test-concurrent help deps docker-build docker-run name_server storage_server client run-name-server run-storage-server run-client