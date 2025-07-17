#!/bin/bash
# Test help and version command-line options

set -e
source "$(dirname "$0")/test_framework.sh"

test_suite_start "Help and Version Commands"

# Test --help flag
test_start "Help flag (--help)"
if $WAITLOCK --help >/dev/null 2>&1; then
    test_pass "Help output accessible"
else
    test_fail "Help command failed"
fi

# Test -h flag
test_start "Help flag (-h)"
if $WAITLOCK -h >/dev/null 2>&1; then
    test_pass "Short help flag works"
else
    test_fail "Short help flag failed"
fi

# Test --version flag
test_start "Version flag (--version)"
if $WAITLOCK --version >/dev/null 2>&1; then
    test_pass "Version output accessible"
else
    test_fail "Version command failed"
fi

# Test -V flag
test_start "Version flag (-V)"
if $WAITLOCK -V >/dev/null 2>&1; then
    test_pass "Short version flag works"
else
    test_fail "Short version flag failed"
fi

# Test help output contains key sections
test_start "Help output completeness"
HELP_OUTPUT=$($WAITLOCK --help 2>&1)
if echo "$HELP_OUTPUT" | grep -q -i "usage" && \
   echo "$HELP_OUTPUT" | grep -q -i "options" && \
   echo "$HELP_OUTPUT" | grep -q -i "examples"; then
    test_pass "Help output contains required sections"
else
    test_fail "Help output missing required sections"
fi

# Test version output format
test_start "Version output format"
VERSION_OUTPUT=$($WAITLOCK --version 2>&1)
if echo "$VERSION_OUTPUT" | grep -q -E "waitlock.*[0-9]+\.[0-9]+"; then
    test_pass "Version output properly formatted"
else
    test_fail "Version output format incorrect"
fi

test_suite_end