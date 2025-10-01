# emCore - Embedded C++ Core Library

Header-only, MCU agnostic C++ library with no RTTI and no dynamic allocation.

## Features

- **Header-only**: No compilation required, just include and use
- **No RTTI/Exceptions**: Optimized for embedded systems
- **No Dynamic Allocation**: All containers use fixed-size storage
- **MCU Agnostic**: Works on any platform (ESP32, Arduino, ARM, etc.)
- **C++17**: Uses modern C++ features with `gnu++17` standard
- **ETL Dependency**: Built on top of the Embedded Template Library

## Project Structure

```
emCore/
├── src/
│   └── emCore/
│       ├── emCore.hpp          # Main library header
│       ├── core/
│       │   ├── types.hpp       # Type definitions
│       │   └── config.hpp      # Configuration
│       ├── task/
│       │   └── taskmaster.hpp  # Task management
│       ├── memory/
│       │   └── pool.hpp        # Memory pool allocator
│       ├── event/
│       │   └── dispatcher.hpp  # Event system
│       └── utils/
│           └── helpers.hpp     # Utility functions
├── external/
│   └── etl/                    # Embedded Template Library (submodule)
├── test/
│   └── test_main.cpp           # Unit tests
└── CMakeLists.txt              # CMake build configuration
```

## Getting Started

### Prerequisites

- C++17 compatible compiler
- CMake 3.16+ (for building)
- Ninja build system (recommended)
- Git (for cloning ETL submodule)

### Setup

1. **Clone with submodules**:
   ```bash
   git clone --recursive <your-repo-url>
   ```

2. **Or initialize submodules after cloning**:
   ```bash
   git submodule update --init --recursive
   ```

### Usage

Simply include the main header in your code:

```cpp
#include <emCore/emCore.hpp>

int main() {
    emCore::initialize();
    
    // Use library features
    emCore::taskmaster tm;
    tm.initialize();
    
    return 0;
}
```

### Building with CMake

```bash
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja
```

### Testing with PlatformIO (ESP32)

```bash
pio test -e esp32dev_test
```

## Configuration

Key configuration options in `src/emCore/emCore.hpp`:

- `EMCORE_MAX_TASKS` - Maximum number of tasks (default: 16)
- `EMCORE_MAX_EVENTS` - Maximum number of events (default: 32)
- `EMCORE_DEBUG` - Enable debug features

## IDE Support

The project includes configuration for:
- **clangd**: `.clangd` configuration file
- **VS Code**: `.vscode/settings.json` and `c_cpp_properties.json`

Clangd will provide IntelliSense, linting, and code completion.

## License

See LICENSE file for details.

## Dependencies

- [ETL (Embedded Template Library)](https://github.com/ETLCPP/etl) - Header-only C++ library for embedded systems
