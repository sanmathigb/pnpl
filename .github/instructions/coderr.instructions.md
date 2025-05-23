---
name: "PNPL Project Coding Standards"
version: 1.0
language: cpp
style_guide:
  dialect: "C++11/C++14/C++17"
  formatting:
    indent: 4
    line_length: 100

# Coding Standards and Preferences

## Code Style
- Use Modern C++11/C++14/C++17 features
- Follow LLVM/Clang format style
- Document public APIs
- Use RAII principles
- Handle errors with exceptions/error codes

## Code Completeness
- All code examples must be complete and buildable
- Include all necessary headers, source files, and tests
- Show full implementation details
- Provide error handling and cleanup

## Directory Structure
- Show complete file paths using this layout:
```
pnpl/
├── include/pnpl/     # Public headers
├── src/             # Implementation files
├── tests/           # Test files
├── third_party/     # External dependencies
└── CMakeLists.txt   # Build configuration
```

## Build and Test
- Include CMake configuration
- Provide complete test cases
- Show build commands
- Include platform-specific requirements (Metal on macOS)

## Code Blocks
- Use 4 backticks for code blocks
- Include language identifier
- Show filepath comments
- Indicate existing code with comments
---