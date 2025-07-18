# Failing Tests Analysis Blueprint

## Current Test Status Summary

After resolving the no-contention hanging issue, the overall test pass rate improved from 25% to 86%. However, there are still 7 failing tests (14%) that need to be addressed. This blueprint analyzes the remaining failures and provides a strategy for resolution.

## Test Suite Status

### ✅ **Fully Passing Suites**
- **Diagnostic Tests**: 8/8 passed (100%)

### ❌ **Suites with Failures**
- **Core Functionality**: 9/10 passed (90%) - 1 test not completing properly
- **Expanded Test Suite**: 21/25 passed (84%) - 4 failures  
- **Comprehensive Test**: 4/6 passed (67%) - 2 failures

### 🔍 **Counting Anomalies**
- Core functionality test reports "9 passed, 0 failed" from 10 total tests
- This suggests 1 test is not completing/being counted rather than explicitly failing
- Need to investigate test execution flow and completion logic

## Critical Issue: Test Counting Anomaly

### **Core Functionality Test Counting Problem**
- **Symptom**: Reports "9 passed, 0 failed" from 10 total tests
- **Analysis**: One test is not completing or being counted properly
- **Likely Cause**: Test 10 (Command execution --exec) may be hanging or timing out
- **Impact**: Test framework may not be properly handling incomplete tests
- **Priority**: HIGH - This affects test reliability and reporting accuracy

### **Investigation Needed**
1. **Test Framework Logic**: Check how incomplete tests are handled
2. **Test 10 Specifically**: Command execution (--exec) appears to be the problem
3. **Timeout Handling**: Tests may be timing out without being marked as failed
4. **Test Completion**: Ensure all tests complete their execution path

## Identified Failing Tests

Based on previous test runs, the following tests are failing:

### **Expanded Test Suite Failures (4 tests)**

#### 1. **Test 5: HOME environment variable fallback**
- **Status**: ❌ FAIL
- **Issue**: HOME should be used as fallback lock directory
- **Category**: Environment Variable Handling
- **Priority**: Medium
- **Description**: The lock directory fallback logic may not be properly checking HOME directory

#### 2. **Test 6: Internal test suite (--test)**
- **Status**: ❌ FAIL  
- **Issue**: Internal test suite should pass
- **Category**: Internal Testing
- **Priority**: Low
- **Description**: The built-in test suite may have issues or dependencies

#### 3. **Test 17: Stale lock detection**
- **Status**: ❌ FAIL
- **Issue**: Stale lock should be detected
- **Category**: Lock Management
- **Priority**: High
- **Description**: Stale lock cleanup logic may not be working correctly

#### 4. **Test 25: Command execution with lock**
- **Status**: ❌ FAIL (timeout)
- **Issue**: Exec mode may be hanging or taking too long
- **Category**: Command Execution
- **Priority**: High
- **Description**: --exec functionality may have timeout or execution issues

### **Comprehensive Test Failures (2 tests)**

#### 5. **Test 2: Check non-existent lock**
- **Status**: ❌ FAIL
- **Issue**: Check should fail for non-existent lock
- **Category**: Lock Checking
- **Priority**: Medium
- **Description**: --check command may be returning wrong exit code

#### 6. **Test 6: Exec mode**
- **Status**: ❌ FAIL (timeout)
- **Issue**: Exec mode hanging or timing out
- **Category**: Command Execution
- **Priority**: High
- **Description**: --exec functionality appears to be problematic

## Failure Categories Analysis

### **Category 1: Command Execution (--exec) - HIGH PRIORITY**
- **Affected Tests**: Test 25 (Expanded), Test 6 (Comprehensive)
- **Impact**: 2 failures
- **Symptoms**: Timeouts, hanging behavior
- **Root Cause**: Likely issues with process management, signal handling, or cleanup in exec mode

### **Category 2: Lock Management - HIGH PRIORITY**
- **Affected Tests**: Test 17 (Stale lock detection)
- **Impact**: 1 failure
- **Symptoms**: Stale locks not being properly detected/cleaned
- **Root Cause**: Process existence checking or stale lock cleanup logic

### **Category 3: Environment/Configuration - MEDIUM PRIORITY**
- **Affected Tests**: Test 5 (HOME fallback), Test 2 (Check command)
- **Impact**: 2 failures
- **Symptoms**: Incorrect directory resolution, wrong exit codes
- **Root Cause**: Environment variable handling or command logic

