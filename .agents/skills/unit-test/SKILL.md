---
name: unit-test
description: Comprehensive unit testing for codebases across major languages and frameworks.
---

# Unit Testing Skill

Comprehensive unit testing for codebases across all major languages and frameworks.

## Trigger

User executes `/unit-test` command or asks to add/improve unit tests.

## Purpose

Create thorough, maintainable unit tests that:
- Verify correctness of individual units (functions, methods, classes)
- Catch regressions before they reach production
- Document expected behavior through executable specifications
- Enable confident refactoring
- Improve code design (testable code is usually better designed)

## Workflow

### Phase 1: Analysis

1. **Identify Testing Framework:**
   ```
   Language        | Primary Framework      | Alternatives
   ----------------|------------------------|------------------
   C++             | GoogleTest (gtest)     | Catch2, doctest
   JavaScript      | Jest                   | Vitest, Mocha
   TypeScript      | Jest + ts-jest         | Vitest
   Python          | pytest                 | unittest
   Go              | testing (built-in)     | testify
   Rust            | cargo test (built-in)  |
   Swift           | XCTest                 | Quick/Nimble
   Java            | JUnit 5                | TestNG
   C#              | xUnit                  | NUnit, MSTest
   ```

2. **Scan Existing Tests:**
   - Find test directory structure
   - Identify naming conventions
   - Check test configuration files
   - Note any test utilities/helpers

3. **Identify Untested Code:**
   - Find functions/methods without tests
   - Look for complex logic paths
   - Identify public APIs
   - Find edge case opportunities

### Phase 2: Test Design

#### Test Structure (AAA Pattern)

```
Arrange  → Set up test data and conditions
Act      → Execute the code under test
Assert   → Verify the expected outcome
```

#### Naming Convention

```
Format: test_<unit>_<scenario>_<expected_result>
        <Unit>_<Scenario>_<ExpectedResult>
        should_<expected>_when_<condition>

Examples:
- test_calculate_tax_with_zero_income_returns_zero
- UserService_CreateUser_WithValidData_ReturnsUser
- should_throw_exception_when_input_is_null
```

#### Test Categories

1. **Happy Path Tests** - Normal expected usage
2. **Edge Case Tests** - Boundary conditions
3. **Error Case Tests** - Invalid inputs, failures
4. **Integration Points** - Mocked dependencies

### Phase 3: Implementation

#### Test Patterns by Scenario

**1. Pure Functions (No Dependencies)**
```cpp
// C++ with GoogleTest
TEST(Calculator, Add_TwoPositiveNumbers_ReturnsSum) {
    Calculator calc;
    EXPECT_EQ(calc.Add(2, 3), 5);
}

TEST(Calculator, Add_NegativeNumbers_ReturnsCorrectSum) {
    Calculator calc;
    EXPECT_EQ(calc.Add(-2, -3), -5);
}

TEST(Calculator, Divide_ByZero_ThrowsException) {
    Calculator calc;
    EXPECT_THROW(calc.Divide(10, 0), std::invalid_argument);
}
```

**2. Classes with Dependencies (Use Mocks)**
```cpp
// C++ with GoogleTest/GMock
class MockDatabase : public IDatabase {
public:
    MOCK_METHOD(User, GetUser, (int id), (override));
    MOCK_METHOD(bool, SaveUser, (const User& user), (override));
};

TEST_F(UserServiceTest, GetUser_ValidId_ReturnsUser) {
    MockDatabase mockDb;
    User expectedUser{1, "John"};
    EXPECT_CALL(mockDb, GetUser(1)).WillOnce(Return(expectedUser));

    UserService service(&mockDb);
    auto result = service.GetUser(1);

    EXPECT_EQ(result.name, "John");
}
```

**3. Async/Callback Code**
```cpp
// C++ async testing
TEST_F(AsyncServiceTest, FetchData_ReturnsDataViaCallback) {
    std::promise<std::string> promise;
    auto future = promise.get_future();

    service.FetchData([&promise](const std::string& data) {
        promise.set_value(data);
    });

    EXPECT_EQ(future.wait_for(std::chrono::seconds(5)),
              std::future_status::ready);
    EXPECT_EQ(future.get(), "expected_data");
}
```

**4. State Machine / Stateful Objects**
```cpp
TEST_F(StateMachineTest, Transition_FromIdleToRunning_OnStart) {
    StateMachine sm;
    EXPECT_EQ(sm.GetState(), State::Idle);

    sm.Start();
    EXPECT_EQ(sm.GetState(), State::Running);
}

TEST_F(StateMachineTest, Transition_InvalidTransition_ThrowsError) {
    StateMachine sm;
    sm.Start();
    EXPECT_THROW(sm.Start(), InvalidTransitionError);
}
```

