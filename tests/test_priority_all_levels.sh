#!/bin/bash
echo "=== Priority System - All 3 Levels Test ==="

killall server 2>/dev/null || true
lsof -ti:8080 | xargs kill -9 2>/dev/null || true
sleep 1

./server > priority_all.log 2>&1 &
SERVER_PID=$!
sleep 2

echo "Creating users..."

# Create users (like the working script)
(echo -e "signup bob pass123\nLIST") | nc -w 1 localhost 8080 > /dev/null 2>&1 &
sleep 0.3
(echo -e "signup vip_alice pass123\nLIST") | nc -w 1 localhost 8080 > /dev/null 2>&1 &
sleep 0.3
(echo -e "signup admin_root pass123\nLIST") | nc -w 1 localhost 8080 > /dev/null 2>&1 &
sleep 1

echo "✅ Users: bob(priority=1), vip_alice(priority=2), admin_root(priority=3)"
echo ""
echo "Sending tasks in REVERSE order (normal→VIP→admin)..."
echo ""

# NORMAL (priority 1) - sent FIRST
for i in {1..3}; do
    (echo -e "login bob pass123\nLIST") | nc -w 1 localhost 8080 > /dev/null 2>&1 &
done
echo "✓ Sent 3 NORMAL tasks"
sleep 0.1

# VIP (priority 2) - sent SECOND  
for i in {1..3}; do
    (echo -e "login vip_alice pass123\nLIST") | nc -w 1 localhost 8080 > /dev/null 2>&1 &
done
echo "✓ Sent 3 VIP tasks"
sleep 0.1

# ADMIN (priority 3) - sent LAST
for i in {1..3}; do
    (echo -e "login admin_root pass123\nLIST") | nc -w 1 localhost 8080 > /dev/null 2>&1 &
done
echo "✓ Sent 3 ADMIN tasks"

sleep 3

echo ""
echo "======================================"
echo " RESULTS"
echo "======================================"

ADMIN=$(grep "admin_root.*priority=3" priority_all.log | wc -l)
VIP=$(grep "vip_alice.*priority=2" priority_all.log | wc -l)
NORMAL=$(grep "bob.*priority=1" priority_all.log | wc -l)

echo ""
echo "Tasks Processed:"
echo "  ADMIN (priority=3):  $ADMIN"
echo "  VIP (priority=2):    $VIP"
echo "  NORMAL (priority=1): $NORMAL"
echo ""

echo "Processing Order:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
grep "Processing.*priority=" priority_all.log

echo ""
echo "Analysis:"
FIRST=$(grep "Processing.*priority=" priority_all.log | head -1)
if echo "$FIRST" | grep -q "admin_root.*priority=3"; then
    echo "✅ SUCCESS! ADMIN (priority=3) processed first!"
    echo "   Priority queue working perfectly! 🎉"
elif echo "$FIRST" | grep -q "vip_alice.*priority=2"; then
    echo "⚠️  VIP processed first (admin should be first)"
elif echo "$FIRST" | grep -q "bob.*priority=1"; then
    echo "❌ Normal processed first (should be admin)"
else
    echo "⚠️  No tasks processed - check priority_all.log"
fi

kill -SIGINT $SERVER_PID 2>/dev/null
sleep 1

echo ""
echo "Full log: priority_all.log"
