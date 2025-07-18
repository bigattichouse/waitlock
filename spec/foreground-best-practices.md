# Foreground Execution Best Practices

## Overview

This document outlines the recommended approach for using waitlock in production scripts, emphasizing foreground execution over background execution for script coordination.

**Last Updated:** 2024-07-18  
**Status:** Active  
**Priority:** High

## Executive Summary

**Use foreground execution for script coordination, not background execution (`&`).**

Background execution creates race conditions and requires complex cleanup. Foreground execution is simpler, more reliable, and provides clear success/failure indication.

## Recommended Approaches

### ✅ Method 1: Foreground with Error Handling (RECOMMENDED)

```bash
#!/bin/bash
# Acquire lock and proceed only if successful
waitlock myapp || {
    echo "Another instance is already running"
    exit 1
}

# Do critical work
perform_critical_operations

# Lock automatically released when script exits
```

**Benefits:**
- Clear success/failure indication
- Automatic cleanup when script exits
- No race conditions
- Simple error handling

### ✅ Method 2: Command Execution Mode (CLEANEST)

```bash
#!/bin/bash
# Execute command while holding lock
waitlock myapp --exec "./critical_script.sh"
```

**Benefits:**
- Cleanest approach
- Automatic lock management
- Signal forwarding
- Exit code propagation

### ✅ Method 3: Timeout-based Coordination

```bash
#!/bin/bash
# Try to acquire lock with timeout
case $(waitlock --timeout 30 myapp; echo $?) in
    0) echo "Lock acquired, proceeding..."
       perform_critical_operations
       ;;
    1) echo "Lock is busy" >&2; exit 1 ;;
    2) echo "Timeout expired" >&2; exit 1 ;;
    *) echo "Unexpected error" >&2; exit 1 ;;
esac
```

**Benefits:**
- Configurable wait time
- Detailed error handling
- Prevents indefinite blocking

### ✅ Method 4: Immediate Check (No Waiting)

```bash
#!/bin/bash
# Try to acquire lock immediately, fail if busy
waitlock --timeout 0 myapp || {
    echo "Resource is busy, try again later"
    exit 1
}

# Proceed with work
perform_critical_operations
```

**Benefits:**
- No waiting/blocking
- Immediate feedback
- Good for optional operations

## ⚠️ AVOID: Background Execution for Production

### ❌ Don't Do This in Production Scripts

```bash
#!/bin/bash
# ❌ WRONG - Creates race conditions and complexity
waitlock myapp &
LOCK_PID=$!

# This may proceed even if lock wasn't acquired
# Requires complex PID management and cleanup
perform_critical_operations

# Manual cleanup required
kill $LOCK_PID
```

**Problems with background execution:**
- ❌ Race conditions - both processes may think they got the lock
- ❌ Complex cleanup - requires manual PID management
- ❌ Unreliable detection - hard to know if lock was actually acquired
- ❌ Error-prone - easy to forget cleanup or handle signals incorrectly

### When Background Execution is Acceptable

Background execution should **only** be used for:

1. **Testing purposes** - When verifying lock behavior in test suites
2. **Test frameworks** - When multiple processes need coordination for testing
3. **Signal-based coordination** - When using `--done` for demonstration

**Never use background execution in production scripts.**

## Comparison Matrix

| Aspect | Foreground | Background (`&`) | `--exec` Mode |
|--------|------------|------------------|---------------|
| **Reliability** | ✅ High | ❌ Low | ✅ High |
| **Simplicity** | ✅ Simple | ❌ Complex | ✅ Very Simple |
| **Cleanup** | ✅ Automatic | ❌ Manual | ✅ Automatic |
| **Error Handling** | ✅ Clear | ❌ Unclear | ✅ Clear |
| **Race Conditions** | ✅ None | ❌ Possible | ✅ None |
| **Production Use** | ✅ Recommended | ❌ Avoid | ✅ Recommended |
| **Testing Use** | ✅ Good | ✅ Useful | ✅ Good |

