#!/bin/bash

echo "VIEW Command Fixes Demonstration"
echo "==============================="
echo ""

echo "Issues Fixed:"
echo "1. ✅ Removed duplicate/empty entries in 'view -a' and 'view -l'"
echo "2. ✅ Added scanning of existing files in storage on Name Server startup"  
echo "3. ✅ Fixed timestamp formatting (no more 1970 dates)"
echo "4. ✅ Improved validation to skip invalid entries"
echo "5. ✅ Fixed empty filename and owner handling"
echo ""

echo "Current files in storage:"
ls -la storage/
echo ""

echo "Testing Instructions:"
echo "1. Start Name Server: ./bin/name_server 8080"
echo "   - Should now scan storage/ directory on startup"
echo "   - Will find and register: file.txt, pile.txt, view_test.txt"
echo ""
echo "2. Start Storage Server: ./bin/storage_server 127.0.0.1 8080 ./storage 9080"
echo ""  
echo "3. Connect as user: ./bin/client 127.0.0.1 8080"
echo ""
echo "4. Test VIEW commands:"
echo "   VIEW        → Should show files directly, no header"
echo "   VIEW -a     → Should show all files, no empty rows"
echo "   VIEW -l     → Should show clean table format"
echo "   VIEW -a -l  → Should show all files in table, no duplicates"
echo ""

echo "Expected Results:"
echo "- 'view_test.txt' should now appear (wasn't showing before)"
echo "- No empty rows or duplicate entries"
echo "- Proper timestamps instead of 1970-01-01 dates"
echo "- Clean formatting without extra blank lines"