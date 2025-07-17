# WaitLock

**WaitLock** is a portable UNIX/POSIX command-line tool that provides mutex and semaphore functionality for shell scripts. It enables synchronized access to resources across multiple processes with automatic cleanup when processes die.

[![Build Status](https://github.com/user/waitlock/workflows/CI/badge.svg)](https://github.com/user/waitlock/actions)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.0-green.svg)](https://github.com/user/waitlock/releases)

## Features

- **Mutex Mode**: Single lock holder (default)
- **Semaphore Mode**: Multiple concurrent lock holders
- **Automatic Cleanup**: Locks released when process dies
- **Signal-based Release**: Clean lock release with `--done` flag
- **CPU-aware Locking**: Can scale locks to CPU count
- **Lock Inspection**: List and check active locks
- **Multiple Output Formats**: Human, CSV, and null-separated
- **Command Execution**: Run commands while holding locks
- **UNIX Integration**: Environment variables, stdin, syslog
- **Portable C Implementation**: Runs on any POSIX system

## Quick Start

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install build-essential autoconf

# Build and install
./configure
make
sudo make install

# Basic usage - acquire exclusive lock
waitlock myapp &
# ... do exclusive work ...
waitlock --done myapp

# Execute command with lock
waitlock database_backup --exec "/usr/local/bin/backup.sh --daily"

# List active locks
waitlock --list
```

## Table of Contents

- [Installation](#installation)
- [Usage](#usage)
- [Examples](#examples)
- [Command Reference](#command-reference)
- [Environment Variables](#environment-variables)
- [Exit Codes](#exit-codes)
- [Advanced Usage](#advanced-usage)
- [Contributing](#contributing)
- [License](#license)

## Installation

### From Source

#### Prerequisites

- C compiler (gcc, clang, or compatible)
- GNU Make
- autoconf (for building from git)

#### Build Instructions

```bash
# Clone the repository
git clone https://github.com/user/waitlock.git
cd waitlock

# Generate configure script (if building from git)
autoreconf -fi

# Configure and build
./configure
make

# Run tests
make check

# Install system-wide
sudo make install

# Or install to custom prefix
./configure --prefix=/usr/local
make install
```

#### Build Options

```bash
# Debug build
./configure CFLAGS="-g -O0 -DDEBUG"

# Release build with optimizations
./configure CFLAGS="-O2 -DNDEBUG"

# Cross-compilation example
./configure --host=arm-linux-gnueabihf
```

### Package Installation

```bash
# Ubuntu/Debian (when available)
sudo apt-get install waitlock

# CentOS/RHEL (when available)
sudo yum install waitlock

# macOS with Homebrew (when available)
brew install waitlock
```

## Usage

### Basic Syntax

```bash
waitlock [options] <descriptor>
waitlock --list [--format=<fmt>] [--all|--stale-only]
waitlock --check <descriptor>
echo <descriptor> | waitlock [options]
```

### Simple Examples

```bash
# Acquire mutex lock
waitlock myapp &
JOB_PID=$!
# ... do exclusive work ...
waitlock --done myapp

# Check if lock is available
if waitlock --check myapp; then
    echo "Lock is available"
else
    echo "Lock is held by another process"
fi

# Execute command while holding lock
waitlock backup_job --exec rsync -av /source /destination

# Use with timeout
waitlock --timeout 30 critical_resource || echo "Timeout!"
```

## Examples

### 1. Basic Mutex (Exclusive Access)

```bash
#!/bin/bash
# Ensure only one backup process runs at a time

waitlock database_backup || {
    echo "Another backup is already running"
    exit 1
}

# Perform backup
mysqldump --all-databases > backup.sql
gzip backup.sql

# Lock automatically released when script exits
```

### 2. Semaphore (Multiple Concurrent Access)

```bash
#!/bin/bash
# Allow up to 4 concurrent download processes

waitlock --allowMultiple 4 download_pool || {
    echo "Too many downloads already running"
    exit 1
}

# Perform download
wget "https://example.com/file.tar.gz"

# Lock automatically released when script exits
```

### 3. CPU-Based Semaphore

```bash
#!/bin/bash
# Use one lock per CPU core, reserving 2 cores for system

waitlock --onePerCPU --excludeCPUs 2 cpu_intensive_task || {
    echo "All CPU slots are busy"
    exit 1
}

# Run CPU-intensive task
./compute_job.sh
```

### 4. Command Execution Mode

```bash
#!/bin/bash
# Execute command while holding lock (recommended approach)

waitlock database_backup --exec bash -c "
    mysqldump --all-databases > backup.sql
    gzip backup.sql
    echo 'Backup completed'
"
```

### 5. Lock Monitoring and Management

```bash
#!/bin/bash
# Monitor active locks

# List all locks in human-readable format
waitlock --list

# List in CSV format for parsing
waitlock --list --format csv

# Show only stale locks
waitlock --list --stale-only

# Count active locks
waitlock --list --format csv | tail -n +2 | wc -l
```

### 6. Pipeline and Batch Processing

```bash
#!/bin/bash
# Process files with controlled parallelism

find /data -name "*.csv" | while read file; do
    basename "$file" | waitlock --allowMultiple 3 --exec process_file "$file"
done

# Or with xargs for better performance
find /data -name "*.csv" | \
    xargs -P 10 -I {} sh -c 'waitlock -m 3 batch_processor --exec "process_file {}"'
```

### 7. Using with Environment Variables

```bash
#!/bin/bash
# Configure via environment variables

export WAITLOCK_TIMEOUT=60
export WAITLOCK_DIR="/var/lock/myapp"
export WAITLOCK_DEBUG=1

waitlock myapp_task --syslog --syslog-facility local0
```

### 8. Error Handling

```bash
#!/bin/bash
# Robust error handling

waitlock --timeout 30 critical_resource
case $? in
    0) echo "Lock acquired successfully" ;;
    1) echo "Lock is busy" >&2; exit 1 ;;
    2) echo "Timeout expired" >&2; exit 1 ;;
    3) echo "Usage error" >&2; exit 1 ;;
    *) echo "Unexpected error" >&2; exit 1 ;;
