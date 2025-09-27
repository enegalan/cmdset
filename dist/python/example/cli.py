"""
CmdSet CLI Interface

Command-line interface for CmdSet Python extension.
"""

import sys
import os
import argparse
from typing import List, Optional

# Add parent directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from cmdset import CmdSet, CmdSetError, PresetError, FileError

def main():
    """Main CLI function"""
    parser = argparse.ArgumentParser(
        description='CmdSet - Command Preset Manager',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  cmdset-py add my-command "ls -la"
  cmdset-py add -e secret "echo 'secret'"
  cmdset-py exec my-command
  cmdset-py list
  cmdset-py remove my-command
  cmdset-py export backup.json
  cmdset-py import backup.json
        '''
    )
    
    parser.add_argument('--working-dir', '-d', 
        help='Working directory for presets')
    
    subparsers = parser.add_subparsers(dest='command', help='Available commands')
    
    # Add command
    add_parser = subparsers.add_parser('add', help='Add a new preset')
    add_parser.add_argument('name', help='Preset name')
    add_parser.add_argument('cmd', help='Command to save')
    add_parser.add_argument('-e', '--encrypt', action='store_true',
        help='Encrypt the command')
    
    # Remove command
    remove_parser = subparsers.add_parser('remove', help='Remove a preset')
    remove_parser.add_argument('name', help='Preset name to remove')
    
    # List command
    list_parser = subparsers.add_parser('list', help='List all presets')
    list_parser.add_argument('--json', action='store_true',
                            help='Output in JSON format')
    
    # Execute command
    exec_parser = subparsers.add_parser('exec', help='Execute a preset')
    exec_parser.add_argument('name', help='Preset name to execute')
    exec_parser.add_argument('args', nargs='*', help='Additional arguments')
    
    # Export command
    export_parser = subparsers.add_parser('export', help='Export presets to file')
    export_parser.add_argument('filename', help='Export filename')
    
    # Import command
    import_parser = subparsers.add_parser('import', help='Import presets from file')
    import_parser.add_argument('filename', help='Import filename')
    
    # Save command
    save_parser = subparsers.add_parser('save', help='Save presets to disk')
    
    # Load command
    load_parser = subparsers.add_parser('load', help='Load presets from disk')
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        return 1
    
    try:
        with CmdSet(working_dir=args.working_dir) as cmdset:
            return handle_command(cmdset, args)
    
    except CmdSetError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\nOperation cancelled", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"Unexpected error: {e}", file=sys.stderr)
        return 1

def handle_command(cmdset: CmdSet, args) -> int:
    """Handle specific commands"""
    command = getattr(args, 'command', None)
    if command == 'add':
        try:
            cmdset.add(args.name, args.cmd, encrypt=args.encrypt)
            print(f"Preset '{args.name}' added successfully")
            return 0
        except PresetError as e:
            print(f"Error: {e}", file=sys.stderr)
            return 1
    
    elif command == 'remove':
        try:
            cmdset.remove(args.name)
            print(f"Preset '{args.name}' removed successfully")
            return 0
        except PresetError as e:
            print(f"Error: {e}", file=sys.stderr)
            return 1
    
    elif command == 'list':
        try:
            presets = cmdset.list_presets()
            if args.json:
                import json
                preset_data = []
                for preset in presets:
                    preset_data.append({
                        'name': preset.name,
                        'command': '[ENCRYPTED]' if preset.encrypt else preset.command,
                        'encrypt': preset.encrypt,
                        'created_at': preset.created_at,
                        'last_used': preset.last_used,
                        'use_count': preset.use_count
                    })
                print(json.dumps(preset_data, indent=2))
            else:
                if not presets:
                    print("No presets found")
                else:
                    print("Presets:")
                    print("--------")
                    for i, preset in enumerate(presets, 1):
                        command_display = "[ENCRYPTED]" if preset.encrypt else preset.command
                        print(f"{i}. {preset.name}: {command_display}")
                    print(f"\nTotal: {len(presets)} preset(s)")
            return 0
        except CmdSetError as e:
            print(f"Error: {e}", file=sys.stderr)
            return 1
    
    elif command == 'exec':
        try:
            result = cmdset.execute(args.name, *args.args)
            return result
        except PresetError as e:
            print(f"Error: {e}", file=sys.stderr)
            return 1
    
    elif command == 'export':
        try:
            cmdset.export(args.filename)
            print(f"Presets exported to '{args.filename}'")
            return 0
        except FileError as e:
            print(f"Error: {e}", file=sys.stderr)
            return 1
    
    elif command == 'import':
        try:
            cmdset.import_presets(args.filename)
            print(f"Presets imported from '{args.filename}'")
            return 0
        except FileError as e:
            print(f"Error: {e}", file=sys.stderr)
            return 1
    
    elif command == 'save':
        try:
            cmdset.save()
            print("Presets saved successfully")
            return 0
        except FileError as e:
            print(f"Error: {e}", file=sys.stderr)
            return 1
    
    elif command == 'load':
        try:
            cmdset.load()
            print("Presets loaded successfully")
            return 0
        except FileError as e:
            print(f"Error: {e}", file=sys.stderr)
            return 1
    
    else:
        print(f"Unknown command: {args.command}", file=sys.stderr)
        return 1

if __name__ == '__main__':
    sys.exit(main())
