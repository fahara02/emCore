#!/usr/bin/env python3
"""
PlatformIO build script for emCore library
This script automatically generates task configuration when users build projects using emCore
"""

import os
import sys
import subprocess
from pathlib import Path
from typing import Optional

# Import PlatformIO build environment
Import("env")

def ensure_pyyaml():
    """Ensure PyYAML is installed, install it if missing."""
    print("🔍 emCore: Checking PyYAML availability...")
    try:
        import yaml
        print("✅ emCore: PyYAML is already available")
        return True
    except ImportError:
        print("📦 emCore: PyYAML not found, installing...")
        try:
            print(f"🐍 emCore: Using Python interpreter: {sys.executable}")
            result = subprocess.run([sys.executable, "-m", "pip", "install", "pyyaml"], 
                                  capture_output=True, text=True, timeout=120)
            if result.returncode == 0:
                print("✅ emCore: PyYAML installed successfully")
                # Try importing again to verify
                try:
                    import yaml
                    print("✅ emCore: PyYAML import verified")
                    return True
                except ImportError:
                    print("❌ emCore: PyYAML installed but import still fails")
                    return False
            else:
                print(f"❌ emCore: pip install failed with return code {result.returncode}")
                if result.stdout:
                    print(f"STDOUT: {result.stdout}")
                if result.stderr:
                    print(f"STDERR: {result.stderr}")
                return False
        except subprocess.TimeoutExpired:
            print("❌ emCore: PyYAML installation timed out")
            return False
        except Exception as e:
            print(f"❌ emCore: Unexpected error installing PyYAML: {e}")
            return False

