#!/bin/bash
set -e

echo "======================================"
echo " ThreadSanitizer Data Race Check"
echo "======================================"
echo ""

# Go to project root
cd "$(dirname "$0")/.."
echo "Working dir: $PWD"

# Fix permissions
echo "Fixing source permissions..."
chmod +r src/*.c src/*.h 2>/dev/null || true

# Clean
echo "Cleaning previous builds..."
make -f make_merged clean > /dev/null 2>&1 || true
rm -f tsan-report.txt* *.log 2>/dev/null || true

# Create dummies
echo "Creating dummy test files..."
echo "test content" > test1.txt
echo "race content" > test_same.txt

# Compile with TSAN
echo "Compiling with ThreadSanitizer..."
if make -f make_merged CFLAGS="-fsanitize=thread -g -O1 -pthread -I src" \
   LDFLAGS="-fsanitize=thread -pthread -lm -ldl" all > build.log 2>&1; then
    echo "✅ Build successful (server + clients)"
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

# Run server with output redirection
echo "Starting server with TSAN..."
export TSAN_OPTIONS="log_path=tsan-report.txt verbosity=1 halt_on_error=0 history_size=7 second_deadlock_stack=1"
./server > server.log 2>&1 &
SERVER_PID=$!
echo "Server PID: $SERVER_PID"

# Wait for server to be ready
echo "Waiting for server startup..."
MAX_WAIT=10
COUNTER=0
while [ $COUNTER -lt $MAX_WAIT ]; do
    if grep -q "Server Ready" server.log 2>/dev/null; then
        echo "✅ Server started successfully"
        break
    fi
    
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo "❌ Server crashed during startup!"
        echo "Server log:"
        cat server.log
        exit 1
    fi
    
    sleep 1
    COUNTER=$((COUNTER + 1))
done

if [ $COUNTER -eq $MAX_WAIT ]; then
    echo "❌ Server startup timeout—log:"
    cat server.log
    kill $SERVER_PID 2>/dev/null || true
    exit 1
fi

# Verify port is listening
if ! netstat -tuln | grep -q ":8080 "; then
    echo "❌ Server not listening on port 8080!"
    echo "Server log:"
    cat server.log
    kill $SERVER_PID 2>/dev/null || true
    exit 1
fi

# Run clients
echo ""
echo "Running test clients..."
echo "→ Running basic client..."
timeout 10 ./client > client.log 2>&1 || echo "  (client finished)"

sleep 1

echo "→ Running file upload tests..."
timeout 15 ./client_file_testing test1.txt > client_file1.log 2>&1 &
CLIENT1=$!

sleep 2

echo "→ Running concurrent file test (race condition)..."
timeout 15 ./client_file_testing test_same.txt > client_file2.log 2>&1 &
CLIENT2=$!

sleep 1

echo "→ Running multi-client test..."
timeout 15 ./client_test > client_multi.log 2>&1 &
CLIENT3=$!

# Wait for all clients
echo "Waiting for clients to complete..."
wait $CLIENT1 2>/dev/null || true
wait $CLIENT2 2>/dev/null || true
wait $CLIENT3 2>/dev/null || true

sleep 2

# Shutdown gracefully
echo ""
echo "Shutting down server..."
kill -SIGINT $SERVER_PID 2>/dev/null || true
sleep 3

# Force kill if still running
if kill -0 $SERVER_PID 2>/dev/null; then
    echo "Force killing server..."
    kill -9 $SERVER_PID 2>/dev/null || true
fi

wait $SERVER_PID 2>/dev/null || true

# Check for TSAN reports
echo ""
echo "======================================"
echo " ThreadSanitizer Results"
echo "======================================"

# List all TSAN report files
TSAN_REPORTS=$(ls tsan-report.txt* 2>/dev/null || true)

if [ -z "$TSAN_REPORTS" ]; then
    echo "ℹ️  No TSAN report files found"
    
    # Check server log for inline warnings
    if grep -q "WARNING: ThreadSanitizer" server.log 2>/dev/null; then
        echo ""
        echo "⚠️ TSAN warnings found in server log:"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        grep -B 5 -A 30 "WARNING: ThreadSanitizer" server.log
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo ""
        echo "❌ FAILURE: Data races detected!"
        exit 1
    fi
    
    echo "✅ SUCCESS: No data races detected!"
    echo ""
    echo "Cleanup..."
    rm -f test*.txt build.log *.log
    exit 0
fi

# Process TSAN report files
echo "⚠️ TSAN reports found:"
echo ""

RACE_FOUND=0
DEADLOCK_FOUND=0

for report in $TSAN_REPORTS; do
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "📄 Report: $report"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    
    if [ -f "$report" ] && [ -s "$report" ]; then
        # Check for data races
        if grep -q "WARNING: ThreadSanitizer: data race" "$report"; then
            echo "🔴 DATA RACE DETECTED:"
            echo ""
            cat "$report"
            echo ""
            RACE_FOUND=1
        # Check for deadlocks
        elif grep -q "WARNING: ThreadSanitizer: lock-order-inversion" "$report"; then
            echo "🔴 POTENTIAL DEADLOCK DETECTED:"
            echo ""
            cat "$report"
            echo ""
            DEADLOCK_FOUND=1
        # Check for other warnings
        elif grep -q "WARNING: ThreadSanitizer" "$report"; then
            echo "⚠️  OTHER TSAN WARNING:"
            echo ""
            cat "$report"
            echo ""
        else
            echo "ℹ️  Report contains no critical warnings"
            # Optionally show first 20 lines
            head -20 "$report"
        fi
    else
        echo "⚠️  Report file is empty or unreadable"
    fi
    echo ""
done

# Final verdict
if [ $RACE_FOUND -eq 1 ] || [ $DEADLOCK_FOUND -eq 1 ]; then
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "❌ FAILURE: Thread safety issues detected!"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    echo "Common causes:"
    echo "  1. Missing mutex locks on shared data structures"
    echo "  2. Accessing task_queue, task_count, or metadata without locks"
    echo "  3. Race conditions in file operations"
    echo "  4. Incorrect lock ordering (deadlock risk)"
    echo ""
    echo "To fix:"
    echo "  - Add pthread_mutex_lock/unlock around ALL shared data access"
    echo "  - Ensure consistent lock ordering across all threads"
    echo "  - Use condition variables for thread synchronization"
    echo ""
    exit 1
fi

echo "✅ SUCCESS: No critical thread safety issues detected!"
echo ""
echo "Cleanup..."
rm -f tsan-report.txt* test*.txt build.log *.log

exit 0