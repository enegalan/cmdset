#!/usr/bin/env python3
"""
Build script for CmdSet Python extension
Runs setup.py from the parent directory
"""

import os
import sys
import subprocess

def main():
    current_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(current_dir)
    cmd = [sys.executable, 'setup.py'] + sys.argv[1:]
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd)
    sys.exit(result.returncode)

if __name__ == '__main__':
    main()
