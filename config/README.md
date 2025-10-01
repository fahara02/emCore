# Task Configuration

emCore supports automatic task generation from YAML configuration files.

## Usage

### Option 1: Use Default Tasks

By default, emCore will use `config/default_tasks.yaml`:

```cpp
#include <emCore/task/generated_tasks.hpp>
#include <emCore/task/taskmaster.hpp>

void setup() {
    auto& tm = taskmaster::instance();
    tm.initialize();
    
    // Load tasks from generated configuration
    tm.create_all_tasks(task_table);
}
```

### Option 2: Use Custom Tasks

1. Create your own `tasks.yaml` file:

```yaml
tasks:
  - name: "my_task"
    function: "my_task_function"
    priority: "high"
    period_ms: 100
    enabled: true
    description: "My custom task"
```

2. Define the path before building:

**CMake:**
```cmake
set(EMCORE_USER_TASKS_YAML "${CMAKE_SOURCE_DIR}/my_tasks.yaml")
```

**Or via command line:**
```bash
cmake -DEMCORE_USER_TASKS_YAML=/path/to/tasks.yaml ..
```

3. Generate the configuration:

```bash
python scripts/generate_tasks.py my_tasks.yaml src/emCore/task/generated_tasks.hpp
```

## YAML Format

```yaml
tasks:
  - name: "task_name"           # Task identifier (max 32 chars)
    function: "function_name"   # C++ function name
    priority: "normal"          # idle, low, normal, high, critical
    period_ms: 1000            # 0 = run once, >0 = periodic
    enabled: true              # true/false
    description: "..."         # Optional description
```

## Task Function Signature

All task functions must have this signature:

```cpp
void task_function_name(void* params) noexcept {
    // Task implementation
}
```

## Priority Levels

- **idle** - Lowest priority, runs when nothing else is ready
- **low** - Low priority background tasks
- **normal** - Default priority
- **high** - High priority tasks
- **critical** - Highest priority, time-critical tasks

## Example

See `config/default_tasks.yaml` for a complete example.
