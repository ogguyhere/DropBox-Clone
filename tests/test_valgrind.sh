#!/bin/bash
set -e

echo "======================================"
echo " Valgrind Memory Leak Check"
echo "======================================"
echo ""

# Go to project root
cd "$(dirname "$0")/.."
echo "Working dir: $PWD"

# Clean build
echo "Cleaning previous build..."
make clean > /dev/null 2>&1 || true

echo "Building server with debug symbols..."
if make CFLAGS="-g -O0 -pthread -I src" \
   LDFLAGS="-pthread -lm -ldl" all > build.log 2>&1; then
    echo "✅ Build successful"
else
    echo "❌ Build failed—log:"
    cat build.log
    exit 1
fi

# Free port
echo "Freeing port 8080..."
lsof -ti:8080 | xargs kill -9 2>/dev/null || true
fuser -k 8080/tcp 2>/dev/null || true
sleep 1

# Create test files
echo "Creating test files..."
echo "test content" > test1.txt

# Start server with Valgrind
echo ""
echo "Starting server with Valgrind..."
echo "(This may take a moment...)"

valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file=valgrind-output.txt \
         --trace-children=yes \
         --track-fds=yes \
         ./server > server.log 2>&1 &

SERVER_PID=$!
echo "Server PID: $SERVER_PID"

# Wait for server to start
echo "Waiting for server startup..."
MAX_WAIT=15
COUNTER=0
while [ $COUNTER -lt $MAX_WAIT ]; do
    if grep -q "Server Ready" server.log 2>/dev/null || \
       netstat -tuln | grep -q ":8080 " 2>/dev/null; then
        echo "✅ Server started successfully"
        break
    fi
    
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo "❌ Server crashed during startup!"
        echo "Server log:"
        cat server.log 2>/dev/null || echo "No server log"
        echo ""
        echo "Valgrind output:"
        cat valgrind-output.txt 2>/dev/null || echo "No valgrind output yet"
        exit 1
    fi
    
    sleep 1
    COUNTER=$((COUNTER + 1))
done

if [ $COUNTER -eq $MAX_WAIT ]; then
    echo "❌ Server startup timeout!"
    kill $SERVER_PID 2>/dev/null || true
    exit 1
fi

sleep 2

# Run test clients
echo ""
echo "Running test clients..."

echo "→ Basic client test..."
timeout 10 ./client > client.log 2>&1 || true

sleep 1

echo "→ File upload test..."
timeout 10 ./client_file_testing test1.txt > client_file.log 2>&1 || true

sleep 2

# Graceful shutdown
echo ""
echo "Shutting down server gracefully..."
kill -SIGINT $SERVER_PID 2>/dev/null || true

# Wait for clean shutdown
SHUTDOWN_WAIT=10
COUNTER=0
while kill -0 $SERVER_PID 2>/dev/null && [ $COUNTER -lt $SHUTDOWN_WAIT ]; do
    echo "  Waiting for shutdown... ($COUNTER/$SHUTDOWN_WAIT)"
    sleep 1
    COUNTER=$((COUNTER + 1))
done

# Force kill if still running
if kill -0 $SERVER_PID 2>/dev/null; then
    echo "⚠️  Force killing server (didn't shutdown gracefully)"
    kill -9 $SERVER_PID 2>/dev/null || true
fi

wait $SERVER_PID 2>/dev/null || true

# Give Valgrind time to write output
sleep 2

# Display results
echo ""
echo "======================================"
echo " Valgrind Results"
echo "======================================"
echo ""

if [ ! -f valgrind-output.txt ]; then
    echo "❌ ERROR: valgrind-output.txt not found!"
    exit 1
fi

# Show heap summary
if grep -q "HEAP SUMMARY" valgrind-output.txt; then
    echo "📊 HEAP SUMMARY:"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    grep -A 5 "HEAP SUMMARY" valgrind-output.txt | head -6
    echo ""
fi

# Show leak summary
if grep -q "LEAK SUMMARY" valgrind-output.txt; then
    echo "💧 LEAK SUMMARY:"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    grep -A 10 "LEAK SUMMARY" valgrind-output.txt | head -11
    echo ""
fi

# Check for specific leak types
echo "🔍 DETAILED LEAK ANALYSIS:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

DEFINITELY_LOST=$(grep "definitely lost:" valgrind-output.txt | tail -1 | grep -oP '\d+(?= bytes)' || echo "0")
INDIRECTLY_LOST=$(grep "indirectly lost:" valgrind-output.txt | tail -1 | grep -oP '\d+(?= bytes)' || echo "0")
POSSIBLY_LOST=$(grep "possibly lost:" valgrind-output.txt | tail -1 | grep -oP '\d+(?= bytes)' || echo "0")
STILL_REACHABLE=$(grep "still reachable:" valgrind-output.txt | tail -1 | grep -oP '\d+(?= bytes)' || echo "0")

echo "  Definitely lost:   $DEFINITELY_LOST bytes"
echo "  Indirectly lost:   $INDIRECTLY_LOST bytes"
echo "  Possibly lost:     $POSSIBLY_LOST bytes"
echo "  Still reachable:   $STILL_REACHABLE bytes"
echo ""

# Show leak details if any
if [ "$DEFINITELY_LOST" != "0" ] || [ "$INDIRECTLY_LOST" != "0" ]; then
    echo "🔴 LEAK DETAILS:"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    grep -B 3 -A 10 "blocks are definitely lost\|blocks are indirectly lost" valgrind-output.txt | head -50
    echo ""
fi

# Check for file descriptor leaks
if grep -q "Open file descriptor" valgrind-output.txt; then
    echo "📁 FILE DESCRIPTOR LEAKS:"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    grep -A 2 "Open file descriptor" valgrind-output.txt | head -20
    echo ""
fi

# Check for invalid reads/writes
if grep -q "Invalid read\|Invalid write" valgrind-output.txt; then
    echo "⚠️  INVALID MEMORY ACCESS:"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    grep -B 2 -A 5 "Invalid read\|Invalid write" valgrind-output.txt | head -20
    echo ""
fi

# Final verdict
echo "======================================"
if [ "$DEFINITELY_LOST" = "0" ] && [ "$INDIRECTLY_LOST" = "0" ]; then
    echo "✅ SUCCESS: No memory leaks detected!"
    echo ""
    if [ "$STILL_REACHABLE" != "0" ]; then
        echo "ℹ️  Note: $STILL_REACHABLE bytes still reachable"
        echo "   (These are typically global/static allocations)"
    fi
    echo ""
    echo "Cleanup..."
    rm -f valgrind-output.txt test*.txt build.log *.log
    exit 0
else
    echo "❌ FAILURE: Memory leaks detected!"
    echo ""
    echo "Common causes in threaded servers:"
    echo "  1. Not freeing task queue nodes"
    echo "  2. Not freeing thread resources (pthread_join missing)"
    echo "  3. Not closing file descriptors"
    echo "  4. Not destroying mutexes/condition variables"
    echo "  5. Not freeing client metadata structures"
    echo ""
    echo "How to fix:"
    echo "  • Add cleanup handler for SIGINT signal"
    echo "  • Free all malloc'd memory before exit"
    echo "  • pthread_join() all worker threads"
    echo "  • pthread_mutex_destroy() all mutexes"
    echo "  • close() all file descriptors"
    echo ""
    echo "Full report saved to: valgrind-output.txt"
    echo "Run: cat valgrind-output.txt | less"
    exit 1
fi
