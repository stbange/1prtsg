# testing-and-coverage Specification

## Purpose
Enforces high software reliability through >= 95% C and Lua unit test coverage gates, deterministic headless automated game tests, and offscreen OpenGL visual smoke tests.

## Requirements

### Requirement: Minimum 95 Percent Unit Test Coverage
The build and verification pipeline MUST execute C and Lua unit test suites and fail if measured line coverage for core C modules or gameplay Lua scripts falls below 95 percent.

#### Scenario: Enforcing C and Lua coverage thresholds
- **GIVEN** an instrumented coverage build and complete C and Lua unit test suites
- **WHEN** the coverage verification scripts execute
- **THEN** both C core coverage and Lua script coverage report >= 95.0 percent line coverage and exit with status 0

### Requirement: Automated End-to-End Game Tests
The engine MUST provide a headless deterministic game test runner executing end-to-end Lua scenarios plus an offscreen OpenGL smoke/screenshot check.

#### Scenario: Running headless and offscreen OpenGL game tests
- **GIVEN** the compiled `1prtsg` binary and external `assets/`, `scripts/`, and `mods/` folders
- **WHEN** headless game scenarios and offscreen OpenGL screenshot verification run
- **THEN** all gameplay assertions pass deterministically and OpenGL renders valid PNG screenshots with zero GL errors

### Requirement: Cross-Platform Continuous Integration Matrix
The repository SHALL include an automated GitHub Actions CI workflow verifying Linux GCC builds with >= 95% C11 and Lua line coverage, sanitizer checks (ASan/UBSan), headless automated game tests, offscreen OpenGL screenshot verification (`gl_errors == 0`), Windows MSVC, macOS Clang, and Emscripten Web/WebXR builds.

#### Scenario: CI enforces coverage and cross-platform build integrity
- **GIVEN** a push or pull request to `main`
- **WHEN** `.github/workflows/ci.yml` executes
- **THEN** all C/Lua unit tests, >= 95% coverage gates, automated game tests, OpenGL smoke tests, and multi-platform builds are verified automatically
