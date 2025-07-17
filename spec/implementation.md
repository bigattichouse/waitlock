# WaitLock Implementation Blueprint

## Overview

This document outlines the missing features and implementation tasks needed to complete the waitlock tool according to the design blueprint. The current implementation covers ~80% of the specification, with key missing features around robustness, portability, and enterprise functionality.

## Current Status

### ✅ Implemented Features
- Core mutex/semaphore functionality
- Lock acquisition, release, timeout handling
- Process existence checking
- Lock listing with multiple output formats (human, CSV, null)
- Environment variable support (WAITLOCK_TIMEOUT, WAITLOCK_DIR, WAITLOCK_DEBUG)
- Basic signal handling and cleanup
- Command line argument parsing
- Comprehensive test suite (42 tests)
- Cross-platform file locking

### 🔴 High Priority Missing Features

#### 1. Lock File Checksums
**Status:** Missing
**Priority:** High
**Description:** Implement CRC32 or SHA-256 checksum calculation and validation for lock file integrity
**Implementation:**
- Add checksum calculation using modern standards (CRC32 or SHA-256)
- Validate checksums when reading lock files
- Handle corrupted lock files gracefully
- Use portable checksum implementation

#### 2. Signal Forwarding in Exec Mode
**Status:** Missing
**Priority:** High
**Description:** Forward signals to child processes in --exec mode
**Implementation:**
- Set up signal handlers that forward SIGTERM, SIGINT, SIGHUP, SIGQUIT to child
- Maintain PID of child process for signal forwarding
- Ensure proper cleanup when child is terminated by signal
- Exit with correct status code (128 + signal number)

#### 3. --syslog-facility Option
**Status:** Missing
**Priority:** High
**Description:** Add configuration for syslog facility selection
**Implementation:**
- Add --syslog-facility command line option
- Support facilities: daemon, local0, local1, local2, local3, local4, local5, local6, local7
- Map facility strings to LOG_DAEMON, LOG_LOCAL0-7 constants
- Update usage and help text

#### 4. Text Fallback Format for Lock Files
**Status:** Missing
**Priority:** High
**Description:** Implement human-readable text format when binary format fails
**Implementation:**
- KEY=VALUE format as specified in blueprint
- Fallback when binary write/read fails
- Include all lock_info fields in text format
- Ensure atomicity with temp files

### 🟡 Medium Priority Features

#### 5. Enhanced Syslog Logging
**Status:** Partial
**Priority:** Medium
**Description:** Comprehensive logging of all lock operations
**Implementation:**
- Log lock acquisition, release, timeout, conflicts
- Use structured format: "waitlock[PID]: action 'descriptor' details"
- Support different log levels (INFO, WARNING)
- Respect syslog facility configuration

#### 6. Portable Process Detection
**Status:** Partial
**Priority:** Medium
**Description:** Add BSD/macOS sysctl implementations
**Implementation:**
- Implement sysctl-based process command line detection for BSD/macOS
- Add HW_NCPU sysctl for CPU counting on BSD systems
- Maintain fallback to ps command for other systems
- Test on multiple platforms

#### 7. Comprehensive Build System
**Status:** Basic
**Priority:** Medium
**Description:** Full autoconf/automake build system
**Implementation:**
- Enhanced configure.ac with feature detection
- Proper Makefile.in with installation targets
- Man page installation
- Platform-specific feature detection
- Package creation support

#### 8. Man Page Documentation
**Status:** Missing
**Priority:** Medium
**Description:** Create waitlock.1 man page
**Implementation:**
- Standard UNIX man page format
- Complete usage examples
- Exit codes documentation
- Environment variables section
- See also references

### 🟢 Lower Priority Enhancements

#### 9. WAITLOCK_SLOT Environment Variable
**Status:** Missing
**Priority:** Low
**Description:** Semaphore slot assignment for resource pools
**Implementation:**
- Export WAITLOCK_SLOT=N for semaphore holders
- Sequential slot assignment (0, 1, 2, ...)
- Useful for GPU selection, worker assignment
- Document in man page and examples

#### 10. Enhanced Error Reporting
**Status:** Basic
**Priority:** Low
**Description:** Better error messages with context
**Implementation:**
- Include operation context in error messages
- More descriptive error messages
- Respect --verbose flag for detailed errors
- Consistent error format

## Implementation Plan

### Phase 1: Critical Robustness (Week 1)
1. **Lock File Checksums** - Prevent corruption issues
2. **Signal Forwarding** - Essential for exec mode reliability
3. **Text Fallback Format** - Debugging and recovery

### Phase 2: Enterprise Features (Week 2)  
4. **--syslog-facility Option** - Production logging control
5. **Enhanced Syslog Logging** - Complete audit trail
6. **Man Page Documentation** - Professional deployment

### Phase 3: Portability & Polish (Week 3)
7. **Portable Process Detection** - BSD/macOS support
8. **Comprehensive Build System** - Distribution ready
9. **WAITLOCK_SLOT** - Advanced use cases
10. **Enhanced Error Reporting** - User experience

## Technical Standards

### Checksum Implementation
- **Primary:** CRC32 (fast, portable, sufficient for corruption detection)
- **Alternative:** SHA-256 (if cryptographic integrity needed)
- **Library:** Use built-in or minimal portable implementation

### Signal Handling
- **Standard:** POSIX sigaction() for reliable signal handling
- **Signals:** SIGTERM, SIGINT, SIGHUP, SIGQUIT forwarding
- **Exit Codes:** Follow UNIX convention (128 + signal number)

### Text Format
- **Format:** KEY=VALUE pairs, one per line
- **Atomicity:** Write to temp file, then rename
- **Encoding:** UTF-8 compatible, escape special characters

### Build System
- **Tool:** Autoconf 2.69+ for maximum compatibility
- **Standards:** GNU coding standards compliance
- **Targets:** all, install, clean, dist, check

## Testing Requirements

Each new feature must include:
- Unit tests in waitlock-test.c
- Integration tests for multi-process scenarios
- Platform-specific testing where applicable
- Documentation updates

## Success Criteria

The implementation is complete when:
- All high-priority features are implemented and tested
- Test suite passes on Linux, BSD, and macOS
- Man page is complete and accurate
- Build system supports standard GNU/autoconf workflow
- Code follows POSIX C89/C90 standards for maximum portability

## Timeline

- **Week 1:** Phase 1 (Critical Robustness)
- **Week 2:** Phase 2 (Enterprise Features)  
- **Week 3:** Phase 3 (Portability & Polish)

Each phase builds upon the previous, ensuring a stable foundation throughout development.