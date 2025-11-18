# Tests Directory

This directory contains all test files for the Distributed Document System.

## Running Tests

All test scripts have been moved to this `tests/` directory. To run them:

```bash
# From the project root directory:
cd tests/
./test_phase5.sh          # Run Phase 5 tests
./test_phase4.sh          # Run Phase 4 tests  
./test_phase3_simple.sh   # Run Phase 3 tests
./health_check.sh         # Run health check

# Or run from root with:
bash tests/test_phase5.sh
```

## Available Tests

- **test_protocol.c** - Protocol unit tests (compiled to ../bin/test_protocol)
- **test_phase1.sh** - Basic connection and initialization tests
- **test_phase2.sh** - State management tests
- **test_phase3_simple.sh** - CREATE/DELETE operations
- **test_phase4.sh** - User management and access control
- **test_phase4_5.sh** - ACL persistence testing
- **test_phase5.sh** - File I/O operations (READ, WRITE, STREAM, UNDO, EXEC)
- **test_phase6.sh** - Advanced features testing
- **test_read_command.sh** - READ command specific tests
- **health_check.sh** - System health verification
- **quick_test.sh** - Quick functionality check

## Note

Test scripts have been updated to work from this subdirectory with relative paths to `../bin/` for executables.