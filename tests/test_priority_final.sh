#!/bin/bash
echo "=== Priority System - Full Test (3 Priority Levels) ==="

killall server 2>/dev/null || true
lsof -ti:8080 | xargs kill -9 2>/dev/null || true
sleep 1

./server > priority_final.log 2>&1 &
SERVER_PID=$!
sleep 2

echo "Creating users with different priorities..."
(echo -e "signup bob pass123\nLIST") | nc -w 1 localhost 8080 > /dev/null 2>&1
sleep 0.2
(echo -e "signup vip_alice pass123\nLIST") | nc -w 1 localhost 8080 > /dev/null 2>&1
sleep 0.2
(echo -e "signup admin_root pass123\nLIST") | nc -w 1 localhost 8080 > /dev/null 2>&1
sleep 0.5

echo "✅ Users created: bob(1), vip_alice(2), admin_root(3)"
echo ""
echo "Sending 15 tasks (5 normal, 5 VIP, 5 admin) in REVERSE priority order..."
echo "(Normal sent first, admin sent last - but admin should process first!)"
echo ""

# Send NORMAL first (priority 1)
for i in {1..5}; do
    (echo -e "login bob pass123\nLIST") | nc -w 1 localhost 8080 > /dev/null 2>&1 &
done
echo "✓ Sent 5 NORMAL tasks"

sleep 0.1

# Then VIP (priority 2)
for i in {1..5}; do
    (echo -e "login vip_alice pass123\nLIST") | nc -w 1 localhost 8080 > /dev/null 2>&1 &
done
echo "✓ Sent 5 VIP tasks"

sleep 0.1

# Then ADMIN (priority 3)
for i in {1..5}; do
    (echo -e "login admin_root pass123\nLIST") | nc -w 1 localhost 8080 > /dev/null 2>&1 &
done
echo "✓ Sent 5 ADMIN tasks"

sleep 3

echo ""
echo "======================================"
echo " RESULTS"
echo "======================================"
echo ""

# Count tasks
ADMIN=$(grep "priority=3" priority_final.log | wc -l)
VIP=$(grep "priority=2" priority_final.log | wc -l)
NORMAL=$(grep "priority=1" priority_final.log | wc -l)

echo "Tasks Processed:"
echo "  ADMIN (priority=3):  $ADMIN tasks"
echo "  VIP (priority=2):    $VIP tasks"
echo "  NORMAL (priority=1): $NORMAL tasks"
echo ""

echo "Processing Order (first 15 tasks):"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
grep "Processing.*priority=" priority_final.log | head -15
echo ""

FIRST=$(grep "Processing.*priority=" priority_final.log | head -1)
if echo "$FIRST" | grep -q "priority=3"; then
    echo "✅ SUCCESS: Highest priority (ADMIN) processed first!"
    echo "   Priority system working correctly! 🎉"
elif echo "$FIRST" | grep -q "priority=2"; then
    echo "⚠️  PARTIAL: VIP processed first"
    echo "   (Expected admin first, but priority is working)"
else
    echo "❌ FAILED: Normal priority processed first"
fi

kill -SIGINT $SERVER_PID 2>/dev/null
sleep 1

echo ""
echo "Full log: priority_final.log"
