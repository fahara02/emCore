#!/usr/bin/env python3
"""
Generate C++ task configuration from YAML file
Usage: python generate_tasks.py [input.yaml] [output.hpp]
"""

import sys
from pathlib import Path

try:
    import yaml
except ImportError:
    print("ERROR: PyYAML not installed. Install with: pip install pyyaml")
    sys.exit(1)

def priority_to_cpp(priority_str):
    """Convert priority string to C++ enum"""
    priority_map = {
        'idle': 'priority::idle',
        'low': 'priority::low',
        'normal': 'priority::normal',
        'high': 'priority::high',
        'critical': 'priority::critical'
    }
    return priority_map.get(priority_str.lower(), 'priority::normal')

def generate_task_config_header(yaml_file, output_file):
    """Generate C++ header from YAML task configuration"""
    
    # Load YAML
    with open(yaml_file, 'r') as f:
        config = yaml.safe_load(f)
    
    tasks = config.get('tasks', [])
    
    # Load template from templates folder
    template_file = Path(__file__).parent.parent / "templates" / "task_config.htf"
    with open(template_file, 'r') as f:
        template = f.read()
    
    # Generate forward declarations
    forward_decls = []
    for task in tasks:
        func_name = task.get('TaskEntryPtr', task.get('function', ''))
        forward_decls.append(f"void {func_name}(void* params) noexcept;")
    
    forward_declarations = '\n'.join(forward_decls)
    
    # Generate task configurations
    task_configs = []
    for task in tasks:
        # Support both old and new YAML formats
        name = task.get('TaskName', task.get('name', ''))
        func = task.get('TaskEntryPtr', task.get('function', ''))
        period = task.get('PeriodicityInMS', task.get('period_ms', 0))
        priority_val = task.get('TaskPriority', task.get('priority', 'normal'))
        
        # Convert priority to C++ enum
        if isinstance(priority_val, int):
            if priority_val == 0:
                priority = 'priority::idle'
            elif priority_val <= 5:
                priority = 'priority::low'
            elif priority_val <= 15:
                priority = 'priority::normal'
            elif priority_val <= 20:
                priority = 'priority::high'
            else:
                priority = 'priority::critical'
        else:
            priority = priority_to_cpp(str(priority_val))
        
        enabled = 'true' if task.get('enabled', True) else 'false'
        desc = task.get('description', name)
        stack_size = task.get('StackSize', 4096)
        create_native = 'true' if task.get('CreateNative', False) else 'false'
        
        task_config = f"""    /* {desc} */
    task_config(
        &{func},           /* function */
        "{name}",          /* name */
        {priority},        /* priority */
        {period},          /* period_ms */
        nullptr,           /* parameters */
        {enabled},         /* enabled */
        {stack_size},      /* stack_size */
        {priority_val},    /* rtos_priority */
        {create_native}    /* create_native */
    )"""
        
        task_configs.append(task_config)
    
    task_configurations = ',\n'.join(task_configs)
    
    # Fill template
    header = template.format(
        filename=Path(output_file).name,
        yaml_source=Path(yaml_file).name,
        forward_declarations=forward_declarations,
        task_configurations=task_configurations
    )
    
    # Write output
    with open(output_file, 'w') as f:
        f.write(header)
    
    print(f"Generated {output_file} with {len(tasks)} tasks")

def main():
    if len(sys.argv) < 2:
        print("Usage: python generate_tasks.py [input.yaml] [output.hpp]")
        print("Using default: config/default_tasks.yaml -> src/emCore/task/generated_tasks.hpp")
        yaml_file = Path(__file__).parent.parent / "config" / "default_tasks.yaml"
        output_file = Path(__file__).parent.parent / "src" / "emCore" / "task" / "generated_tasks.hpp"
    else:
        yaml_file = Path(sys.argv[1])
        output_file = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("generated_tasks.hpp")
    
    if not yaml_file.exists():
        print(f"Error: {yaml_file} not found")
        sys.exit(1)
    
    generate_task_config_header(yaml_file, output_file)

if __name__ == "__main__":
    main()
