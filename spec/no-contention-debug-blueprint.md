# No-Contention Hanging Issue Debug Blueprint

## Issue Summary

**Problem**: waitlock hangs in no-contention scenarios (when slots should be immediately available)
**Working**: Timeout functionality works correctly WITH contention (0.2 seconds as expected)
**Isolated**: The hang occurs in the lock acquisition path when slots are available

## Root Cause Analysis Framework

### 1. Issue Boundaries (CONFIRMED)
- ✅ **Timeout logic works** - confirmed by minimal tests
- ✅ **Signal handling works** - processes respond to signals
- ✅ **Argument parsing works** - help/version commands work
- ✅ **Contention scenarios work** - timeout triggers correctly when slots occupied
- ❌ **No-contention scenarios hang** - this is the isolated issue

### 2. Suspected Code Path Analysis

Based on `src/lock/lock.c` acquire_lock() function:

```c
// Line 155: Main acquisition loop
while (1) {
    // Lines 157-225: Stale lock cleanup and counting (LIKELY OK)
    // Lines 227-235: Slot availability check (LIKELY OK)
    // Lines 237-301: ATOMIC SLOT CLAIMING (SUSPECT AREA)
    // Lines 342-360: Timeout check (CONFIRMED OK)
    // Lines 416-444: Sleep/backoff logic (LIKELY OK)
}
```

### 3. Prime Suspect: Atomic Slot Claiming Logic

**Location**: Lines 241-301 in `acquire_lock()`

**Problem Pattern**:
1. `active_locks < max_holders` → slots available
2. Enters slot claiming loop (lines 246-297)
3. For each slot attempt:
   - Creates temp file
   - Writes lock info
   - Attempts atomic rename
   - **HANG LIKELY OCCURS HERE**

**Potential Issues**:
1. **Infinite loop in slot claiming** - all slots appear taken but aren't
2. **File system race condition** - temp file creation/rename fails silently
3. **Signal handler interference** - signal interrupts file operations
4. **Directory permission issues** - can create temp files but can't rename

### 4. Diagnostic Strategy

#### Phase 1: Instrumentation
Add debug logging to isolate exactly where the hang occurs:

```c
debug("PHASE1: Starting slot claiming, active_locks=%d, max_holders=%d", active_locks, max_holders);
for (slot_attempt = 0; slot_attempt < max_holders; slot_attempt++) {
    debug("PHASE1: Attempting slot %d", try_slot);
    
    // Log each major step
    debug("PHASE1: Creating temp file: %s", temp_path);
    fd = open(temp_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    debug("PHASE1: Temp file result: fd=%d, errno=%d", fd, errno);
    
    debug("PHASE1: Attempting rename: %s -> %s", temp_path, lock_path);
    if (rename(temp_path, lock_path) == 0) {
        debug("PHASE1: Rename successful - slot claimed");
        break;
    } else {
        debug("PHASE1: Rename failed: errno=%d (%s)", errno, strerror(errno));
    }
}
```

#### Phase 2: Reproduce and Capture
1. Create minimal reproduction case
2. Run with debug logging
3. Capture strace output
4. Identify exact hanging point

#### Phase 3: Root Cause Isolation
Based on Phase 1-2 results, implement targeted fixes

## Suspected Root Causes (In Priority Order)

### 1. **Slot Counting Race Condition** (HIGH PROBABILITY)
**Problem**: `active_locks` count doesn't match reality
**Symptoms**: Code thinks slots are available but they're all taken
**Location**: Lines 157-225 (lock counting logic)
**Fix**: Improve stale lock detection and counting

### 2. **Atomic Rename Failure Loop** (HIGH PROBABILITY)
**Problem**: rename() fails but error handling is incorrect
**Symptoms**: Loop continues indefinitely on rename failures
**Location**: Lines 283-296 (atomic rename section)
**Fix**: Better error handling and loop termination

### 3. **Signal Handler Interference** (MEDIUM PROBABILITY)
**Problem**: Signal handler interrupts file operations
**Symptoms**: System calls return EINTR but aren't handled
**Location**: Lines 263-296 (file operations)
**Fix**: Add EINTR handling and signal masking

### 4. **Directory Permission Issues** (MEDIUM PROBABILITY)
**Problem**: Can create temp files but can't rename
**Symptoms**: open() succeeds but rename() fails
**Location**: Lines 263-296 (file operations)
**Fix**: Better permission checking and error reporting

### 5. **Temp File Name Collision** (LOW PROBABILITY)
**Problem**: temp file names collide causing infinite retry
**Symptoms**: Multiple processes use same temp file name
**Location**: Lines 255-256 (temp file naming)
**Fix**: Better temp file name uniqueness

## Detailed Code Analysis

### Current Slot Claiming Logic Flow:
```
1. Count active locks (lines 157-225)
2. Check if slots available (line 241)
3. FOR each slot (lines 246-297):
   a. Create temp file path with slot number
   b. Open temp file O_CREAT | O_EXCL
   c. Write lock info to temp file
   d. Attempt atomic rename to final path
   e. If rename succeeds: CLAIM SLOT, break
   f. If rename fails: clean up temp file, try next slot
4. If no slots claimed: check timeout and retry
```

### Critical Issues in Current Logic:

#### Issue 1: Inconsistent Slot Counting
```c
// Lines 157-225: Count active locks
while ((entry = readdir(dir)) != NULL) {
    // Complex logic to count active locks
    // PROBLEM: May not accurately reflect available slots
}
```

#### Issue 2: Slot Claiming Race Window
```c
// Lines 246-297: Slot claiming loop
for (slot_attempt = 0; slot_attempt < max_holders; slot_attempt++) {
    // PROBLEM: No guarantee that counted slots are still available
    // Another process may claim slots between count and claim
}
```

#### Issue 3: Insufficient Error Handling
```c
// Lines 283-296: Atomic rename
if (rename(temp_path, lock_path) == 0) {
    slot_claimed = try_slot;
    break;
} else {
    // PROBLEM: Only handles EEXIST, ignores other errors
    if (errno != EEXIST) {
        error(E_SYSTEM, "Cannot create lock file: %s", strerror(errno));
        return E_SYSTEM;
    }
}
```

## Fix Strategy

### Phase 1: Immediate Debug Fix
1. Add comprehensive logging to slot claiming logic
2. Add timeout protection to slot claiming loop
3. Improve error handling for file operations

### Phase 2: Structural Fix
1. Simplify slot claiming logic
2. Remove race conditions between counting and claiming
3. Add proper signal handling during file operations

### Phase 3: Robustness Improvements
1. Better temp file naming
2. Improved stale lock detection
3. Enhanced error reporting

## Test Plan

### Minimal Reproduction Test
```bash
# Should complete immediately but currently hangs
timeout 2 ./waitlock --timeout 0.1 --lock-dir /tmp/test_lock test_desc
```

### Comprehensive Test Suite
1. Run existing diagnostic tests
2. Add specific no-contention tests
3. Test with different file systems
4. Test with different signal conditions

## Success Criteria

1. **No-contention scenarios complete immediately** (< 0.1 seconds)
2. **Existing contention behavior preserved** (timeout works correctly)
3. **All diagnostic tests pass**
4. **No new hanging scenarios introduced**

## Implementation Priority

1. **IMMEDIATE**: Add debug logging and timeout protection
2. **HIGH**: Fix slot claiming race conditions
3. **MEDIUM**: Improve error handling
4. **LOW**: Optimize performance and cleanup

This blueprint provides a systematic approach to debug and fix the no-contention hanging issue while preserving the working timeout functionality.