#!/bin/bash
# Test --list command-line option

set -e
source "$(dirname "$0")/test_framework.sh"

test_suite_start "List Command"

# Test empty list
test_start "Empty lock list"
LIST_OUTPUT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null)
if echo "$LIST_OUTPUT" | grep -q "DESCRIPTOR"; then
    test_pass "Empty list shows header"
else
    test_fail "Empty list should show header"
fi

# Test list with active lock
test_start "List with active lock"
$WAITLOCK --lock-dir "$LOCK_DIR" list_test >/dev/null 2>&1 &
LIST_PID=$!

sleep 1

if $WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -q "list_test"; then
    test_pass "Active lock appears in list"
else
    test_fail "Active lock should appear in list"
fi

kill $LIST_PID 2>/dev/null || true

# Test -l short form
test_start "List short form (-l)"
$WAITLOCK --lock-dir "$LOCK_DIR" list_short >/dev/null 2>&1 &
LIST_SHORT_PID=$!

sleep 1

if $WAITLOCK --lock-dir "$LOCK_DIR" -l 2>/dev/null | grep -q "list_short"; then
    test_pass "Short form list works"
else
    test_fail "Short form list failed"
fi

kill $LIST_SHORT_PID 2>/dev/null || true

# Test human format (default)
test_start "Human format (default)"
$WAITLOCK --lock-dir "$LOCK_DIR" human_test >/dev/null 2>&1 &
HUMAN_PID=$!

sleep 1

HUMAN_OUTPUT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list --format human 2>/dev/null)
if echo "$HUMAN_OUTPUT" | grep -q "DESCRIPTOR" && \
   echo "$HUMAN_OUTPUT" | grep -q "PID" && \
   echo "$HUMAN_OUTPUT" | grep -q "human_test"; then
    test_pass "Human format shows proper columns"
else
    test_fail "Human format missing required columns"
fi

kill $HUMAN_PID 2>/dev/null || true

# Test CSV format
test_start "CSV format"
$WAITLOCK --lock-dir "$LOCK_DIR" csv_test >/dev/null 2>&1 &
CSV_PID=$!

sleep 1

CSV_OUTPUT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list --format csv 2>/dev/null)
if echo "$CSV_OUTPUT" | grep -q "descriptor,pid" && \
   echo "$CSV_OUTPUT" | grep -q "csv_test"; then
    test_pass "CSV format works correctly"
else
    test_fail "CSV format incorrect"
fi

kill $CSV_PID 2>/dev/null || true

# Test null format
test_start "Null format"
$WAITLOCK --lock-dir "$LOCK_DIR" null_test >/dev/null 2>&1 &
NULL_PID=$!

sleep 1

NULL_OUTPUT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list --format null 2>/dev/null)
if echo "$NULL_OUTPUT" | grep -q "null_test"; then
    test_pass "Null format works"
else
    test_fail "Null format failed"
fi

kill $NULL_PID 2>/dev/null || true

# Test -f short form for format
test_start "Format short form (-f)"
$WAITLOCK --lock-dir "$LOCK_DIR" format_short >/dev/null 2>&1 &
FORMAT_SHORT_PID=$!

sleep 1

if $WAITLOCK --lock-dir "$LOCK_DIR" --list -f csv 2>/dev/null | grep -q "format_short"; then
    test_pass "Short form format flag works"
else
    test_fail "Short form format flag failed"
fi

kill $FORMAT_SHORT_PID 2>/dev/null || true

# Test multiple locks in list
test_start "Multiple locks in list"
$WAITLOCK --lock-dir "$LOCK_DIR" multi_1 >/dev/null 2>&1 &
MULTI_PID1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" multi_2 >/dev/null 2>&1 &
MULTI_PID2=$!
$WAITLOCK --lock-dir "$LOCK_DIR" multi_3 >/dev/null 2>&1 &
MULTI_PID3=$!

sleep 1

MULTI_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "multi_" || echo 0)
if [ "$MULTI_COUNT" -eq 3 ]; then
    test_pass "Multiple locks shown in list"
else
    test_fail "Expected 3 locks in list, got $MULTI_COUNT"
fi

kill $MULTI_PID1 $MULTI_PID2 $MULTI_PID3 2>/dev/null || true

# Test --all flag
test_start "All locks flag (--all)"
if $WAITLOCK --lock-dir "$LOCK_DIR" --list --all >/dev/null 2>&1; then
    test_pass "All locks flag works"
else
    test_fail "All locks flag failed"
fi

# Test -a short form
test_start "All locks short form (-a)"
if $WAITLOCK --lock-dir "$LOCK_DIR" --list -a >/dev/null 2>&1; then
    test_pass "All locks short form works"
else
    test_fail "All locks short form failed"
fi

# Test --stale-only flag
test_start "Stale only flag (--stale-only)"
if $WAITLOCK --lock-dir "$LOCK_DIR" --list --stale-only >/dev/null 2>&1; then
    test_pass "Stale only flag works"
else
    test_fail "Stale only flag failed"
fi

# Test list with semaphore
test_start "List with semaphore holders"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 sem_list_1 >/dev/null 2>&1 &
SEM_LIST_PID1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 sem_list_2 >/dev/null 2>&1 &
SEM_LIST_PID2=$!

sleep 1

SEM_LIST_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "sem_list" || echo 0)
if [ "$SEM_LIST_COUNT" -eq 2 ]; then
    test_pass "Semaphore holders shown in list"
else
    test_fail "Expected 2 semaphore holders in list, got $SEM_LIST_COUNT"
fi

kill $SEM_LIST_PID1 $SEM_LIST_PID2 2>/dev/null || true

test_suite_end