#!/usr/bin/env python3
"""
Generate C++ task configuration from YAML file
Usage: python generate_tasks.py [input.yaml] [output.hpp]
"""

import sys
import os
from pathlib import Path

try:
    import yaml
except ImportError:
    print("ERROR: PyYAML not installed. Install with: pip install pyyaml")
    sys.exit(1)

# -----------------------------
# YAML validation
# -----------------------------
def _is_identifier(name: str) -> bool:
    if not isinstance(name, str) or not name:
        return False
    if not (name[0].isalpha() or name[0] == '_'):
        return False
    for ch in name:
        if not (ch.isalnum() or ch == '_' or ch == ':'):
            return False
    return True

def validate_yaml(config: dict) -> None:
    """Validate YAML schema for tasks, messages, channels, and messaging root.
    Raises ValueError listing all issues if any are found."""
    errors = []

    # Sections
    messages = config.get('messages', []) or []
    channels = config.get('channels', []) or []
    tasks = config.get('tasks', []) or []
    messaging = config.get('messaging', {}) or {}

    # Messages: uniqueness and identifier sanity
    if not isinstance(messages, list):
        errors.append("'messages' must be a list")
        messages = []
    msg_names = []
    for idx, msg in enumerate(messages):
        if not isinstance(msg, dict):
            errors.append(f"messages[{idx}] must be a mapping")
            continue
        name = msg.get('name', '')
        if not _is_identifier(name):
            errors.append(f"messages[{idx}].name is missing or not a valid identifier: {name}")
        elif name in msg_names:
            errors.append(f"Duplicate message name: {name}")
        else:
            msg_names.append(name)
        fields = msg.get('fields', []) or []
        if not isinstance(fields, list):
            errors.append(f"messages[{idx}].fields must be a list")
        else:
            for fidx, field in enumerate(fields):
                if not isinstance(field, dict):
                    errors.append(f"messages[{idx}].fields[{fidx}] must be a mapping")
                    continue
                fname = field.get('name', '')
                ftype = field.get('type', '')
                if not _is_identifier(fname):
                    errors.append(f"messages[{idx}].fields[{fidx}].name invalid: {fname}")
                if not _is_identifier(ftype):
                    errors.append(f"messages[{idx}].fields[{fidx}].type invalid: {ftype}")

    # Channels: uniqueness, required keys, and references
    if not isinstance(channels, list):
        errors.append("'channels' must be a list")
        channels = []
    ch_names = []
    allowed_priorities = { 'idle','low','normal','high','critical' }
    allowed_flags = { 'none','requires_ack','broadcast','urgent','persistent' }
    allowed_ts = { 'producer','broker' }
    allowed_overflow = { 'drop_oldest','reject' }
    for idx, ch in enumerate(channels):
        if not isinstance(ch, dict):
            errors.append(f"channels[{idx}] must be a mapping")
            continue
        name = ch.get('name', '')
        mtype = ch.get('message_type', '')
        if not _is_identifier(name):
            errors.append(f"channels[{idx}].name invalid: {name}")
        elif name in ch_names:
            errors.append(f"Duplicate channel name: {name}")
        else:
            ch_names.append(name)
        if not _is_identifier(mtype):
            errors.append(f"channels[{idx}].message_type invalid: {mtype}")
        elif mtype not in msg_names:
            errors.append(f"channels[{idx}].message_type '{mtype}' not found in messages list")
        # numeric options
        qsize = ch.get('queue_size', 16)
        if not isinstance(qsize, int) or qsize <= 0:
            errors.append(f"channels[{idx}].queue_size must be positive integer")
        msubs = ch.get('max_subscribers', 8)
        if not isinstance(msubs, int) or msubs <= 0:
            errors.append(f"channels[{idx}].max_subscribers must be positive integer")
        # defaults
        prio = str(ch.get('default_priority','normal')).lower()
        if prio not in allowed_priorities:
            errors.append(f"channels[{idx}].default_priority invalid: {prio}")
        flags = ch.get('default_flags', []) or []
        if isinstance(flags, str):
            flags = [flags]
        if not isinstance(flags, list):
            errors.append(f"channels[{idx}].default_flags must be list or string")
        else:
            for f in flags:
                if str(f).lower() not in allowed_flags:
                    errors.append(f"channels[{idx}].default_flags contains invalid flag: {f}")
        ts = str(ch.get('timestamp_source','producer')).lower()
        if ts not in allowed_ts:
            errors.append(f"channels[{idx}].timestamp_source invalid: {ts}")
        ofp = ch.get('overflow_policy', 'drop_oldest')
        if ofp is not None and str(ofp).lower() not in allowed_overflow:
            errors.append(f"channels[{idx}].overflow_policy invalid: {ofp}")

    # Messaging root
    if messaging:
        if not isinstance(messaging, dict):
            errors.append("'messaging' must be a mapping if provided")
        else:
            tpm = messaging.get('topic_queues_per_mailbox', None)
            if tpm is not None and (not isinstance(tpm, int) or tpm <= 0):
                errors.append("messaging.topic_queues_per_mailbox must be positive integer")
            hrn = messaging.get('topic_high_ratio_num', None)
            hrd = messaging.get('topic_high_ratio_den', None)
            if hrd == 0:
                errors.append("messaging.topic_high_ratio_den must be non-zero")
            if hrn is not None and (not isinstance(hrn, int) or hrn < 0):
                errors.append("messaging.topic_high_ratio_num must be non-negative integer")
            if hrd is not None and (not isinstance(hrd, int) or hrd < 1):
                errors.append("messaging.topic_high_ratio_den must be positive integer")
            no = messaging.get('notify_on_empty_only', None)
            if no is not None and not isinstance(no, bool):
                errors.append("messaging.notify_on_empty_only must be boolean")

    # Tasks
    if not isinstance(tasks, list):
        errors.append("'tasks' must be a list")
        tasks = []
    task_names = []
    for idx, t in enumerate(tasks):
        if not isinstance(t, dict):
            errors.append(f"tasks[{idx}] must be a mapping")
            continue
        name = t.get('TaskName', t.get('name',''))
        func = t.get('TaskEntryPtr', t.get('function',''))
        if not _is_identifier(name):
            errors.append(f"tasks[{idx}].name invalid: {name}")
        elif name in task_names:
            errors.append(f"Duplicate task name: {name}")
        else:
            task_names.append(name)
        if not _is_identifier(func):
            errors.append(f"tasks[{idx}].function invalid: {func}")
        # Required watchdog fields (as enforced later)
        if t.get('watchdog_timeout_ms', None) is None:
            errors.append(f"tasks[{idx}] missing required 'watchdog_timeout_ms'")
        if t.get('watchdog_action', None) is None:
            errors.append(f"tasks[{idx}] missing required 'watchdog_action'")
        # Subscriptions
        subs = t.get('subscribes_to', []) or []
        if not isinstance(subs, list):
            errors.append(f"tasks[{idx}].subscribes_to must be a list")
        else:
            for sidx, sub in enumerate(subs):
                if isinstance(sub, str):
                    ch_name = sub
                elif isinstance(sub, dict):
                    ch_name = sub.get('channel','')
                    md = sub.get('mailbox_depth', None)
                    if md is not None and (not isinstance(md, int) or md <= 0):
                        errors.append(f"tasks[{idx}].subscribes_to[{sidx}].mailbox_depth must be positive integer")
                    ofp = sub.get('overflow_policy', None)
                    if ofp is not None and str(ofp).lower() not in allowed_overflow:
                        errors.append(f"tasks[{idx}].subscribes_to[{sidx}].overflow_policy invalid: {ofp}")
                else:
                    errors.append(f"tasks[{idx}].subscribes_to[{sidx}] must be string or mapping")
                    continue
                if ch_name not in ch_names:
                    errors.append(f"tasks[{idx}].subscribes_to[{sidx}] references unknown channel: {ch_name}")

    if errors:
        raise ValueError("YAML validation failed:\n - " + "\n - ".join(errors))

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
    # Ensure direct includes for types used later in generated helpers
    message_code.append("// Auto-generated message types")
    message_code.append("#include <cstddef>")
    message_code.append("#include <cstdint>")
    message_code.append("#include <etl/algorithm.h>  // etl::copy_n")
    message_code.append("#include <emCore/messaging/message_types.hpp>  // medium_message, priorities, flags")
    
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

