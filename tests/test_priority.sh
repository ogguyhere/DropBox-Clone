
#!/bin/bash
echo "=== Priority System - All 3 Levels Test ==="

killall server 2>/dev/null || true
lsof -ti:8080 | xargs kill -9 2>/dev/null || true
sleep 1

./server > priority_all.log 2>&1 &
SERVER_PID=$!
echo "Server PID: $SERVER_PID"

echo "Waiting for server boot..."
sleep 5
if ! nc -z localhost 8080 2>/dev/null; then
    echo "❌ Server not listening—log:"
    cat priority_all.log
    kill $SERVER_PID 2>/dev/null || true
    exit 1
fi
echo "✅ Server ready on 8080"

echo "Creating users..."

(echo -e "signup bob pass123\n") | nc -w 3 localhost 8080 > user_bob.log 2>&1 &
sleep 0.5
(echo -e "signup vip_alice pass123\n") | nc -w 3 localhost 8080 > user_vip.log 2>&1 &
sleep 0.5
(echo -e "signup admin_root pass123\n") | nc -w 3 localhost 8080 > user_admin.log 2>&1 &
sleep 2

echo "✅ Users: bob(priority=1), vip_alice(priority=2), admin_root(priority=3)"
echo "User logs:"
cat user_bob.log user_vip.log user_admin.log
echo ""

echo "Sending tasks in REVERSE order (normal→VIP→admin)..."
echo ""

# NORMAL (priority 1) - sent FIRST (low pri stays front)
for i in {1..3}; do
    sleep 0.2  # Stagger
    (echo -e "login bob pass123\nLIST\n") | nc -w 5 localhost 8080 > normal_$i.log 2>&1 &
done
echo "✓ Sent 3 NORMAL tasks"
sleep 1  # Wait for enqueue, but no dequeue yet

# VIP (priority 2) - sent SECOND (higher pri, insert front)
for i in {1..3}; do
    sleep 0.2
    (echo -e "login vip_alice pass123\nLIST\n") | nc -w 5 localhost 8080 > vip_$i.log 2>&1 &
done
echo "✓ Sent 3 VIP tasks"
sleep 1

# ADMIN (priority 3) - sent LAST (highest pri, jump to front)
for i in {1..3}; do
    sleep 0.2
    (echo -e "login admin_root pass123\nLIST\n") | nc -w 5 localhost 8080 > admin_$i.log 2>&1 &
done
echo "✓ Sent 3 ADMIN tasks"

echo "Waiting for task processing..."
sleep 20  # Allow dequeues after all enqueued

echo ""
echo "======================================"
echo " RESULTS"
echo "======================================"

echo "Server log snippet (processing lines):"
grep "Processing\|Enqueuing\|LOGIN\|LIST\|priority" priority_all.log || echo "No processing logs—check enqueue in commands.c."

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
grep "Processing.*priority=" priority_all.log || echo "No priority logs—add to worker.c."

echo ""
echo "Analysis:"
FIRST=$(grep "Processing.*priority=" priority_all.log | head -1)
if echo "$FIRST" | grep -q "admin_root.*priority=3"; then
    echo "✅ SUCCESS! ADMIN (priority=3) processed first!"
elif echo "$FIRST" | grep -q "vip_alice.*priority=2"; then
    echo "⚠️ VIP processed first (admin should be first)"
elif echo "$FIRST" | grep -q "bob.*priority=1"; then
    echo "❌ Normal processed first (should be admin)"
else
    echo "⚠️ No tasks processed - full log below"
fi

kill -SIGINT $SERVER_PID 2>/dev/null
sleep 1

echo ""
echo "Full server log:"
cat priority_all.log
echo ""
echo "Sample client logs:"
ls normal_* vip_* admin_* 2>/dev/null | head -6 | xargs cat || echo "No client logs."
