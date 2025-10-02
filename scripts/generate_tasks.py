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

def generate_message_types(messages):
    """Generate C++ message type definitions"""
    if not messages:
        return ""
    
    message_code = []
    message_code.append("// Auto-generated message types")
    
    for msg in messages:
        name = msg.get('name', '')
        desc = msg.get('description', name)
        fields = msg.get('fields', [])
        
        message_code.append(f"""
/**
 * @brief {desc}
 */
struct {name} {{""")
        
        for field in fields:
            field_name = field.get('name', '')
            field_type = field.get('type', 'u32')
            message_code.append(f"    {field_type} {field_name};")
        
        message_code.append("};")
        # Ensure payload fits into medium_message by default
        message_code.append(f"static_assert(sizeof({name}) <= emCore::messaging::medium_payload_size, \"Payload too large for medium_message\");")
    
    return '\n'.join(message_code)

def generate_communication_setup(channels, tasks):
    """Generate C++ communication setup code using YAML-aware broker system"""
    if not channels:
        return ""
    
    comm_code = []
    comm_code.append("// Auto-generated YAML-aware messaging system")
    comm_code.append("// Type-safe broker with YAML channels and message types")
    comm_code.append("")
    
    # Helper: deterministic 16-bit FNV-1a hash (stable across runs)
    def fnv1a16(text: str) -> int:
        h = 0x811C9DC5  # 32-bit offset basis
        for ch in text.encode('utf-8'):
            h ^= ch
            h = (h * 0x01000193) & 0xFFFFFFFF  # 32-bit FNV prime
        # Fold to 16-bit
        h ^= (h >> 16)
        return h & 0xFFFF

    # Generate topic ID mappings from channels using deterministic hash-based IDs
    comm_code.append("// Topic ID mappings from YAML channels")
    comm_code.append("// IDs generated from deterministic FNV-1a hash to prevent conflicts")
    comm_code.append("enum class yaml_topic : u16 {")
    
    used_ids = set()
    for channel in channels:
        name = channel.get('name', '')
        # Deterministic hash with range 0x1000-0xEFFF to avoid system reserved ranges
        hash_val = fnv1a16(name)
        topic_id = 0x1000 + (hash_val % 0xE000)  # Range: 0x1000-0xEFFF
        # Handle hash collisions by incrementing
        while topic_id in used_ids:
            topic_id = 0x1000 + ((topic_id - 0x1000 + 1) % 0xE000)
        used_ids.add(topic_id)
        comm_code.append(f"    {name.replace('_channel', '')} = 0x{topic_id:04X},  // {name} (hash-based)")
    comm_code.append("};")
    comm_code.append("")
    
    # Generate helper functions for YAML message packing/unpacking - Library heavy lifting
    comm_code.append("// Helper functions for YAML message types - Library heavy lifting")
    for channel in channels:
        name = channel.get('name', '')
        msg_type = channel.get('message_type', '')
        topic_name = name.replace('_channel', '')
        
        # Pack function
        comm_code.append(f"// Pack {msg_type} into medium_message for {name}")
        comm_code.append(f"inline void pack_{topic_name}_message(const {msg_type}& data, emCore::messaging::medium_message& msg) noexcept {{")
        comm_code.append(f"    msg.header.type = static_cast<emCore::u16>(yaml_topic::{topic_name});")
        comm_code.append(f"    msg.header.payload_size = sizeof({msg_type});")
        comm_code.append(f"    msg.header.priority = static_cast<emCore::u8>(emCore::messaging::message_priority::normal);")
        comm_code.append(f"    msg.header.flags = static_cast<emCore::u8>(emCore::messaging::message_flags::none);")
        comm_code.append(f"    msg.header.timestamp = emCore::platform::get_system_time_us();")
        comm_code.append(f"    etl::copy_n(reinterpret_cast<const uint8_t*>(&data), sizeof({msg_type}), msg.payload);")
        comm_code.append(f"}}")
        comm_code.append("")
        
        # Unpack function  
        comm_code.append(f"// Unpack {msg_type} from medium_message for {name}")
        comm_code.append(f"inline bool unpack_{topic_name}_message(const emCore::messaging::medium_message& msg, {msg_type}& data) noexcept {{")
        comm_code.append(f"    if (msg.header.type != static_cast<emCore::u16>(yaml_topic::{topic_name})) {{")
        comm_code.append(f"        return false;")
        comm_code.append(f"    }}")
        comm_code.append(f"    etl::copy_n(msg.payload, sizeof({msg_type}), reinterpret_cast<uint8_t*>(&data));")
        comm_code.append(f"    return true;")
        comm_code.append(f"}}")
        comm_code.append("")
    
    # Generate comprehensive setup function - Library heavy lifting
    comm_code.append("// Complete YAML-based task and communication setup - Library heavy lifting")
    comm_code.append("inline void setup_yaml_system(emCore::taskmaster& task_mgr) noexcept {")
    comm_code.append("    auto& watchdog = emCore::get_global_watchdog();")
    comm_code.append("    auto& profiler = emCore::diagnostics::get_global_profiler();")
    comm_code.append("    auto& health_monitor = emCore::diagnostics::get_global_health_monitor();")
    comm_code.append("    auto& scheduler = emCore::task::get_global_scheduler();")
    comm_code.append("")
    
    # Generate setup for each task with ALL attributes
    # Map channel -> queue_size and max_subscribers for configuration
    channel_queue_size = { (ch.get('name','')): ch.get('queue_size', 16) for ch in channels }
    channel_max_subs = { (ch.get('name','')): ch.get('max_subscribers', 8) for ch in channels }

    # Configure per-topic capacities once (before per-task subscriptions)
    comm_code.append("    // Configure per-topic subscriber capacities from YAML")
    for ch in channels:
        name = ch.get('name','')
        topic_name = name.replace('_channel','')
        max_subs = int(ch.get('max_subscribers', 8) or 8)
        comm_code.append(f"    task_mgr.set_topic_capacity(emCore::topic_id_t(static_cast<emCore::u16>(yaml_topic::{topic_name})), {max_subs});")

    for task in tasks:
        task_name = task.get('name', '')
        subscribes = task.get('subscribes_to', [])
        publishes = task.get('publishes_to', [])
        
        # Advanced attributes from YAML
        cpu_affinity = task.get('cpu_affinity', -1)
        watchdog_timeout = task.get('watchdog_timeout_ms', 10000)
        watchdog_action = task.get('watchdog_action', 'log_warning')
        max_execution = task.get('max_execution_us', 0)
        
        comm_code.append(f"    // Setup {task_name} with all YAML attributes")
        comm_code.append(f"    auto {task_name.lower()}_id = task_mgr.get_task_by_name(\"{task_name}\");")
        comm_code.append(f"    if ({task_name.lower()}_id.is_ok()) {{")
        comm_code.append(f"        auto task_id = {task_name.lower()}_id.value();")
        comm_code.append("")
        
        # Subscriptions
        if subscribes:
            comm_code.append(f"        // Subscriptions for {task_name}")
            for channel_name in subscribes:
                for channel in channels:
                    if channel.get('name', '') == channel_name:
                        topic_name = channel_name.replace('_channel', '')
                        comm_code.append(f"        emCore::taskmaster::subscribe(emCore::topic_id_t(static_cast<emCore::u16>(yaml_topic::{topic_name})), task_id);")
                        # Configure mailbox depth for this subscriber using channel queue_size
                        qsize = channel_queue_size.get(channel_name, 16)
                        comm_code.append(f"        task_mgr.set_mailbox_depth(task_id, {int(qsize)});")
                        break
        
        # Watchdog setup with YAML attributes
        watchdog_action_enum = {
            'none': 'emCore::watchdog_action::none',
            'log_warning': 'emCore::watchdog_action::log_warning', 
            'reset_task': 'emCore::watchdog_action::reset_task',
            'system_reset': 'emCore::watchdog_action::system_reset'
        }.get(watchdog_action, 'emCore::watchdog_action::log_warning')
        
        comm_code.append(f"        // Watchdog setup from YAML")
        comm_code.append(f"        watchdog.register_task(task_id, emCore::watchdog_timeout_ms({watchdog_timeout}), {watchdog_action_enum});")
        
        # Profiler and health monitoring
        comm_code.append(f"        profiler.register_task(task_id);")
        comm_code.append(f"        health_monitor.register_task(task_id);")
        
        # RTOS scheduling with YAML attributes
        if max_execution > 0 or cpu_affinity >= 0:
            comm_code.append(f"        // RTOS scheduling from YAML")
            comm_code.append(f"        emCore::task::task_execution_context ctx;")
            if max_execution > 0:
                comm_code.append(f"        ctx.max_execution_time_us = {max_execution};")
            if cpu_affinity >= 0:
                comm_code.append(f"        ctx.cpu_core_id = {cpu_affinity};")
                comm_code.append(f"        ctx.pin_to_core = true;")
            comm_code.append(f"        scheduler.register_task(task_id, ctx);")
        
        comm_code.append(f"    }}")
        comm_code.append("")
    
    comm_code.append("}")
    
    return '\n'.join(comm_code)

