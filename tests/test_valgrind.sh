#!/bin/bash
set -e

echo "======================================"
echo "  Valgrind Memory Leak Check"
echo "======================================"
echo ""

# Clean build
echo "Building server..."
make clean > /dev/null 2>&1
make > /dev/null 2>&1

# Start server with Valgrind
echo "Starting server with Valgrind..."
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file=valgrind-output.txt \
         ./server &

SERVER_PID=$!
echo "Server PID: $SERVER_PID"

# Wait for server to start
sleep 2

# Run test client
echo ""
echo "Running test client..."
./client > /dev/null 2>&1

sleep 1

# Kill server gracefully
echo ""
echo "Shutting down server..."
kill -SIGINT $SERVER_PID
wait $SERVER_PID 2>/dev/null

# Display results
echo ""
echo "======================================"
echo "  Valgrind Results"
echo "======================================"
grep -A 20 "HEAP SUMMARY" valgrind-output.txt || echo "No summary found"
echo ""
grep -A 10 "LEAK SUMMARY" valgrind-output.txt || echo "No leaks found"
echo ""

# Check for leaks
if grep -q "definitely lost: 0 bytes" valgrind-output.txt && \
   grep -q "indirectly lost: 0 bytes" valgrind-output.txt; then
    echo "✅ SUCCESS: No memory leaks detected!"
    exit 0
else
    echo "❌ FAILURE: Memory leaks detected!"
    echo ""
    echo "Full report saved to: valgrind-output.txt"
    exit 1
fi