### **Category 4: Internal Testing - LOW PRIORITY**
- **Affected Tests**: Test 6 (Internal test suite)
- **Impact**: 1 failure
- **Symptoms**: Internal --test flag not working
- **Root Cause**: Built-in test implementation issues

## Investigation Strategy

### **Phase 1: Critical Priority Fixes**

#### 1.1 **Test Framework Counting Issue** - CRITICAL
- **Goal**: Fix test counting anomaly (9 passed, 0 failed from 10 tests)
- **Approach**:
  - Investigate test framework completion logic
  - Check how timeouts and incomplete tests are handled
  - Fix test reporting accuracy
  - Ensure all tests are properly counted

#### 1.2 **Exec Mode Investigation** - HIGH
- **Goal**: Fix hanging/timeout issues in --exec mode (likely causing counting issue)
- **Approach**:
  - Run isolated --exec tests with debug logging
  - Check signal handling in exec mode
  - Verify process cleanup and exit code handling
  - Test with simple commands (echo, sleep, etc.)
  - Fix timeout handling in test framework

#### 1.3 **Stale Lock Detection** - HIGH
- **Goal**: Ensure stale locks are properly detected and cleaned
- **Approach**:
  - Test process existence checking logic
  - Verify lock file cleanup mechanisms
  - Check timing of stale lock detection
  - Test with killed processes vs naturally exited processes

### **Phase 2: Medium Priority Fixes**

#### 2.1 **HOME Directory Fallback**
- **Goal**: Fix lock directory resolution when HOME is used
- **Approach**:
  - Test lock directory search order
  - Verify HOME/.waitlock creation and permissions
  - Check environment variable precedence

#### 2.2 **Check Command Logic**
- **Goal**: Fix --check command exit codes
- **Approach**:
  - Test --check with existing vs non-existing locks
  - Verify exit code correctness (0 for available, 1 for busy)
  - Check lock availability detection logic

### **Phase 3: Low Priority Fixes**

#### 3.1 **Internal Test Suite**
- **Goal**: Fix --test flag functionality
- **Approach**:
  - Review built-in test implementation
  - Fix any dependency or logic issues
  - Ensure test framework works correctly

## Implementation Plan

### **Step 1: Detailed Failure Analysis**
1. Run each failing test individually with maximum debugging
2. Capture exact error messages and failure points
3. Identify root cause for each failure category
4. Create minimal reproduction cases

### **Step 2: Prioritized Fixes**
1. **Fix exec mode issues** (affects 2 tests)
2. **Fix stale lock detection** (affects 1 test)
3. **Fix environment handling** (affects 2 tests)
4. **Fix internal test suite** (affects 1 test)

### **Step 3: Verification**
1. Test each fix in isolation
2. Run full test suite to ensure no regressions
3. Update test pass rate metrics
4. Document fixes and improvements

## Success Criteria

### **Target Pass Rates**
- **Phase 1 Complete**: 90%+ pass rate (fix exec and stale lock issues)
- **Phase 2 Complete**: 95%+ pass rate (fix environment issues)
- **Phase 3 Complete**: 98%+ pass rate (fix internal tests)

### **Quality Gates**
- No new test failures introduced
- All existing passing tests continue to pass
- Fixes are minimal and targeted (no over-engineering)
- Comprehensive regression testing

## Risk Assessment

### **Low Risk Fixes**
- Internal test suite (isolated functionality)
- Environment variable handling (well-scoped)

### **Medium Risk Fixes**
- Check command logic (affects core functionality)
- HOME directory fallback (affects lock directory resolution)

### **High Risk Fixes**
- Exec mode (affects process management and signal handling)
- Stale lock detection (affects lock cleanup mechanisms)

## Next Steps

1. **Create detailed investigation plan** for each failing test
2. **Set up isolated test environment** for debugging
3. **Implement fixes in order of priority** (exec mode first)
4. **Run comprehensive regression testing** after each fix
5. **Update documentation** with findings and solutions

## Notes

- The significant improvement from 25% to 86% pass rate shows the infrastructure is solid
- Remaining failures appear to be specific implementation issues rather than architectural problems
- Most failures are in edge cases or advanced features, not core functionality
- The failing tests represent important functionality that should be fixed for production readiness

This blueprint provides a systematic approach to addressing the remaining test failures while maintaining the stability achieved through the hang fix.