def generate_task_config_header(yaml_file, output_file):
    """Generate C++ header from YAML task configuration"""
    
    # Load YAML
    with open(yaml_file, 'r') as f:
        config = yaml.safe_load(f)
    
    tasks = config.get('tasks', [])
    messages = config.get('messages', [])
    channels = config.get('channels', [])
    
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
    
    # Generate message types and communication setup
    message_types = generate_message_types(messages)
    communication_setup = generate_communication_setup(channels, tasks)
    
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
        
        # Advanced configuration
        cpu_affinity = task.get('cpu_affinity', -1)  # -1 means no affinity
        watchdog_timeout = task.get('watchdog_timeout_ms', 0)
        watchdog_action = task.get('watchdog_action', 'none')
        max_execution = task.get('max_execution_us', 0)
        
        # Communication
        publishes_to = task.get('publishes_to', [])
        subscribes_to = task.get('subscribes_to', [])
        
        # Convert priority to numeric value for RTOS priority
        if isinstance(priority_val, int):
            rtos_priority_num = priority_val
        else:
            # Convert string priority to numeric RTOS priority
            priority_to_num = {
                'idle': 0,
                'low': 1,
                'normal': 5,
                'high': 10,
                'critical': 15
            }
            rtos_priority_num = priority_to_num.get(str(priority_val).lower(), 5)
        
        # Advanced attributes from YAML - use YAML values, minimal defaults
        cpu_affinity = task.get('cpu_affinity', -1)  # -1 = no affinity if not specified
        watchdog_timeout = task.get('watchdog_timeout_ms')  # Must be specified in YAML
        watchdog_action = task.get('watchdog_action')  # Must be specified in YAML  
        max_execution = task.get('max_execution_us', 0)  # 0 = no limit if not specified
        
        # Validate required YAML attributes
        if watchdog_timeout is None:
            raise ValueError(f"Task '{name}' missing required 'watchdog_timeout_ms' in YAML")
        if watchdog_action is None:
            raise ValueError(f"Task '{name}' missing required 'watchdog_action' in YAML")
        
        # Convert watchdog action to enum
        watchdog_action_enum = {
            'none': 'watchdog_action::none',
            'log_warning': 'watchdog_action::log_warning',
            'reset_task': 'watchdog_action::reset_task', 
            'system_reset': 'watchdog_action::system_reset'
        }.get(watchdog_action, 'watchdog_action::log_warning')
        
        task_config = f"""    /* {desc} */
    task_config(
        &{func},           /* function */
        "{name}",          /* name */
        {priority},        /* priority */
        {period},          /* period_ms */
        nullptr,           /* parameters */
        {enabled},         /* enabled */
        make::stack_size({stack_size}),      /* stack_size */
        make::rtos_priority({rtos_priority_num}),    /* rtos_priority */
        {create_native},   /* create_native */
        make::cpu_affinity({cpu_affinity}),  /* cpu_affinity */
        make::watchdog_timeout({watchdog_timeout}), /* watchdog_timeout_ms */
        {watchdog_action_enum},  /* watchdog_action */
        make::max_execution({max_execution})  /* max_execution_us */
    )"""
        
        task_configs.append(task_config)
    
    task_configurations = ',\n'.join(task_configs)
    
    # Fill template
    header = template.format(
        filename=Path(output_file).name,
        yaml_source=Path(yaml_file).name,
        forward_declarations=forward_declarations,
        message_types=message_types,
        communication_setup=communication_setup,
        task_configurations=task_configurations
    )
    
    # Write main generated header
    with open(output_file, 'w') as f:
        f.write(header)

    # Derive messaging limits from YAML to allow config overrides
    # - Queue capacity: use the maximum 'queue_size' across channels (fallback 16)
    # - Max topics: number of channels (fallback 32)
    # - Max subscribers per topic: maximum 'max_subscribers' across channels (fallback 8)
    max_queue_size = 0
    max_subscribers = 0
    for ch in channels:
        q = ch.get('queue_size', 0) or 0
        s = ch.get('max_subscribers', 0) or 0
        if isinstance(q, int):
            max_queue_size = max(max_queue_size, q)
        if isinstance(s, int):
            max_subscribers = max(max_subscribers, s)
    topics_count = len(channels)

    # Defaults if YAML doesn't specify
    if max_queue_size <= 0:
        max_queue_size = 16
    if topics_count <= 0:
        topics_count = 32
    if max_subscribers <= 0:
        max_subscribers = 8

    # Emit generated messaging config header for compile-time overrides
    generated_dir = Path(__file__).parent.parent / "src" / "emCore" / "generated"
    generated_dir.mkdir(parents=True, exist_ok=True)
    messaging_cfg_path = generated_dir / "messaging_config.hpp"
    messaging_cfg = f"""
#pragma once
// Auto-generated from {{yaml_file}}
// Derived limits from YAML to override defaults in emCore::config
#define EMCORE_MSG_QUEUE_CAPACITY {max_queue_size}
#define EMCORE_MSG_MAX_TOPICS {topics_count}
#define EMCORE_MSG_MAX_SUBS_PER_TOPIC {max_subscribers}
"""
    # Format with actual yaml file name visible in header comment
    messaging_cfg = messaging_cfg.replace("{yaml_file}", str(Path(yaml_file).name))
    with open(messaging_cfg_path, 'w') as f:
        f.write(messaging_cfg)

    print(f"Generated {output_file} with {len(tasks)} tasks")
    print(f"Generated {messaging_cfg_path} with YAML-derived messaging limits")

