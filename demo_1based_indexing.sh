#!/bin/bash

# Comprehensive 1-Based Indexing Demo Script

echo "=== 1-Based Indexing Implementation Example ==="
echo ""

# Create directories
mkdir -p storage logs

# Create test content
echo "Hello world. This is a test." > storage/demo.txt

echo "📝 Original file content:"
cat storage/demo.txt
echo ""

echo "📊 File Structure Analysis (1-Based Indexing):"
echo "   Sentence 1: 'Hello world.'"
echo "     Word 1: 'Hello'"
echo "     Word 2: 'world.'" 
echo ""
echo "   Sentence 2: 'This is a test.'"
echo "     Word 1: 'This'"
echo "     Word 2: 'is'"
echo "     Word 3: 'a'"
echo "     Word 4: 'test.'"
echo ""

echo "🔧 Implementation Features:"
echo ""
echo "✅ User Interface: 1-Based Indexing"
echo "   - Users input sentence numbers starting from 1"
echo "   - Users input word indices starting from 1"
echo "   - All error messages show 1-based indices"
echo ""

echo "✅ Internal Processing: 0-Based Indexing"  
echo "   - Client converts user input: sentence_num -= 1"
echo "   - Client converts user input: word_index -= 1"
echo "   - Storage server uses 0-based arrays internally"
echo ""

echo "✅ Consistent User Experience:"
echo "   - Usage: 'WRITE filename sentence_num (1-based indexing)'"
echo "   - Prompt: 'Enter word updates: <word_index> <content> (1-based)'"
echo "   - Validation: 'Word index must be >= 1'"
echo "   - Display: 'Lock acquired for sentence 1'"
echo ""

echo "📋 Example Commands:"
echo ""
echo "1️⃣  Edit first sentence, first word:"
echo "   Command: WRITE demo.txt 1"
echo "   Input:   1 Goodbye"
echo "   Result:  'Goodbye world. This is a test.'"
echo ""

echo "2️⃣  Edit second sentence, second word:" 
echo "   Command: WRITE demo.txt 2"
echo "   Input:   2 was"  
echo "   Result:  'Goodbye world. This was a test.'"
echo ""

echo "3️⃣  Add new content:"
echo "   Command: WRITE demo.txt 1"
echo "   Input:   3 amazing"
echo "   Result:  'Goodbye world amazing. This was a test.'"
echo ""

echo "❌ Error Examples:"
echo "   WRITE demo.txt 0  → Error: Sentence number must be >= 1"
echo "   Word input: 0 content  → Error: Word index must be >= 1"
echo "   WRITE demo.txt 99 → Error: Invalid sentence index"
echo ""

echo "🔄 Index Conversion Logic:"
echo "   Client Side:"
echo "   - User enters: sentence 1, word 1"
echo "   - Client sends: sentence 0, word 0 (internally)"
echo ""
echo "   Storage Server:"
echo "   - Receives: sentence 0, word 0"  
echo "   - Processes: array[0], word[0]"
echo "   - Displays: 'Word 1 updated' (to user)"
echo ""

echo "This implementation provides intuitive 1-based user experience"
echo "while maintaining efficient 0-based internal processing! ✨"