#!/usr/bin/env python3
"""
Setup script for cmdset Python extension
"""

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import os
import platform
import shutil

# Detect dependency paths
def get_include_dirs():
    # Get absolute path to the cmdset root directory where cmdset.h is located
    current_dir = os.path.dirname(os.path.abspath(__file__))
    cmdset_root = os.path.dirname(os.path.dirname(current_dir))  # Go up two levels from dist/python to cmdset root
    include_dirs = [cmdset_root]  # C files in cmdset root directory
    # Detect operating system
    system = platform.system().lower()
    if system == 'darwin':  # macOS
        # Homebrew paths
        homebrew_openssl = '/opt/homebrew/opt/openssl@3'
        if os.path.exists(homebrew_openssl):
            include_dirs.append(f'{homebrew_openssl}/include')
        homebrew_jsonc = '/opt/homebrew/Cellar/json-c'
        if os.path.exists(homebrew_jsonc):
            versions = [d for d in os.listdir(homebrew_jsonc) if os.path.isdir(os.path.join(homebrew_jsonc, d))]
            if versions:
                latest_version = sorted(versions)[-1]
                jsonc_path = os.path.join(homebrew_jsonc, latest_version)
                include_dirs.append(f'{jsonc_path}/include')
    elif system == 'linux':
        include_dirs.extend(['/usr/include', '/usr/local/include'])
    return include_dirs

def get_library_dirs():
    library_dirs = []
    system = platform.system().lower()
    if system == 'darwin': # macOS
        homebrew_openssl = '/opt/homebrew/opt/openssl@3'
        if os.path.exists(homebrew_openssl):
            library_dirs.append(f'{homebrew_openssl}/lib')
        homebrew_jsonc = '/opt/homebrew/Cellar/json-c'
        if os.path.exists(homebrew_jsonc):
            versions = [d for d in os.listdir(homebrew_jsonc) if os.path.isdir(os.path.join(homebrew_jsonc, d))]
            if versions:
                latest_version = sorted(versions)[-1]
                jsonc_path = os.path.join(homebrew_jsonc, latest_version)
                library_dirs.append(f'{jsonc_path}/lib')
    elif system == 'linux':
        library_dirs.extend(['/usr/lib', '/usr/local/lib'])
    return library_dirs

class CustomBuildExt(build_ext):
    def build_extensions(self):
        # Ensure build directory exists with proper permissions
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp, mode=0o755)
        else:
            os.chmod(self.build_temp, 0o755)
        
        # For macOS universal builds, we need to handle the compilation differently
        if platform.system().lower() == 'darwin':
            # Try building for current architecture only first
            import platform as plat
            machine = plat.machine()
            if machine == 'arm64':
                os.environ['ARCHFLAGS'] = '-arch arm64'
            elif machine == 'x86_64':
                os.environ['ARCHFLAGS'] = '-arch x86_64'
            else:
                os.environ['ARCHFLAGS'] = '-arch arm64 -arch x86_64'
            os.chmod(self.build_temp, 0o755)
            lib_dir = os.path.join(self.build_lib, 'cmdset_python')
            if not os.path.exists(lib_dir):
                os.makedirs(lib_dir, mode=0o755)
            else:
                os.chmod(lib_dir, 0o755)
        super().build_extensions()

current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(os.path.dirname(current_dir)) # Go up two levels from dist/python to cmdset root

cmdset_extension = Extension(
    'cmdset',
    sources=[os.path.join(parent_dir, 'wrappers/python/cmdset_python_wrapper.c'), 
        os.path.join(parent_dir, 'cmdset.c')],
    include_dirs=get_include_dirs(),
    libraries=['crypto', 'ssl', 'json-c'],
    library_dirs=get_library_dirs(),
    extra_compile_args=['-std=c99', '-Wall', '-Wextra'],
    extra_link_args=[],
    define_macros=[('PY_SSIZE_T_CLEAN', None)]
)

setup(
    name='cmdset',
    version='1.0.0',
    description='Command Preset Manager - Python Extension',
    long_description='''
    CmdSet Python Extension
    
    A Python extension for the CmdSet command preset manager.
    Allows you to save, manage, and execute command presets directly from Python.
    ''',
    author='Eneko Galan',
    ext_modules=[cmdset_extension],
    python_requires='>=3.6',
    packages=['cmdset_python'],
    package_dir={'cmdset_python': '.'},
    zip_safe=False,
    install_requires=[], # No external dependencies
    dependency_links=[], # No dependency links
)