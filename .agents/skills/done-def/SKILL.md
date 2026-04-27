---
name: done-def
description: Production readiness review for features, components, and applications.
---

# Definition of Done Skill

Production readiness review for features, components, and applications.

## Trigger

User executes `/done-def` command.

## Purpose

Systematically verify that code changes meet production-quality standards before deployment. This skill applies software engineering best practices to ensure reliability, maintainability, security, and performance.

## Review Process

### Phase 1: Code Quality Review

1. **Read all modified/new files** in the current session
2. **Check coding standards:**
   - Consistent naming conventions (camelCase, snake_case, PascalCase as appropriate)
   - Proper indentation and formatting
   - No commented-out code (except intentional TODOs)
   - No debug statements (console.log, print, NSLog in production paths)
   - No hardcoded secrets, API keys, or credentials
   - Functions/methods are reasonably sized (< 50 lines preferred)
   - Single responsibility principle followed

3. **Memory & Resource Management:**
   - No memory leaks (RAII, smart pointers, proper cleanup)
   - Resources properly released (file handles, network connections, DB connections)
   - No dangling pointers or references
   - Proper ownership semantics

4. **Error Handling:**
   - All error paths handled (no silent failures)
   - Appropriate error messages (informative, not exposing internals)
   - Graceful degradation where appropriate
   - No unhandled exceptions in critical paths

### Phase 2: Testing Verification

1. **Unit Tests:**
   - New code has corresponding tests
   - Tests cover happy path AND edge cases
   - Tests are deterministic (no flaky tests)
   - Test names clearly describe what they test
   - Mocks/stubs used appropriately

2. **Run Tests:**
   ```bash
   # Find and run test command for the project
   # Common patterns: npm test, pytest, cargo test, go test, make test
   ```

3. **Test Coverage Check:**
   - Critical paths have test coverage
   - Edge cases covered (null, empty, boundary values)
   - Error conditions tested

### Phase 3: Security Review

1. **Input Validation:**
   - All external inputs validated
   - SQL injection prevention (parameterized queries)
   - XSS prevention (output encoding)
   - Path traversal prevention
   - Command injection prevention

2. **Authentication & Authorization:**
   - Auth checks in place where needed
   - Principle of least privilege followed
   - Sensitive data not logged

3. **Data Protection:**
   - Sensitive data encrypted at rest/in transit
   - No PII in logs
   - Secure defaults

### Phase 4: Performance Review

1. **Algorithmic Efficiency:**
   - No O(n²) or worse in hot paths
   - Appropriate data structures used
   - No unnecessary allocations in loops

2. **Resource Usage:**
   - Database queries optimized (no N+1 queries)
   - Proper caching where beneficial
   - No blocking operations on main/UI thread

3. **Memory Efficiency:**
   - No unbounded growth
   - Large data processed in chunks/streams
   - Proper cleanup after use

### Phase 5: Documentation & Maintainability

1. **Code Documentation:**
   - Public APIs documented
   - Complex logic has explanatory comments
   - README updated if needed
   - CHANGELOG updated

2. **Architecture:**
   - Changes follow existing patterns
   - No circular dependencies introduced
   - Clear separation of concerns

### Phase 6: Build & Deploy

1. **Build Verification:**
   - Code compiles without warnings (treat warnings as errors)
   - No deprecated API usage
   - Dependencies up to date (no known vulnerabilities)

2. **Configuration:**
   - Environment-specific configs externalized
   - Feature flags for risky changes
   - Rollback plan exists

## Output Format

Generate a comprehensive report:

```markdown
# Definition of Done Review

## Summary
- **Status**: PASS / PASS WITH NOTES / FAIL
- **Risk Level**: Low / Medium / High
- **Reviewed**: [list of files/components]

## Code Quality
- [ ] Coding standards followed
- [ ] No debug code in production paths
- [ ] No hardcoded secrets
- [ ] Error handling complete

## Testing
- [ ] Unit tests exist and pass
- [ ] Edge cases covered
- [ ] Test count: X tests, Y passing

## Security
- [ ] Input validation in place
- [ ] No injection vulnerabilities
- [ ] Auth/authz appropriate

## Performance
- [ ] No algorithmic issues
- [ ] No blocking operations on main thread
- [ ] Memory management correct

## Documentation
- [ ] Code documented
- [ ] CHANGELOG updated
- [ ] README current

## Issues Found
1. [CRITICAL/HIGH/MEDIUM/LOW] Description and location
2. ...

## Recommendations
1. ...

## Final Verdict
[READY FOR PRODUCTION / NEEDS WORK]
```

## Checklist by Language

### C/C++
- [ ] No raw new/delete (use smart pointers)
- [ ] RAII for all resources
- [ ] No buffer overflows
- [ ] Proper const correctness
- [ ] No undefined behavior

### JavaScript/TypeScript
- [ ] No any types (TypeScript)
- [ ] Proper async/await handling
- [ ] No callback hell
- [ ] Dependencies audited

### Python
- [ ] Type hints where appropriate
- [ ] No mutable default arguments
- [ ] Context managers for resources
- [ ] Virtual environment documented

### Swift/Objective-C
- [ ] No force unwraps in production
- [ ] Proper memory management (ARC aware)
- [ ] Main thread for UI operations

## Severity Definitions

- **CRITICAL**: Must fix before merge. Security vulnerability, data loss risk, crash.
- **HIGH**: Should fix before merge. Significant bugs, performance issues.
- **MEDIUM**: Fix soon. Code quality issues, missing tests.
- **LOW**: Nice to have. Style issues, minor improvements.

## Notes

- This review complements but does not replace peer code review
- Focus on the changes in the current session/PR
- Be thorough but pragmatic - perfect is the enemy of good
- Document any accepted technical debt with clear TODOs
