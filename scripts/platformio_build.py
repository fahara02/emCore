#!/usr/bin/env python3
"""
PlatformIO build script for emCore library
This script automatically generates task configuration when users build projects using emCore
"""

import os
import sys
import subprocess
from pathlib import Path

# Import PlatformIO build environment
Import("env")

print("🚀 emCore: Starting task generation check...")
print("🔥 PLATFORMIO BUILD SCRIPT IS RUNNING!")
print(f"🔥 PROJECT_DIR: {env.get('PROJECT_DIR')}")
print(f"🔥 BUILD_DIR: {env.get('BUILD_DIR')}")

def generate_tasks_if_needed():
    """Generate task configuration if tasks.yaml exists in user project"""
    
    # Get the project directory (where user's platformio.ini is)
    project_dir = Path(env.get("PROJECT_DIR"))
    
    # Look for tasks.yaml in user's project
    tasks_yaml = project_dir / "tasks.yaml"
    if not tasks_yaml.exists():
        print("📝 emCore: No tasks.yaml found, skipping task generation")
        return
    
    # Force touch the YAML file to ensure proper timestamp detection
    tasks_yaml.touch(exist_ok=True)
    
    # Path to generated file in user's src directory
    generated_file = project_dir / "src" / "generated_tasks.hpp"
    
    # Check if we need to regenerate (tasks.yaml is newer than generated file)
    if generated_file.exists():
        tasks_mtime = tasks_yaml.stat().st_mtime
        generated_mtime = generated_file.stat().st_mtime
        print(f"🕐 YAML timestamp: {tasks_mtime}, Generated timestamp: {generated_mtime}")
        if tasks_mtime <= generated_mtime:
            print("📝 emCore: generated_tasks.hpp is up to date")
            return
        else:
            print("🔄 emCore: YAML is newer, regenerating...")
            # Clear any cached build artifacts
            cache_dir = project_dir / "cache"
            if cache_dir.exists():
                print("🗑️ Clearing build cache to force regeneration...")
                import shutil
                try:
                    shutil.rmtree(cache_dir)
                    print("✅ Cache cleared")
                except Exception as e:
                    print(f"⚠️ Could not clear cache: {e}")
    
    print("=" * 60)
    print("🚀 emCore: GENERATING TASK CONFIGURATION...")
    print("=" * 60)
    
    # Find the emCore library directory
    # In PlatformIO context, __file__ might not be available
    try:
        lib_dir = Path(__file__).parent.parent
    except NameError:
        # Fallback: find emCore in libdeps
        libdeps_dir = project_dir / ".pio" / "libdeps"
        lib_dir = None
        if libdeps_dir.exists():
            for env_dir in libdeps_dir.iterdir():
                if env_dir.is_dir():
                    emcore_dir = env_dir / "emCore"
                    if emcore_dir.exists():
                        lib_dir = emcore_dir
                        break
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
        
        result = subprocess.run([
            sys.executable, 
            str(generator_script)
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

# Run generation immediately when script loads (before any compilation starts)
print("🔥 emCore: Running task generation NOW...")
try:
    generate_tasks_if_needed()
    print("✅ emCore: Task generation completed!")
except Exception as e:
    print(f"❌ emCore: Task generation failed: {e}")

# Also register callbacks for future builds
env.AddPreAction("buildprog", generate_tasks_if_needed)
env.AddPreAction("upload", generate_tasks_if_needed)

print("📦 emCore library loaded - automatic task generation enabled")