def find_task_yaml():
    """Find task configuration YAML file by scanning all .yaml files for proper schema"""
    current_dir = Path.cwd()
    
    # Find all .yaml and .yml files in current directory
    yaml_files = []
    yaml_files.extend(current_dir.glob("*.yaml"))
    yaml_files.extend(current_dir.glob("*.yml"))
    
    for yaml_path in yaml_files:
        try:
            with open(yaml_path, 'r') as f:
                content = yaml.safe_load(f)
                
                # Check if it has proper task schema structure
                if (isinstance(content, dict) and 
                    'tasks' in content and 
                    isinstance(content['tasks'], list) and
                    len(content['tasks']) > 0):
                    
                    # Verify first task has required schema fields
                    first_task = content['tasks'][0]
                    required_fields = ['name', 'function']
                    
                    if all(field in first_task for field in required_fields):
                        print(f"Found valid task configuration: {yaml_path.name}")
                        return yaml_path
                        
        except Exception as e:
            # Skip files that can't be parsed or read
            continue
    
    return None

def main():
    if len(sys.argv) < 2:
        # Auto-detect task configuration file in user's project
        yaml_file = find_task_yaml()
        output_file = Path.cwd() / "src" / "generated_tasks.hpp"
        
        if yaml_file is None:
            print("Error: No valid task configuration file found")
            print("Scanned all .yaml and .yml files in current directory")
            print("Looking for files with this structure:")
            print("  tasks:")
            print("    - name: \"TaskName\"")
            print("      function: \"task_function\"")
            print("      priority: \"low\"")
            print("      # ... other fields")
            print("\nExample usage:")
            print("  python /path/to/generate_tasks.py tasks.yaml src/generated_tasks.hpp")
            print("  python /path/to/generate_tasks.py  # (auto-detects valid YAML)")
            sys.exit(1)
            
        print(f"Auto-detected: {yaml_file} -> {output_file}")
    else:
        yaml_file = Path(sys.argv[1])
        output_file = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("generated_tasks.hpp")
    
    if not yaml_file.exists():
        print(f"Error: {yaml_file} not found")
        sys.exit(1)
    
    generate_task_config_header(yaml_file, output_file)

if __name__ == "__main__":
    main()
