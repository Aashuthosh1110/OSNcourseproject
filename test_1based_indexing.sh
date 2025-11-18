#!/bin/bash

# Test script to demonstrate 1-based indexing

echo "=== Testing 1-Based Indexing System ==="
echo ""

# Create test file with known content
rm -f storage/test1based.txt*
echo "Hello world. This is a test." > storage/test1based.txt

echo "Original file content:"
cat storage/test1based.txt
echo ""

echo "=== 1-Based Indexing Examples ==="
echo ""
echo "File content breakdown:"
echo "Sentence 1: 'Hello world.'"
echo "  Word 1: 'Hello'"
echo "  Word 2: 'world.'"
echo ""
echo "Sentence 2: 'This is a test.'"
echo "  Word 1: 'This'"
echo "  Word 2: 'is'"
echo "  Word 3: 'a'"
echo "  Word 4: 'test.'"
echo ""

echo "To replace the first word of the first sentence with 'Goodbye':"
echo "  Command: WRITE test1based.txt 1"
echo "  Word Update: 1 Goodbye"
echo ""

echo "To replace the second word of the second sentence with 'was':"
echo "  Command: WRITE test1based.txt 2" 
echo "  Word Update: 2 was"
echo ""

echo "Note: All indexing is now 1-based for user input!"
echo "  - Sentence 1 = first sentence"
echo "  - Word 1 = first word in that sentence"