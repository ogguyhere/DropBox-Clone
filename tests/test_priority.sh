#!/bin/bash
echo "=== Testing Priority System ==="

# Cleanup
echo "Cleaning up..."
killall server 2>/dev/null || true
lsof -ti:8080 | xargs kill -9 2>/dev/null || true
sleep 1

# Start server
echo "Starting server..."
./server > server_priority.log 2>&1 &
SERVER_PID=$!
sleep 2

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "❌ Server failed to start!"
    cat server_priority.log
    exit 1
fi

echo "✅ Server running (PID: $SERVER_PID)"
echo ""

# Simple test function
test_user() {
    local username=$1
    local password=$2
    local filename=$3
    
    (
        echo "signup $username $password"
        sleep 0.2
        echo "LIST"
        sleep 0.2
    ) | nc localhost 8080 > /dev/null 2>&1
}

# Create users
echo "Creating users..."
test_user "vip_alice" "pass123" &
sleep 0.3
test_user "bob" "pass123" &
sleep 0.5

echo "Users created!"
echo ""

# Now create multiple LIST requests to test priority
echo "Sending multiple LIST commands..."
echo "VIP users should be processed first!"
echo ""

for i in {1..3}; do
    (echo -e "login vip_alice pass123\nLIST") | nc -w 1 localhost 8080 > /dev/null 2>&1 &
    echo "  Sent VIP request $i"
done

sleep 0.1

for i in {1..3}; do
    (echo -e "login bob pass123\nLIST") | nc -w 1 localhost 8080 > /dev/null 2>&1 &
    echo "  Sent NORMAL request $i"
done

sleep 2

echo ""
echo "=== Task Processing Order ==="
echo "(VIP tasks should appear first)"
echo ""
grep -E "Worker.*Processing.*vip_alice|Worker.*Processing.*bob" server_priority.log

echo ""
echo "=== Full Worker Activity ==="
grep "Worker.*Processing" server_priority.log | tail -15

echo ""
echo "Shutting down..."
kill -SIGINT $SERVER_PID 2>/dev/null
sleep 1
kill -9 $SERVER_PID 2>/dev/null || true

echo ""
echo "✅ Test complete!"
echo ""
echo "Analysis:"
echo "  - If vip_alice tasks appear first → Priority working! ✅"
echo "  - If mixed order → Priority not working ❌"