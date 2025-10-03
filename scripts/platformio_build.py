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

print("🚀 emCore: Starting task generation check...")
print("🔥 PLATFORMIO BUILD SCRIPT IS RUNNING!")
print(f"🔥 PROJECT_DIR: {env.get('PROJECT_DIR')}")
print(f"🔥 BUILD_DIR: {env.get('BUILD_DIR')}")

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
    
    # Look for tasks.yaml in user's project root or test/
    tasks_yaml = project_dir / "tasks.yaml"
    generated_file = project_dir / "src" / "generated_tasks.hpp"
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
        result = subprocess.run([
            sys.executable, 
            str(generator_script),
            str(tasks_yaml),
            str(generated_file)
        ], 
        cwd=str(project_dir),
        capture_output=True, 
        text=True,
        check=True
        )
        
        print("✅ emCore: Task configuration generated successfully!")
        if result.stdout.strip():
            print(result.stdout)
        
        # Verify the output file was created
        if generated_file.exists():
            file_size = generated_file.stat().st_size
            print(f"✅ Generated: {generated_file} ({file_size} bytes)")
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

    # Probe locations: <project>/packet.yml or <project>/test/packet.yml
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
    generated_header = project_dir / "src" / "generated_packet_config.hpp"

    print("=" * 60)
    print("🚀 emCore: GENERATING PACKET CONFIGURATION...")
    print("=" * 60)
    try:
        print(f"📝 Running packet generator: {generator_script}")
        print(f"📄 Packet file: {packet_yaml}")
        # Pass explicit output path to place header in userspace
        result = subprocess.run([
            sys.executable,
            str(generator_script),
            str(packet_yaml),
            str(generated_header)
        ], cwd=str(project_dir), capture_output=True, text=True, check=True)

        print("✅ emCore: Packet configuration generated successfully!")
        if result.stdout.strip():
            print(result.stdout)

        if generated_header.exists():
            size = generated_header.stat().st_size
            print(f"✅ Generated (userspace): {generated_header} ({size} bytes)")
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

    # Probe locations: <project>/commands.yaml or <project>/test/commands.yaml
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
    generated_header = project_dir / "src" / "generated_command_table.hpp"

    print("=" * 60)
    print("🚀 emCore: GENERATING COMMAND TABLE...")
    print("=" * 60)
    try:
        print(f"📝 Running command generator: {generator_script}")
        print(f"📄 Command file: {commands_yaml}")
        # Pass explicit output path to place header in userspace
        result = subprocess.run([
            sys.executable,
            str(generator_script),
            str(commands_yaml),
            str(generated_header)
        ], cwd=str(project_dir), capture_output=True, text=True, check=True)

        print("✅ emCore: Command table generated successfully!")
        if result.stdout.strip():
            print(result.stdout)

        if generated_header.exists():
            size = generated_header.stat().st_size
            print(f"✅ Generated (userspace): {generated_header} ({size} bytes)")
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


# Only run generators once, not on every script load
if not hasattr(env, '_emcore_generators_run'):
    print("🔥 emCore: Running generators NOW...")
    try:
        generate_tasks_if_needed()
        generate_packet_if_needed()
        generate_command_if_needed()
        print("✅ emCore: Generators completed!")
        env._emcore_generators_run = True
    except Exception as e:
        print(f"❌ emCore: Generators failed: {e}")

print("📦 emCore library loaded - automatic task generation enabled")