esac

# Your critical section here
perform_critical_operation
```

### 9. Signal-based Lock Release

```bash
#!/bin/bash
# Clean lock release using --done flag

# Start long-running process with lock
waitlock long_running_task &
LOCK_PID=$!

# Simulate some work
sleep 2

# Later, signal the process to release the lock cleanly
waitlock --done long_running_task

# Wait for the process to exit gracefully
wait $LOCK_PID
echo "Process exited with code: $?"
```

### 10. Resource Pool Management

```bash
#!/bin/bash
# Manage GPU resources

# Export slot number for GPU selection
waitlock --allowMultiple 4 gpu_pool &
LOCK_PID=$!

# Wait for lock and get slot number
wait $LOCK_PID
if [ $? -eq 0 ]; then
    # Use WAITLOCK_SLOT environment variable
    export CUDA_VISIBLE_DEVICES=$WAITLOCK_SLOT
    ./gpu_computation.py
fi
```

### 11. Distributed Locking (NFS)

```bash
#!/bin/bash
# Coordinate across multiple machines using NFS

export WAITLOCK_DIR="/mnt/shared/locks"

waitlock cluster_job --timeout 300 --exec bash -c "
    echo 'Running on $(hostname)'
    ./distributed_task.sh
"
```

## Command Reference

### Core Options

| Option | Description |
|--------|-------------|
| `-m, --allowMultiple N` | Allow N concurrent holders (semaphore mode) |
| `-c, --onePerCPU` | Allow one lock per CPU core |
| `-x, --excludeCPUs N` | Reserve N CPUs (reduce available locks by N) |
| `-t, --timeout SECS` | Maximum wait time before giving up |
| `--check` | Test if lock is available without acquiring |
| `--done` | Signal lock holder to release lock (sends SIGTERM) |
| `-e, --exec CMD` | Execute command while holding lock |

### Output Options

| Option | Description |
|--------|-------------|
| `-q, --quiet` | Suppress all non-error output |
| `-v, --verbose` | Verbose output for debugging |
| `-f, --format FMT` | Output format: human, csv, null |
| `--syslog` | Log operations to syslog |
| `--syslog-facility FAC` | Syslog facility (daemon\|local0-7) |

### Management Options

| Option | Description |
|--------|-------------|
| `-l, --list` | List active locks and exit |
| `-a, --all` | Include stale locks in list |
| `--stale-only` | Show only stale locks |

### Configuration Options

| Option | Description |
|--------|-------------|
| `-d, --lock-dir DIR` | Directory for lock files |
| `-h, --help` | Show usage information |
| `-V, --version` | Show version information |

## Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `WAITLOCK_DIR` | Lock directory path | auto-detect |
| `WAITLOCK_TIMEOUT` | Default timeout in seconds | infinite |
| `WAITLOCK_DEBUG` | Enable debug output | disabled |
| `WAITLOCK_SLOT` | Preferred semaphore slot | auto |

### Environment Variable Examples

```bash
# Set default timeout
export WAITLOCK_TIMEOUT=300