## Common Patterns

### Pattern 1: Exclusive Script Execution

```bash
#!/bin/bash
# Ensure only one instance of this script runs
SCRIPT_NAME=$(basename "$0")
waitlock "$SCRIPT_NAME" || {
    echo "Another instance of $SCRIPT_NAME is already running"
    exit 1
}

# Script logic here
main_function
```

### Pattern 2: Resource Pool Management

```bash
#!/bin/bash
# Manage limited resources (e.g., GPUs, network connections)
waitlock --allowMultiple 4 gpu_pool || {
    echo "All GPU slots are busy"
    exit 1
}

# Use WAITLOCK_SLOT for resource selection
export CUDA_VISIBLE_DEVICES=$WAITLOCK_SLOT
run_gpu_computation
```

### Pattern 3: Critical Section Protection

```bash
#!/bin/bash
# Protect critical sections in larger scripts
function critical_section() {
    waitlock critical_operation || {
        echo "Critical operation already in progress"
        return 1
    }
    
    # Critical code here
    update_shared_resource
    
    # Lock automatically released when function exits
}
```

### Pattern 4: Distributed Coordination

```bash
#!/bin/bash
# Coordinate across multiple machines via NFS
export WAITLOCK_DIR="/mnt/shared/locks"

waitlock cluster_job --timeout 300 || {
    echo "Cluster job already running on another node"
    exit 1
}

# Run distributed task
run_cluster_computation
```

## Migration Guide

### From Background to Foreground

**Old Pattern:**
```bash
waitlock myapp &
LOCK_PID=$!
do_work
kill $LOCK_PID
```

**New Pattern:**
```bash
waitlock myapp || exit 1
do_work
# Lock automatically released
```

### From Manual to Exec Mode

**Old Pattern:**
```bash
waitlock myapp || exit 1
./my_script.sh
```

**New Pattern:**
```bash
waitlock myapp --exec "./my_script.sh"
```

## Testing Considerations

### Testing with Foreground Approach

```bash
#!/bin/bash
# Test script coordination
test_coordination() {
    # Start first instance
    waitlock test_coordination --exec "sleep 5" &
    PID1=$!
    
    # Try to start second instance (should fail)
    if waitlock --timeout 1 test_coordination --exec "sleep 1"; then
        echo "FAIL: Second instance should have been blocked"
        return 1
    else
        echo "PASS: Second instance was properly blocked"
    fi
    
    # Wait for first instance to complete
    wait $PID1
    
    # Now second instance should succeed
    if waitlock --timeout 1 test_coordination --exec "sleep 1"; then
        echo "PASS: Second instance succeeded after first completed"
        return 0
    else
        echo "FAIL: Second instance should have succeeded"
        return 1
    fi
}
```

## Performance Considerations

### Foreground vs Background Performance

- **Foreground**: Slightly lower overhead, simpler code path
- **Background**: Higher overhead due to process management
- **Exec Mode**: Optimal performance, minimal overhead

### Resource Usage

- **Foreground**: One process per lock
- **Background**: Two processes per lock (parent + waitlock)
- **Exec Mode**: One process per lock, optimal resource usage

## Conclusion

Foreground execution is the recommended approach for production script coordination with waitlock. It provides better reliability, simpler code, and automatic cleanup while avoiding the race conditions and complexity of background execution.

**Summary of recommendations:**
1. ✅ Use foreground execution for script coordination
2. ✅ Use `--exec` mode for cleanest approach
3. ✅ Use timeout for non-blocking scenarios
4. ⚠️ Only use background execution for testing
5. ❌ Never use background execution in production

This approach makes waitlock scripts more maintainable, reliable, and easier to debug.

---

**Document Status:** Active  
**Next Review:** 2024-08-18  
**Related Documents:** 
- `README.md` - Updated examples
- `spec/test-coverage-analysis.md` - Best practices section
- `test/foreground_coordination_test.sh` - Test implementation