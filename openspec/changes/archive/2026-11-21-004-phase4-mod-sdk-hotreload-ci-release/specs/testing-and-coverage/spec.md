# Spec Delta

## ADDED Requirements

### Requirement: Cross-Platform Continuous Integration Matrix
The repository SHALL include an automated GitHub Actions CI workflow verifying Linux GCC builds with >= 95% C11 and Lua line coverage, sanitizer checks (ASan/UBSan), headless automated game tests, offscreen OpenGL screenshot verification (`gl_errors == 0`), Windows MSVC, macOS Clang, and Emscripten Web/WebXR builds.

#### Scenario: CI enforces coverage and cross-platform build integrity
- **GIVEN** a push or pull request to `main`
- **WHEN** `.github/workflows/ci.yml` executes
- **THEN** all C/Lua unit tests, >= 95% coverage gates, automated game tests, OpenGL smoke tests, and multi-platform builds are verified automatically
