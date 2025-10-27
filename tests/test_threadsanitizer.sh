#!/bin/bash
set -e

echo "======================================"
echo "  ThreadSanitizer Data Race Check"
echo "======================================"
echo ""

# Clean previous builds
echo "Cleaning previous builds..."
make clean > /dev/null 2>&1

# Compile with TSAN
echo "Compiling with ThreadSanitizer..."
gcc -fsanitize=thread -g -O1 -pthread \
    main.c client_threadpool.c commands.c worker.c \
    queue.c metadata.c file_io.c \
    -o server_tsan

echo "✅ Build successful"
echo ""

# Run server with TSAN
echo "Starting server with TSAN..."
export TSAN_OPTIONS="log_path=tsan-report.txt"
./server_tsan &

SERVER_PID=$!
echo "Server PID: $SERVER_PID"

# Wait for startup
sleep 2

# Run test client
echo ""
echo "Running test client..."
./client

sleep 2

# Shutdown
echo ""
echo "Shutting down server..."
kill -SIGINT $SERVER_PID
wait $SERVER_PID 2>/dev/null || true

# Check results
echo ""
echo "======================================"
echo "  ThreadSanitizer Results"
echo "======================================"

if ls tsan-report.txt* 1> /dev/null 2>&1; then
    echo "⚠️  TSAN reports found:"
    for report in tsan-report.txt*; do
        echo ""
        echo "--- $report ---"
        cat "$report"
    done
    echo ""
    echo "❌ FAILURE: Data races detected!"
    exit 1
else
    echo "✅ SUCCESS: No data races detected!"
    exit 0
fi