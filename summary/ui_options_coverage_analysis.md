# UI Options Test Coverage Analysis

## Already Covered in Existing Tests ✅

### Core Options (`test/test_core.c:test_parse_args()`)
- ✅ **Basic descriptor parsing**: `waitlock test_descriptor`
- ✅ **--done mode**: `waitlock --done test_descriptor`
- ✅ **Semaphore (-m)**: `waitlock -m 5 test_descriptor`
- ✅ **Semaphore (--allowMultiple)**: `waitlock --allowMultiple 3 test_descriptor`  
- ✅ **Check mode**: `waitlock --check test_descriptor`
- ✅ **List mode**: `waitlock --list`
- ✅ **Timeout (-t)**: `waitlock -t 30 test_descriptor`
- ✅ **Timeout (--timeout)**: `waitlock --timeout 45.5 test_descriptor`
- ✅ **Exec mode**: `waitlock --exec echo hello test_descriptor`

### Validation Tests (`test/test_core.c:test_argument_validation()`)
- ✅ **Invalid descriptors**: `invalid@descriptor` rejected
- ✅ **Missing descriptors**: `waitlock` (no args) rejected  
- ✅ **Invalid semaphore values**: negative, zero, non-numeric
- ✅ **Invalid timeout values**: negative values rejected

### Utility Functions (`test/test_core.c`)
- ✅ **Syslog facility parsing**: All valid facilities (daemon, local0-7)
- ✅ **Help/Usage output**: `usage()` function tested
- ✅ **Version output**: `version()` function tested
- ✅ **Debug output**: `debug()` function with verbose on/off
- ✅ **Error output**: `error()` function with quiet mode
- ✅ **Environment variables**: WAITLOCK_DEBUG, WAITLOCK_TIMEOUT, etc.

### Integration Tests (`test/test_integration.c`)
- ✅ **Check functionality**: `check_lock()` with active/inactive locks
- ✅ **List functionality**: Various output formats tested
- ✅ **Done functionality**: `done_lock()` signal handling

## Gaps in Test Coverage ❌

### Missing Command-Line Options
- ❌ **onePerCPU (-c/--onePerCPU)**: No dedicated tests found
- ❌ **excludeCPUs (-x/--excludeCPUs)**: No dedicated tests found  
- ❌ **Quiet mode (-q/--quiet)**: Only tested indirectly via error output
- ❌ **Verbose mode (-v/--verbose)**: Only tested indirectly via debug output
- ❌ **Lock directory (-d/--lock-dir)**: No parsing tests found
- ❌ **Syslog (--syslog)**: Facility parsing tested, but not flag itself
- ❌ **Syslog facility (--syslog-facility)**: Integration not tested

### Missing Format Option Tests
- ❌ **List format validation**: Only basic list tested, not format combinations
- ❌ **All flag (--all)**: Not systematically tested
- ❌ **Stale-only flag (--stale-only)**: Not systematically tested

### Missing Option Combinations
- ❌ **Incompatible mode combinations**: e.g. `--check` + `--exec`
- ❌ **Complex multi-option scenarios**: e.g. `--timeout 5 --allowMultiple 3 --verbose`
- ❌ **CPU options with semaphores**: `--onePerCPU` + `--allowMultiple`

### Missing Edge Cases
- ❌ **Descriptor length limits**: 255+ character descriptors
- ❌ **Special characters in paths**: `--lock-dir` with spaces/special chars
- ❌ **Numeric edge cases**: Very large timeout values, max int semaphore values
- ❌ **Help/version with other args**: `--help --verbose`, etc.

## Recommendation: Focus on Gaps

Instead of creating redundant tests, focus on the **missing coverage areas**:

1. **CPU-based locking options** (onePerCPU, excludeCPUs)
2. **Output control options** (quiet, verbose flags)
3. **Directory and syslog integration**
4. **Option combination validation**
5. **Edge case validation**

The existing test suite in `test_core.c` already provides excellent coverage of the core argument parsing functionality.