# Use custom lock directory
export WAITLOCK_DIR="/var/lock/myapp"

# Enable debug output
export WAITLOCK_DEBUG=1

# Prefer specific semaphore slot
export WAITLOCK_SLOT=2
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Lock is busy |
| 2 | Timeout expired |
| 3 | Usage error |
| 4 | System error |
| 5 | Permission denied |
| 6 | Lock directory not accessible |
| 75 | Temporary failure |
| 126 | Command not executable |
| 127 | Command not found |

## Advanced Usage

### Syslog Integration

```bash
# Log all operations to syslog
waitlock --syslog --syslog-facility local0 myapp

# Monitor syslog for lock operations
tail -f /var/log/syslog | grep waitlock
```

### Lock File Format

WaitLock uses binary lock files with the following structure:
- Magic number (0x57414C4B = "WALK")
- Process metadata (PID, PPID, UID)
- Lock information (type, slot, max holders)
- Timestamps and command line
- CRC32 checksum for integrity

### Platform Support

WaitLock is tested on:
- Linux (glibc, musl)
- FreeBSD
- OpenBSD
- NetBSD
- macOS

### Performance Considerations

- Lock files are stored in `/var/lock/waitlock` (system) or `/tmp/waitlock` (user)
- Directory scanning is O(n) where n = number of lock files
- Use hierarchical descriptors for namespace separation
- Consider tmpfs for high-frequency locking

### Troubleshooting

#### Common Issues

1. **Permission Denied**
   ```bash
   # Check directory permissions
   ls -la /var/lock/waitlock
   
   # Use user-specific directory
   export WAITLOCK_DIR="$HOME/.waitlock"
   ```

2. **Stale Locks**
   ```bash
   # List stale locks
   waitlock --list --stale-only
   
   # Clean up automatically (locks are cleaned on next access)
   waitlock --check any_descriptor
   ```

3. **High Contention**
   ```bash
   # Monitor lock contention
   waitlock --verbose --timeout 1 busy_resource
   
   # Use exponential backoff (built-in)
   waitlock --timeout 60 busy_resource
   ```

#### Debug Mode

```bash
# Enable debug output
export WAITLOCK_DEBUG=1
waitlock --verbose myapp

# Or use command line
waitlock --verbose myapp
```

## Contributing

### Development Setup

```bash
# Clone repository
git clone https://github.com/user/waitlock.git
cd waitlock

# Install development dependencies
sudo apt-get install autoconf automake libtool

# Generate build files
autoreconf -fi

# Configure for development
./configure --enable-debug CFLAGS="-g -O0"

# Build and test
make
make check
```

### Running Tests

```bash
# Run internal test suite
./src/waitlock --test

# Run shell-based tests
./test_basic.sh
./test_semaphore.sh
./test_timeout.sh
```

### Code Style

- Follow POSIX C89/C90 standards
- Use 4-space indentation
- Include comprehensive error handling
- Add tests for new features

### Submitting Changes

1. Fork the repository
2. Create a feature branch
3. Make changes with tests
4. Submit a pull request

## License

WaitLock is released under the MIT License. See [LICENSE](LICENSE) for details.

## Support

- **Documentation**: See `man waitlock` after installation
- **Issues**: Report bugs on [GitHub Issues](https://github.com/user/waitlock/issues)
- **Discussions**: Join discussions on [GitHub Discussions](https://github.com/user/waitlock/discussions)

## Acknowledgments

WaitLock was designed following UNIX philosophy principles and inspired by tools like `flock(1)`, `lockfile(1)`, and `sem(1)`. Special thanks to the POSIX standards committee for providing a solid foundation for portable system programming.