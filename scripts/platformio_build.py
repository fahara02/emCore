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

print("🚀 emCore: Starting task generation check...")
print("🔥 PLATFORMIO BUILD SCRIPT IS RUNNING!")
print(f"🔥 PROJECT_DIR: {env.get('PROJECT_DIR')}")
print(f"🔥 BUILD_DIR: {env.get('BUILD_DIR')}")

# -----------------------------
# YAML scanning & aggregation
# -----------------------------
def _list_yaml_files(project_dir: Path):
    """Recursively list all .yaml/.yml files in the user's project directory."""
    files = []
    try:
        files.extend(project_dir.rglob("*.yaml"))
        files.extend(project_dir.rglob("*.yml"))
    except Exception:
        pass
    # Deduplicate while preserving order
    seen = set()
    out = []
    for p in files:
        if p.is_file():
            s = str(p.resolve())
            if s not in seen:
                seen.add(s)
                out.append(p)
    return out

def _aggregate_yaml(project_dir: Path):
    """Aggregate reserved YAML sections across all YAML files.

    Returns a tuple of (tasks_cfg, packet_cfg, commands_cfg) where each element is either a dict
    with the reserved keys or None if nothing was found.
    """
    # Ensure PyYAML is importable (should be after ensure_pyyaml())
    try:
        import yaml  # type: ignore
    except Exception:
        print("⚠️  emCore: PyYAML unavailable; skipping aggregation")
        return None, None, None

    tasks_cfg = { 'tasks': [], 'messages': [], 'channels': [], 'messaging': {} }
    packet_cfg = { 'packet': {}, 'opcodes': [] }
    commands_cfg = { 'commands': [], 'config': {} }

    found_any_tasks = False
    found_any_packet = False
    found_any_commands = False

    for yf in _list_yaml_files(project_dir):
        try:
            with yf.open('r', encoding='utf-8') as f:
                data = yaml.safe_load(f)
        except Exception:
            continue
        if not isinstance(data, dict):
            continue

        # Aggregate tasks/messages/channels/messaging
        if isinstance(data.get('tasks'), list) and data['tasks']:
            tasks_cfg['tasks'].extend([t for t in data['tasks'] if isinstance(t, dict)])
            found_any_tasks = True
        if isinstance(data.get('messages'), list) and data['messages']:
            tasks_cfg['messages'].extend([m for m in data['messages'] if isinstance(m, dict)])
            found_any_tasks = True
        if isinstance(data.get('channels'), list) and data['channels']:
            tasks_cfg['channels'].extend([c for c in data['channels'] if isinstance(c, dict)])
            found_any_tasks = True
        if isinstance(data.get('messaging'), dict) and data['messaging']:
            # Shallow update (last writer wins) for scalar YAML-wide messaging knobs
            tasks_cfg['messaging'].update({ k:v for k,v in data['messaging'].items() if v is not None })
            found_any_tasks = True

        # Aggregate packet/opcodes
        if isinstance(data.get('packet'), dict) and data['packet']:
            # Last writer wins for per-project packet root
            packet_cfg['packet'].update({ k:v for k,v in data['packet'].items() if v is not None })
            found_any_packet = True
        if isinstance(data.get('opcodes'), list) and data['opcodes']:
            packet_cfg['opcodes'].extend([op for op in data['opcodes'] if isinstance(op, dict)])
            found_any_packet = True

        # Aggregate commands/config
        if isinstance(data.get('commands'), list) and data['commands']:
            commands_cfg['commands'].extend([cm for cm in data['commands'] if isinstance(cm, dict)])
            found_any_commands = True
        if isinstance(data.get('config'), dict) and data['config']:
            commands_cfg['config'].update({ k:v for k,v in data['config'].items() if v is not None })
            found_any_commands = True

    return (tasks_cfg if found_any_tasks else None,
            packet_cfg if found_any_packet else None,
            commands_cfg if found_any_commands else None)

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
    
    # Try aggregated YAML first
    merged_dir = project_dir / ".pio" / "emcore"
    merged_tasks_yaml = merged_dir / "merged_tasks.yaml"
    aggregated = _aggregate_yaml(project_dir)[0]
    if aggregated is not None:
        # Only keep reserved keys
        merged_obj = {
            'tasks': aggregated.get('tasks', []),
            'messages': aggregated.get('messages', []),
            'channels': aggregated.get('channels', []),
            'messaging': aggregated.get('messaging', {}),
        }
        if _write_merged_yaml(merged_obj, merged_tasks_yaml):
            tasks_yaml = merged_tasks_yaml
            generated_file = project_dir / "include" / "generated_tasks.hpp"
            print(f"🧩 emCore: Using merged tasks YAML ({tasks_yaml}) with {len(merged_obj['tasks'])} tasks, {len(merged_obj['channels'])} channels")
        else:
            # Fallback to legacy detection
            tasks_yaml = project_dir / "tasks.yaml"
            generated_file = project_dir / "include" / "generated_tasks.hpp"
            if not tasks_yaml.exists():
                alt = project_dir / "test" / "tasks.yaml"
                if alt.exists():
                    tasks_yaml = alt
                    generated_file = project_dir / "test" / "src" / "generated_tasks.hpp"
                else:
                    print("📝 emCore: No tasks.yaml found (root or test/), skipping task generation")
                    return
    else:
        # Fallback to legacy detection
        tasks_yaml = project_dir / "tasks.yaml"
        generated_file = project_dir / "include" / "generated_tasks.hpp"
        if not tasks_yaml.exists():
            alt = project_dir / "test" / "tasks.yaml"
            if alt.exists():
                tasks_yaml = alt
                generated_file = project_dir / "test" / "src" / "generated_tasks.hpp"
            else:
                print("📝 emCore: No tasks.yaml found (root or test/), skipping task generation")
                return
    
    # Force touch the YAML file to ensure proper timestamp detection
    tasks_yaml.touch(exist_ok=True)
    
    # Always regenerate to avoid stale headers
    print("🔄 emCore: Regenerating task header from YAML (forced)...")
    
    print("=" * 60)
    print("🚀 emCore: GENERATING TASK CONFIGURATION...")
    print("=" * 60)
    
    # Find the emCore library directory
    # In PlatformIO context, __file__ might not be available
    lib_dir = _resolve_lib_dir(project_dir)
    if not lib_dir:
        print("❌ emCore ERROR: Could not find library directory")
        return
    
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
            # Fallback to legacy detection
            packet_yaml = project_dir / "packet.yml"
            if not packet_yaml.exists():
                alt = project_dir / "test" / "packet.yml"
                if alt.exists():
                    packet_yaml = alt
                else:
                    print("📝 emCore: No packet.yml found, skipping packet generation")
                    return
    else:
        # Fallback to legacy detection
        packet_yaml = project_dir / "packet.yml"
        if not packet_yaml.exists():
            alt = project_dir / "test" / "packet.yml"
            if alt.exists():
                packet_yaml = alt
            else:
                print("📝 emCore: No packet.yml found, skipping packet generation")
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
            # Fallback to legacy detection
            commands_yaml = project_dir / "commands.yaml"
            if not commands_yaml.exists():
                alt = project_dir / "test" / "commands.yaml"
                if alt.exists():
                    commands_yaml = alt
                else:
                    print("📝 emCore: No commands.yaml found, skipping command table generation")
                    return
    else:
        # Fallback to legacy detection
        commands_yaml = project_dir / "commands.yaml"
        if not commands_yaml.exists():
            alt = project_dir / "test" / "commands.yaml"
            if alt.exists():
                commands_yaml = alt
            else:
                print("📝 emCore: No commands.yaml found, skipping command table generation")
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