### Phase 4: Edge Cases Checklist

Always test these conditions:

#### Numeric Inputs
- [ ] Zero
- [ ] Negative numbers
- [ ] Maximum value (INT_MAX, etc.)
- [ ] Minimum value (INT_MIN, etc.)
- [ ] Overflow conditions
- [ ] Floating point precision (0.1 + 0.2 != 0.3)

#### String Inputs
- [ ] Empty string ""
- [ ] Single character
- [ ] Very long strings
- [ ] Unicode characters
- [ ] Special characters (quotes, backslashes, newlines)
- [ ] Whitespace only
- [ ] Leading/trailing whitespace

#### Collections
- [ ] Empty collection
- [ ] Single element
- [ ] Large collections
- [ ] Null/nullptr elements
- [ ] Duplicate elements
- [ ] Sorted vs unsorted

#### Pointers/References
- [ ] Null/nullptr
- [ ] Dangling references
- [ ] Self-reference

#### Time/Date
- [ ] Epoch (1970-01-01)
- [ ] Leap years
- [ ] Daylight saving transitions
- [ ] Timezone boundaries
- [ ] Far future dates (Y2K38 problem)

#### Files/IO
- [ ] File not found
- [ ] Permission denied
- [ ] Disk full
- [ ] Concurrent access
- [ ] Invalid path characters

### Phase 5: Test Quality Verification

1. **Run All Tests:**
   ```bash
   # Detect and run appropriate test command
   cmake --build build --target <TestTarget> && ./build/<TestTarget>
   npm test
   pytest -v
   go test ./...
   cargo test
   ```

2. **Verify No Flaky Tests:**
   - Run tests multiple times
   - Check for time-dependent failures
   - Ensure proper test isolation

3. **Check Test Independence:**
   - Tests should pass in any order
   - No shared mutable state between tests
   - Each test sets up its own fixtures

4. **Verify Assertions:**
   - Tests should fail for the right reasons
   - Temporarily break code to verify test catches it

### Phase 6: Coverage Analysis

1. **Generate Coverage Report:**
   ```bash
   # C++ with gcov/lcov
   cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="--coverage" ..
   make && ./tests
   lcov --capture --directory . --output-file coverage.info

   # JavaScript with Jest
   npm test -- --coverage

   # Python with pytest-cov
   pytest --cov=src --cov-report=html
   ```

2. **Coverage Targets:**
   - Critical business logic: 90%+
   - Utility functions: 80%+
   - UI code: 60%+ (harder to test)
   - Overall: 70%+ minimum

## Test File Organization

```
project/
├── src/
│   ├── calculator.cpp
│   └── user_service.cpp
├── tests/
│   ├── calculator_test.cpp      # Unit tests
│   ├── user_service_test.cpp
│   ├── integration/             # Integration tests
│   │   └── api_integration_test.cpp
│   ├── fixtures/                # Test data
│   │   └── sample_users.json
│   └── mocks/                   # Mock implementations
│       └── mock_database.h
└── CMakeLists.txt               # Includes test configuration
```

## Common Pitfalls to Avoid

1. **Testing Implementation, Not Behavior**
   - BAD: Test that internal method was called
   - GOOD: Test that output is correct

2. **Over-Mocking**
   - BAD: Mock everything, test nothing
   - GOOD: Mock external dependencies only

3. **Fragile Tests**
   - BAD: Tests that break with unrelated changes
   - GOOD: Test public interfaces, not internals

4. **Test Interdependence**
   - BAD: Test B fails if Test A doesn't run first
   - GOOD: Each test is completely isolated

5. **Ignoring Test Maintenance**
   - BAD: Commented out tests, skipped tests
   - GOOD: Delete or fix broken tests immediately

## Output

After running `/unit-test`:

```markdown
# Unit Test Report

## Summary
- **Tests Added**: X new tests
- **Tests Modified**: Y tests
- **Total Tests**: Z tests
- **All Passing**: YES/NO

## New Tests
| File | Test Name | Coverage |
|------|-----------|----------|
| calculator_test.cpp | Add_EdgeCases | 5 cases |
| ... | ... | ... |

## Test Execution
```
[==========] Running X tests from Y test suites.
[  PASSED  ] X tests.
```

## Coverage Impact
- Before: XX%
- After: YY%
- Delta: +Z%

## Recommendations
1. Additional tests needed for [component]
2. Consider adding integration tests for [feature]
```

## References

See [references/testing-patterns.md](references/testing-patterns.md) for language-specific patterns.
See [references/mocking-guide.md](references/mocking-guide.md) for mock/stub strategies.
