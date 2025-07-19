# WaitLock Design and Implementation

## Implementation Status

The current implementation covers ~80% of the specification, with key missing features around robustness, portability, and enterprise functionality.

### ✅ Implemented Features
- Core mutex/semaphore functionality
- Signal handling and cleanup
- Command-line interface
- Syslog integration
- Environment variable support
- Basic cross-platform support

### ❌ Missing Features
- Advanced error recovery
- Network-based distributed locking
- Performance optimizations
- Enhanced monitoring capabilities

## Usage Best Practices

### Foreground Execution (RECOMMENDED)

**DO USE foreground execution for script coordination:**

```bash
# ✅ CORRECT - Foreground execution
waitlock myapp || {
    echo "Another instance is already running"
    exit 1
}
# Clear success/failure indication
# Automatic cleanup when script exits
```

**WHY foreground is better:**
- ✅ **Reliable** - Clear success/failure indication
- ✅ **Simple** - No PID management or manual cleanup needed
- ✅ **Safe** - No race conditions
- ✅ **Automatic** - Lock released when process exits

### Background Execution (AVOID for production)

**DON'T USE background execution for script coordination:**

```bash
# ❌ WRONG - Don't do this for script coordination
waitlock myapp &
# This returns immediately, whether lock was acquired or not
# Both scripts may think they got the lock
# Requires complex PID management and cleanup
```

### When Background is Acceptable

- ⚠️ **Only for testing** - When you need to verify lock behavior
- ⚠️ **Never for production** - Use `--exec` or foreground instead

### Command Execution Mode (BEST PRACTICE)

```bash
# ✅ BEST - Execute command while holding lock
waitlock database_backup --exec bash -c "
    mysqldump --all-databases > backup.sql
    gzip backup.sql
    echo 'Backup completed'
"
```

### Semaphore Best Practices

```bash
# ✅ Good - CPU-aware semaphore
waitlock --onePerCPU --excludeCPUs 2 cpu_intensive_task || {
    echo "All CPU slots are busy"
    exit 1
}

# ✅ Good - Fixed semaphore count
waitlock --allowMultiple 4 download_pool || {
    echo "Too many downloads already running"
    exit 1
}
```

### Error Handling

```bash
# ✅ Robust error handling
waitlock --timeout 30 critical_resource
case $? in
    0) echo "Lock acquired successfully" ;;
    1) echo "Lock is busy" >&2; exit 1 ;;
    2) echo "Timeout expired" >&2; exit 1 ;;
    3) echo "Usage error" >&2; exit 1 ;;
    *) echo "Unexpected error" >&2; exit 1 ;;
esac
```

## Implementation Architecture

### Lock File Format
- Magic number (0x57414C4B = "WALK")
- Process metadata (PID, PPID, UID)
- Lock information (type, slot, max holders)
- Timestamps and command line
- CRC32 checksum for integrity

### Platform Support
- Linux (glibc, musl)
- FreeBSD, OpenBSD, NetBSD
- macOS
- Platform-specific optimizations

### Performance Considerations
- Lock files stored in `/var/lock/waitlock` or `/tmp/waitlock`
- Directory scanning is O(n) where n = number of lock files
- Use hierarchical descriptors for namespace separation
- Consider tmpfs for high-frequency locking

## Future Implementation Priorities

1. **Enhanced Error Recovery** - Better handling of filesystem issues
2. **Distributed Locking** - Network-based coordination
3. **Performance Optimization** - Reduce lock acquisition overhead
4. **Monitoring Integration** - Better observability tools