def generate_communication_setup(channels, tasks, messaging_cfg):
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
        # Channel defaults
        default_priority = channel.get('default_priority', 'normal')
        priority_enum = {
            'idle': 'emCore::messaging::message_priority::low',
            'low': 'emCore::messaging::message_priority::low',
            'normal': 'emCore::messaging::message_priority::normal',
            'high': 'emCore::messaging::message_priority::high',
            'critical': 'emCore::messaging::message_priority::critical'
        }.get(str(default_priority).lower(), 'emCore::messaging::message_priority::normal')

        flags_list = channel.get('default_flags', []) or []
        if isinstance(flags_list, str):
            flags_list = [flags_list]
        flag_map = {
            'none': 'emCore::messaging::message_flags::none',
            'requires_ack': 'emCore::messaging::message_flags::requires_ack',
            'broadcast': 'emCore::messaging::message_flags::broadcast',
            'urgent': 'emCore::messaging::message_flags::urgent',
            'persistent': 'emCore::messaging::message_flags::persistent',
        }
        flag_exprs = [flag_map.get(str(f).lower(), 'emCore::messaging::message_flags::none') for f in flags_list]
        if not flag_exprs:
            flags_expr = 'emCore::messaging::message_flags::none'
        else:
            flags_expr = ' | '.join(flag_exprs)

        timestamp_source = str(channel.get('timestamp_source', 'producer')).lower()
        
        # Pack function
        comm_code.append(f"// Pack {msg_type} into medium_message for {name}")
        comm_code.append(f"inline void pack_{topic_name}_message(const {msg_type}& data, emCore::messaging::medium_message& msg) noexcept {{")
        comm_code.append(f"    msg.header.type = static_cast<emCore::u16>(yaml_topic::{topic_name});")
        comm_code.append(f"    msg.header.payload_size = sizeof({msg_type});")
        comm_code.append(f"    msg.header.priority = static_cast<emCore::u8>({priority_enum});")
        comm_code.append(f"    msg.header.flags = static_cast<emCore::u8>({flags_expr});")
        if timestamp_source == 'broker':
            comm_code.append(f"    msg.header.timestamp = 0;  // broker will stamp on publish")
        else:
            comm_code.append(f"    msg.header.timestamp = emCore::platform::get_system_time_us();")
        # Avoid array-to-pointer decay by taking address of first element explicitly
        comm_code.append(f"    etl::copy_n(reinterpret_cast<const uint8_t*>(&data), sizeof({msg_type}), &msg.payload[0]);")
        comm_code.append(f"}}")
        comm_code.append("")
        
        # Unpack function  
        comm_code.append(f"// Unpack {msg_type} from medium_message for {name}")
        comm_code.append(f"inline bool unpack_{topic_name}_message(const emCore::messaging::medium_message& msg, {msg_type}& data) noexcept {{")
        comm_code.append(f"    if (msg.header.type != static_cast<emCore::u16>(yaml_topic::{topic_name})) {{")
        comm_code.append(f"        return false;")
        comm_code.append(f"    }}")
        # Avoid array-to-pointer decay by taking address of first element explicitly
        comm_code.append(f"    etl::copy_n(&msg.payload[0], sizeof({msg_type}), reinterpret_cast<uint8_t*>(&data));")
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

    # Global broker policies
    notify_only = bool(messaging_cfg.get('notify_on_empty_only', True))
    comm_code.append(f"    // Broker notify policy from YAML")
    comm_code.append(f"    task_mgr.set_notify_on_empty_only({str(notify_only).lower()});")

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
            # Determine overflow policy per task from subscriptions (default drop_oldest)
            task_drop_oldest = True
            for sub in subscribes:
                if isinstance(sub, dict):
                    channel_name = sub.get('channel', '')
                else:
                    channel_name = sub
                for channel in channels:
                    if channel.get('name','') == channel_name:
                        topic_name = channel_name.replace('_channel','')
                        comm_code.append(f"        emCore::taskmaster::subscribe(emCore::topic_id_t(static_cast<emCore::u16>(yaml_topic::{topic_name})), task_id);")
                        # Mailbox depth: prefer explicit subscription override, fallback to channel queue_size
                        q_override = 0
                        if isinstance(sub, dict):
                            q_override = int(sub.get('mailbox_depth', 0) or 0)
                        qsize = q_override if q_override > 0 else channel_queue_size.get(channel_name, 16)
                        comm_code.append(f"        task_mgr.set_mailbox_depth(task_id, {int(qsize)});")
                        # Overflow policy: subscription override > channel setting > default
                        sub_policy = None
                        if isinstance(sub, dict):
                            sub_policy = sub.get('overflow_policy', None)
                        ch_policy = channel.get('overflow_policy', None)
                        policy = sub_policy if sub_policy is not None else ch_policy
                        if isinstance(policy, str) and policy.lower() == 'reject':
                            task_drop_oldest = False
                        break
            comm_code.append(f"        task_mgr.set_overflow_policy(task_id, {str(task_drop_oldest).lower()});")
        
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
    # Validate YAML schema and invariants
    validate_yaml(config)
    
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
    messaging_root = config.get('messaging', {}) or {}
    communication_setup = generate_communication_setup(channels, tasks, messaging_root)
    
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

    # Also mirror into project's include/ so that libraries can include it
    try:
        out_path = Path(output_file)
        # Mirror only when placed under a typical userspace src/ tree
        if out_path.parent.name == 'src':
            include_dir = out_path.parent.parent / 'include'
            include_dir.mkdir(parents=True, exist_ok=True)
            include_path = include_dir / out_path.name
            with open(include_path, 'w') as f:
                f.write(header)
    except Exception:
        # Best-effort; non-fatal if mirroring fails
        pass

    # Derive messaging limits from YAML to allow config overrides (opt-in generation)
    # Enable by setting environment variable: EMCORE_GENERATE_MESSAGING_CONFIG=1
    emit_msg_cfg = os.getenv("EMCORE_GENERATE_MESSAGING_CONFIG", "0") == "1"
    if emit_msg_cfg:
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
// Auto-generated from {yaml_file}
// Derived limits from YAML to override defaults in emCore::config
#define EMCORE_MSG_QUEUE_CAPACITY {max_queue_size}
#define EMCORE_MSG_MAX_TOPICS {topics_count}
#define EMCORE_MSG_MAX_SUBS_PER_TOPIC {max_subscribers}
"""
        # Optional overrides from messaging root (per-topic queues and high/normal split)
        if isinstance(messaging_root, dict):
            tpm = messaging_root.get('topic_queues_per_mailbox', None)
            hrn = messaging_root.get('topic_high_ratio_num', None)
            hrd = messaging_root.get('topic_high_ratio_den', None)
            if isinstance(tpm, int) and tpm > 0:
                messaging_cfg += f"#define EMCORE_MSG_TOPIC_QUEUES_PER_MAILBOX {int(tpm)}\n"
            if isinstance(hrn, int) and hrn > 0:
                messaging_cfg += f"#define EMCORE_MSG_TOPIC_HIGH_RATIO_NUM {int(hrn)}\n"
            if isinstance(hrd, int) and hrd > 0:
                messaging_cfg += f"#define EMCORE_MSG_TOPIC_HIGH_RATIO_DEN {int(hrd)}\n"
        # Format with actual yaml file name visible in header comment
        messaging_cfg = messaging_cfg.replace("{yaml_file}", str(Path(yaml_file).name))
        with open(messaging_cfg_path, 'w') as f:
            f.write(messaging_cfg + "\n")  # Add newline at end of file

        print(f"Generated {messaging_cfg_path} with YAML-derived messaging limits")
    else:
        print("Skipping generation of messaging_config.hpp (EMCORE_GENERATE_MESSAGING_CONFIG != 1)")

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