# -----------------------------
# Pre-validation helpers
# -----------------------------
def _import_module(module_path: Path):
    import importlib.util
    spec = importlib.util.spec_from_file_location(module_path.stem, str(module_path))
    if spec is None or spec.loader is None:
        raise ImportError(f"Cannot import module at {module_path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)  # type: ignore[attr-defined]
    return mod

def _validate_tasks_yaml_with_generator(lib_dir: Path, yaml_path: Path) -> None:
    mod_path = lib_dir / "scripts" / "generate_tasks.py"
    mod = _import_module(mod_path)
    import yaml  # type: ignore
    with open(yaml_path, 'r', encoding='utf-8') as f:
        cfg = yaml.safe_load(f)
    # Raises ValueError with detailed list on failure
    mod.validate_yaml(cfg)

def _validate_packet_yaml_with_generator(lib_dir: Path, yaml_path: Path) -> None:
    mod_path = lib_dir / "scripts" / "generate_packet_config.py"
    mod = _import_module(mod_path)
    import yaml  # type: ignore
    with open(yaml_path, 'r', encoding='utf-8') as f:
        cfg = yaml.safe_load(f)
    mod.validate_packet_yaml(cfg)

def _validate_commands_yaml_basic(yaml_path: Path) -> None:
    import yaml  # type: ignore
    with open(yaml_path, 'r', encoding='utf-8') as f:
        cfg = yaml.safe_load(f) or {}
    errors = []
    cmds = cfg.get('commands', []) or []
    if not isinstance(cmds, list) or not cmds:
        errors.append("commands must be a non-empty list")
    names = set()
    opcodes = set()
    def _to_int(v):
        try:
            if isinstance(v, int):
                return v
            if isinstance(v, str):
                return int(v, 0)
        except Exception:
            return None
        return None
    for idx, c in enumerate(cmds):
        if not isinstance(c, dict):
            errors.append(f"commands[{idx}] must be a mapping")
            continue
        name = c.get('name')
        func = c.get('function')
        opc = _to_int(c.get('opcode'))
        if not isinstance(name, str) or not name.strip():
            errors.append(f"commands[{idx}].name must be non-empty string")
        elif name in names:
            errors.append(f"Duplicate command name: {name}")
        else:
            names.add(name)
        if not isinstance(func, str) or not func.strip():
            errors.append(f"commands[{idx}].function must be non-empty string")
        if opc is None or opc < 0 or opc > 0xFF:
            errors.append(f"commands[{idx}].opcode must be a byte (int or 0xNN string)")
        elif opc in opcodes:
            errors.append(f"Duplicate command opcode value: 0x{opc:02X}")
        else:
            opcodes.add(opc)
    if errors:
        raise ValueError("commands.yaml validation failed:\n - " + "\n - ".join(errors))

print("🚀 emCore: Starting task generation check...")
print("🔥 PLATFORMIO BUILD SCRIPT IS RUNNING!")
print(f"🔥 PROJECT_DIR: {env.get('PROJECT_DIR')}")
print(f"🔥 BUILD_DIR: {env.get('BUILD_DIR')}")

# -----------------------------
# YAML discovery & aggregation
# -----------------------------

def _candidate_roots(project_dir: Path) -> list[Path]:
    """Determine YAML search roots.

    Rules:
    - Always include the project root (same level as platformio.ini / .git / src)
    - Include well-known config dir names directly under the project root: 
      config, configs, setting, settings, cfg, conf, yaml, yml
    - Do NOT include test/ by default (tests should not feed production config)
    - Allow overrides/additions via EMCORE_YAML_DIRS (pathsep-separated)
    """
    roots = []
    try:
        roots.append(project_dir.resolve())
    except Exception:
        roots.append(project_dir)

    for name in ("config", "configs", "setting", "settings", "cfg", "conf", "yaml", "yml"):
        p = project_dir / name
        try:
            if p.exists() and p.is_dir():
                roots.append(p.resolve())
        except Exception:
            continue

    extra = os.environ.get("EMCORE_YAML_DIRS", "")
    if extra:
        for raw in extra.split(os.pathsep):
            p = Path(raw.strip())
            if not p:
                continue
            try:
                if p.exists() and p.is_dir():
                    roots.append(p.resolve())
            except Exception:
                continue

    # Dedup while preserving order
    seen = set()
    uniq_roots = []
    for r in roots:
        s = str(r)
        if s not in seen:
            seen.add(s)
            uniq_roots.append(r)
    return uniq_roots


def _list_yaml_files(project_dir: Path):
    """Walk candidate roots and collect any *.yml/*.yaml, excluding build/vendor dirs and merged outputs."""
    exclude_dir_names = {
        ".pio", "libdeps", ".git", ".svn", ".hg", ".vscode", ".idea", ".cache", ".vs",
        "build", "cmake", "out", "dist", "node_modules", "external", "third_party",
    }
    merged_file_names = {"merged_tasks.yaml", "merged_packet.yml", "merged_commands.yaml"}

    results = []
    seen_paths = set()
    for root in _candidate_roots(project_dir):
        try:
            for dirpath, dirnames, filenames in os.walk(root):
                # prune excluded directories in-place
                pruned = []
                for d in list(dirnames):
                    dn = d.lower()
                    if dn in exclude_dir_names or dn.startswith("cmake-build"):
                        pruned.append(d)
                for d in pruned:
                    try:
                        dirnames.remove(d)
                    except ValueError:
                        pass

                for fn in filenames:
                    if not (fn.endswith(".yaml") or fn.endswith(".yml")):
                        continue
                    if fn in merged_file_names:
                        continue
                    p = Path(dirpath) / fn
                    # Exclude anything under project .pio/emcore to avoid re-reading our merged outputs
                    try:
                        rel = p.resolve().relative_to(project_dir.resolve())
                        if str(rel).startswith(".pio/emcore/"):
                            continue
                    except Exception:
                        pass
                    sp = str(p.resolve())
                    if sp not in seen_paths:
                        seen_paths.add(sp)
                        results.append(p.resolve())
        except Exception:
            continue
    return results

def _aggregate_yaml(project_dir: Path):
    """Aggregate reserved YAML sections across user YAMLs with deduplication by name.

    Returns (tasks_cfg, packet_cfg, commands_cfg) or (None, None, None) if nothing found.
    - Last writer wins on duplicate names within the scoped set of user YAMLs.
    - Only root and test YAMLs are considered; .pio/libdeps/templates are ignored.
    """
    try:
        import yaml  # type: ignore
    except Exception:
        print("⚠️  emCore: PyYAML unavailable; skipping aggregation")
        return None, None, None

    # Helpers
    def _norm_name(n):
        if isinstance(n, str):
            return n.strip()
        return n
    def _to_int(v):
        try:
            if isinstance(v, int):
                return v
            if isinstance(v, str):
                return int(v, 0)
        except Exception:
            return None
        return None

    # Indexed maps for dedupe (preserve insertion order)
    tasks_by_name = {}
    messages_by_name = {}
    channels_by_name = {}
    messaging_cfg = {}

    packet_root = {}
    opcodes_by_name = {}
    opcodes_by_value = {}

    commands_by_name = {}
    commands_by_value = {}
    commands_config = {}

    found_tasks = False
    found_packet = False
    found_commands = False

    for yf in _list_yaml_files(project_dir):
        try:
            with yf.open('r', encoding='utf-8') as f:
                data = yaml.safe_load(f)
        except Exception:
            continue
        if not isinstance(data, dict):
            continue

        # Tasks/messages/channels/messaging
        if isinstance(data.get('tasks'), list) and data['tasks']:
            for t in data['tasks']:
                if isinstance(t, dict):
                    name = _norm_name(t.get('name'))
                    if isinstance(name, str) and name:
                        tasks_by_name[name] = t  # last writer wins
                        found_tasks = True
        if isinstance(data.get('messages'), list) and data['messages']:
            for m in data['messages']:
                if isinstance(m, dict):
                    name = _norm_name(m.get('name'))
                    if isinstance(name, str) and name:
                        messages_by_name[name] = m
                        found_tasks = True
        if isinstance(data.get('channels'), list) and data['channels']:
            for c in data['channels']:
                if isinstance(c, dict):
                    name = _norm_name(c.get('name'))
                    if isinstance(name, str) and name:
                        channels_by_name[name] = c
                        found_tasks = True
        if isinstance(data.get('messaging'), dict) and data['messaging']:
            # shallow merge
            for k, v in data['messaging'].items():
                if v is not None:
                    messaging_cfg[k] = v
            found_tasks = True

        # Packet/opcodes
        if isinstance(data.get('packet'), dict) and data['packet']:
            for k, v in data['packet'].items():
                if v is not None:
                    packet_root[k] = v
            found_packet = True
        if isinstance(data.get('opcodes'), list) and data['opcodes']:
            for op in data['opcodes']:
                if isinstance(op, dict):
                    name = _norm_name(op.get('name'))
                    if isinstance(name, str) and name:
                        # Track by name and numeric value; last writer wins
                        val = _to_int(op.get('opcode') if 'opcode' in op else op.get('value'))
                        # Remove any previous entry that had the same value but different name
                        if val is not None:
                            prev = opcodes_by_value.get(val)
                            if prev is not None and prev != name:
                                # Drop previous name mapping for this value
                                opcodes_by_name.pop(prev, None)
                        opcodes_by_name[name] = op
                        if val is not None:
                            opcodes_by_value[val] = name
                        found_packet = True

        # Commands/config
        if isinstance(data.get('commands'), list) and data['commands']:
            for cm in data['commands']:
                if isinstance(cm, dict):
                    name = _norm_name(cm.get('name'))
                    if isinstance(name, str) and name:
                        # Also dedupe by opcode/value if present
                        val = _to_int(cm.get('opcode') if 'opcode' in cm else cm.get('value'))
                        if val is not None:
                            prev = commands_by_value.get(val)
                            if prev is not None and prev != name:
                                commands_by_name.pop(prev, None)
                        commands_by_name[name] = cm
                        if val is not None:
                            commands_by_value[val] = name
                        found_commands = True
        if isinstance(data.get('config'), dict) and data['config']:
            for k, v in data['config'].items():
                if v is not None:
                    commands_config[k] = v
            found_commands = True

    tasks_cfg = None
    if found_tasks:
        tasks_cfg = {
            'tasks': list(tasks_by_name.values()),
            'messages': list(messages_by_name.values()),
            'channels': list(channels_by_name.values()),
            'messaging': messaging_cfg,
        }

    packet_cfg = None
    if found_packet:
        # Rebuild opcodes list ensuring uniqueness by numeric value (last writer wins)
        unique_ops = []
        seen_vals = set()
        for name, op in opcodes_by_name.items():
            val = _to_int(op.get('opcode') if 'opcode' in op else op.get('value'))
            if val is None:
                unique_ops.append(op)
                continue
            if val in seen_vals:
                continue
            seen_vals.add(val)
            unique_ops.append(op)
        packet_cfg = {
            'packet': packet_root,
            'opcodes': unique_ops,
        }

    commands_cfg = None
    if found_commands:
        # Ensure uniqueness by opcode as well
        unique_cmds = []
        seen_vals = set()
        for name, cm in commands_by_name.items():
            val = _to_int(cm.get('opcode') if 'opcode' in cm else cm.get('value'))
            if val is None:
                unique_cmds.append(cm)
                continue
            if val in seen_vals:
                continue
            seen_vals.add(val)
            unique_cmds.append(cm)
        commands_cfg = {
            'commands': unique_cmds,
            'config': commands_config,
        }

    return tasks_cfg, packet_cfg, commands_cfg

def _write_merged_yaml(cfg: dict, out_path: Path) -> bool:
    try:
        import yaml  # type: ignore
        out_path.parent.mkdir(parents=True, exist_ok=True)
        with out_path.open('w', encoding='utf-8') as f:
            yaml.safe_dump(cfg, f, sort_keys=False)
        return True
    except Exception as e:
        print(f"❌ emCore: Failed to write merged YAML {out_path}: {e}")
        return False

def _resolve_lib_dir(project_dir: Path) -> Optional[Path]:
    """Resolve the emCore library directory in both dev and libdeps contexts."""
    try:
        return Path(__file__).parent.parent
    except NameError:
        libdeps_dir = project_dir / ".pio" / "libdeps"
        if libdeps_dir.exists():
            for env_dir in libdeps_dir.iterdir():
                if env_dir.is_dir():
                    emcore_dir = env_dir / "emCore"
                    if emcore_dir.exists():
                        return emcore_dir
        return None


def generate_tasks_if_needed():
    """Generate task configuration if tasks.yaml exists in user project"""
    
    # Get the project directory (where user's platformio.ini is)
    project_dir = Path(env.get("PROJECT_DIR"))
    
    # Aggregate YAML strictly (no legacy fallbacks)
    merged_dir = project_dir / ".pio" / "emcore"
    merged_tasks_yaml = merged_dir / "merged_tasks.yaml"
    aggregated = _aggregate_yaml(project_dir)[0]
    if aggregated is None:
        print("📝 emCore: No YAML found to aggregate for tasks under candidate roots; skipping task generation")
        return
    merged_obj = {
        'tasks': aggregated.get('tasks', []),
        'messages': aggregated.get('messages', []),
        'channels': aggregated.get('channels', []),
        'messaging': aggregated.get('messaging', {}),
    }
    if not _write_merged_yaml(merged_obj, merged_tasks_yaml):
        print("📝 emCore: Failed to write merged tasks YAML; skipping task generation")
        return
    tasks_yaml = merged_tasks_yaml
    generated_file = project_dir / "include" / "generated_tasks.hpp"
    print(f"🧩 emCore: Using merged tasks YAML ({tasks_yaml}) with {len(merged_obj['tasks'])} tasks, {len(merged_obj['channels'])} channels")

    # Validate before generation and resolve library dir
    lib_dir = _resolve_lib_dir(project_dir)
    if not lib_dir:
        print("❌ emCore ERROR: Could not find library directory")
        return
    try:
        print("🔎 emCore: Validating tasks YAML...")
        _validate_tasks_yaml_with_generator(lib_dir, tasks_yaml)
        print("✅ emCore: Tasks YAML is valid")
    except Exception as ve:
        print("❌ emCore: Tasks YAML validation failed")
        print(str(ve))
        print("⛔ Skipping task generation due to validation errors")
        return
    
    # Force touch the YAML file to ensure proper timestamp detection
    tasks_yaml.touch(exist_ok=True)
    
    # Always regenerate to avoid stale headers
    print("🔄 emCore: Regenerating task header from YAML (forced)...")
    
    print("=" * 60)
    print("🚀 emCore: GENERATING TASK CONFIGURATION...")
    print("=" * 60)
    
    print(f"📂 emCore library directory: {lib_dir}")
    
    generator_script = lib_dir / "scripts" / "generate_tasks.py"
    
    if not generator_script.exists():
        print(f"❌ emCore ERROR: Generator script not found: {generator_script}")
        print(f"📂 Searched in library directory: {lib_dir}")
        return
    
    # Run the generator from user's project directory
    try:
        print(f"📝 Running emCore task generator...")
        print(f"📂 Project directory: {project_dir}")
        print(f"📄 Tasks file: {tasks_yaml}")
        print(f"🎯 Output: {generated_file}")
        
        # Explicitly pass YAML and output to ensure correct location
        # Use the same Python environment and ensure PyYAML is available
        env_vars = os.environ.copy()
        env_vars['PYTHONPATH'] = os.pathsep.join([str(p) for p in sys.path])
        
        result = subprocess.run([
            sys.executable, 
            str(generator_script),
            str(tasks_yaml),
            str(generated_file)
        ], 
        cwd=str(project_dir),
        capture_output=True, 
        text=True,
        check=True,
        env=env_vars
        )
        
        print("✅ emCore: Task configuration generated successfully!")
        if result.stdout.strip():
            print(result.stdout)
        
        # Verify the output file was created
        if generated_file.exists():
            file_size = generated_file.stat().st_size
            print(f"✅ Generated: {generated_file} ({file_size} bytes)")
            # Mirror into library as a fallback include so #include "generated_tasks.hpp" can resolve even if project include path is missing
            try:
                lib_dir.mkdir(parents=True, exist_ok=True)
                lib_tasks_header = lib_dir / "src" / "generated_tasks.hpp"
                lib_tasks_header.parent.mkdir(parents=True, exist_ok=True)
                with open(generated_file, 'r', encoding='utf-8') as srcf:
                    text = srcf.read()
                with open(lib_tasks_header, 'w', encoding='utf-8') as dstf:
                    dstf.write(text)
                print(f"🪄 Mirrored tasks header into library: {lib_tasks_header}")
            except Exception as me:
                print(f"⚠️  emCore: Failed to mirror tasks header into library: {me}")
        else:
            print(f"❌ ERROR: Generated file not found: {generated_file}")
            
    except subprocess.CalledProcessError as e:
        print(f"❌ emCore ERROR: Generator failed with exit code {e.returncode}")
        if e.stdout:
            print(f"STDOUT: {e.stdout}")
        if e.stderr:
            print(f"STDERR: {e.stderr}")
        print("⚠️  Build will continue, but generated_tasks.hpp may be missing")
        
    except Exception as e:
        print(f"❌ emCore ERROR: Unexpected error: {e}")
        print("⚠️  Build will continue, but generated_tasks.hpp may be missing")
    
    print("=" * 60)

def generate_packet_if_needed():
    """Generate packet configuration if packet.yml exists (root or test/packet.yml)."""
    project_dir = Path(env.get("PROJECT_DIR"))

    # Try aggregated YAML first
    merged_dir = project_dir / ".pio" / "emcore"
    merged_packet_yaml = merged_dir / "merged_packet.yml"
    aggregated = _aggregate_yaml(project_dir)[1]
    if aggregated is not None:
        merged_obj = {
            'packet': aggregated.get('packet', {}),
            'opcodes': aggregated.get('opcodes', []),
        }
        if _write_merged_yaml(merged_obj, merged_packet_yaml):
            packet_yaml = merged_packet_yaml
            print(f"🧩 emCore: Using merged packet YAML ({packet_yaml}) with {len(merged_obj['opcodes'])} opcodes")
        else:
            print("📝 emCore: Failed to write merged packet YAML; skipping packet generation")
            return
    else:
        print("📝 emCore: No YAML found to aggregate for packet under candidate roots; skipping packet generation")
        return

    # Resolve library dir and generator script
    lib_dir = _resolve_lib_dir(project_dir)
    if not lib_dir:
        print("❌ emCore ERROR: Could not find library directory for packet generation")
        return

    generator_script = lib_dir / "scripts" / "generate_packet_config.py"
    if not generator_script.exists():
        print(f"❌ emCore ERROR: Packet generator script not found: {generator_script}")
        return

    # Expected output in USER PROJECT SPACE
    generated_header = project_dir / "include" / "generated_packet_config.hpp"

    # Validate before generation
    try:
        print("🔎 emCore: Validating packet YAML...")
        _validate_packet_yaml_with_generator(lib_dir, packet_yaml)
        print("✅ emCore: Packet YAML is valid")
    except Exception as ve:
        print("❌ emCore: Packet YAML validation failed")
        print(str(ve))
        print("⛔ Skipping packet generation due to validation errors")
        return

    print("=" * 60)
    print("🚀 emCore: GENERATING PACKET CONFIGURATION...")
    print("=" * 60)
    try:
        print(f"📝 Running packet generator: {generator_script}")
        print(f"📄 Packet file: {packet_yaml}")
        # Pass explicit output path to place header in userspace
        # Use the same Python environment and ensure PyYAML is available
        env_vars = os.environ.copy()
        env_vars['PYTHONPATH'] = os.pathsep.join([str(p) for p in sys.path])
        
        result = subprocess.run([
            sys.executable,
            str(generator_script),
            str(packet_yaml),
            str(generated_header)
        ], cwd=str(project_dir), capture_output=True, text=True, check=True, env=env_vars)

        print("✅ emCore: Packet configuration generated successfully!")
        if result.stdout.strip():
            print(result.stdout)

        if generated_header.exists():
            size = generated_header.stat().st_size
            print(f"✅ Generated (userspace): {generated_header} ({size} bytes)")
            # Mirror packet config into library so protocol_global.hpp fallback include works
            try:
                lib_gen_dir = lib_dir / "src" / "emCore" / "generated"
                lib_gen_dir.mkdir(parents=True, exist_ok=True)
                lib_packet_header = lib_gen_dir / "packet_config.hpp"
                with open(generated_header, 'r', encoding='utf-8') as srcf:
                    text = srcf.read()
                with open(lib_packet_header, 'w', encoding='utf-8') as dstf:
                    dstf.write(text)
                print(f"🪄 Mirrored packet config into library: {lib_packet_header}")
            except Exception as me:
                print(f"⚠️  emCore: Failed to mirror packet config into library: {me}")
        else:
            print(f"❌ ERROR: Generated header not found (userspace): {generated_header}")

    except subprocess.CalledProcessError as e:
        print(f"❌ emCore ERROR: Packet generator failed with exit code {e.returncode}")
        if e.stdout:
            print(f"STDOUT: {e.stdout}")
        if e.stderr:
            print(f"STDERR: {e.stderr}")
        print("⚠️  Build will continue, but packet_config.hpp may be missing")
    except Exception as e:
        print(f"❌ emCore ERROR: Unexpected error (packet): {e}")
        print("⚠️  Build will continue, but packet_config.hpp may be missing")

def generate_command_if_needed():
    """Generate command table if commands.yaml exists (root or test/commands.yaml)."""
    project_dir = Path(env.get("PROJECT_DIR"))

    # Try aggregated YAML first
    merged_dir = project_dir / ".pio" / "emcore"
    merged_commands_yaml = merged_dir / "merged_commands.yaml"
    aggregated = _aggregate_yaml(project_dir)[2]
    if aggregated is not None:
        merged_obj = {
            'commands': aggregated.get('commands', []),
            'config': aggregated.get('config', {}),
        }
        if _write_merged_yaml(merged_obj, merged_commands_yaml):
            commands_yaml = merged_commands_yaml
            print(f"🧩 emCore: Using merged commands YAML ({commands_yaml}) with {len(merged_obj['commands'])} commands")
        else:
            print("📝 emCore: Failed to write merged commands YAML; skipping command table generation")
            return
    else:
        print("📝 emCore: No YAML found to aggregate for commands under candidate roots; skipping command table generation")
        return

    # Resolve library dir and generator script
    lib_dir = _resolve_lib_dir(project_dir)
    if not lib_dir:
        print("❌ emCore ERROR: Could not find library directory for command generation")
        return

    generator_script = lib_dir / "scripts" / "generate_command_table.py"
    if not generator_script.exists():
        print(f"❌ emCore ERROR: Command generator script not found: {generator_script}")
        return

    # Expected output in USER PROJECT SPACE
    generated_header = project_dir / "include" / "generated_command_table.hpp"

    # Validate before generation
    try:
        print("🔎 emCore: Validating commands YAML...")
        _validate_commands_yaml_basic(commands_yaml)
        print("✅ emCore: Commands YAML is valid")
    except Exception as ve:
        print("❌ emCore: Commands YAML validation failed")
        print(str(ve))
        print("⛔ Skipping command table generation due to validation errors")
        return

    print("=" * 60)
    print("🚀 emCore: GENERATING COMMAND TABLE...")
    print("=" * 60)
    try:
        print(f"📝 Running command generator: {generator_script}")
        print(f"📄 Command file: {commands_yaml}")
        # Pass explicit output path to place header in userspace
        # Use the same Python environment and ensure PyYAML is available
        env_vars = os.environ.copy()
        env_vars['PYTHONPATH'] = os.pathsep.join([str(p) for p in sys.path])
        
        result = subprocess.run([
            sys.executable,
            str(generator_script),
            str(commands_yaml),
            str(generated_header)
        ], cwd=str(project_dir), capture_output=True, text=True, check=True, env=env_vars)

        print("✅ emCore: Command table generated successfully!")
        if result.stdout.strip():
            print(result.stdout)

        if generated_header.exists():
            size = generated_header.stat().st_size
            print(f"✅ Generated (userspace): {generated_header} ({size} bytes)")
            # Mirror command table into library for convenience
            try:
                lib_gen_dir = lib_dir / "src" / "emCore" / "generated"
                lib_gen_dir.mkdir(parents=True, exist_ok=True)
                lib_cmd_header = lib_gen_dir / "generated_command_table.hpp"
                with open(generated_header, 'r', encoding='utf-8') as srcf:
                    text = srcf.read()
                with open(lib_cmd_header, 'w', encoding='utf-8') as dstf:
                    dstf.write(text)
                print(f"🪄 Mirrored command table into library: {lib_cmd_header}")
            except Exception as me:
                print(f"⚠️  emCore: Failed to mirror command table into library: {me}")
        else:
            print(f"❌ ERROR: Generated header not found (userspace): {generated_header}")

    except subprocess.CalledProcessError as e:
        print(f"❌ emCore ERROR: Command generator failed with exit code {e.returncode}")
        if e.stdout:
            print(f"STDOUT: {e.stdout}")
        if e.stderr:
            print(f"STDERR: {e.stderr}")
        print("⚠️  Build will continue, but generated_command_table.hpp may be missing")
    except Exception as e:
        print(f"❌ emCore ERROR: Unexpected error (command): {e}")
        print("⚠️  Build will continue, but generated_command_table.hpp may be missing")


# Note: No link-time RAM budget injection here; compile-time budgeting is enforced
# via src/emCore/memory/budget.hpp. Keep this script limited to code generation.


# Only run generators once, not on every script load
if not hasattr(env, '_emcore_generators_run'):
    print("🔥 emCore: Running generators NOW...")
    
    # Ensure PyYAML is available before running generators
    print("🔧 emCore: Checking Python dependencies...")
    pyyaml_ok = ensure_pyyaml()
    if not pyyaml_ok:
        print("⚠️  emCore: PyYAML installation failed, generators may not work")
        print("⚠️  emCore: Manual installation: pip install pyyaml")
    else:
        print("✅ emCore: Python dependencies ready")
    
    try:
        generate_tasks_if_needed()
        generate_packet_if_needed()
        generate_command_if_needed()
        print("✅ emCore: Generators completed!")
        env._emcore_generators_run = True
    except Exception as e:
        print(f"❌ emCore: Generators failed: {e}")

print("📦 emCore library loaded - automatic task generation enabled")
