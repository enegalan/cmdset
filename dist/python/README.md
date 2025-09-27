# CmdSet Python Extension

A Python extension for CmdSet that allows you to use the full functionality of CmdSet directly from Python as an importable library.

## Features

- **Direct import**: `import cmdset` - Use CmdSet like any Python library
- **Native Python interface**: High-level API designed for Python
- **Full functionality**: All CmdSet features available
- **Encryption**: Complete support for encrypted commands
- **Cross-platform**: Works on macOS, Linux and Windows
- **Context Manager**: Support for `with` statements
- **Type Hints**: Fully typed for better IDE support

## Installation

### Prerequisites

Make sure you have the system dependencies installed:

#### macOS
```bash
# With Homebrew
brew install openssl@3 json-c

# With MacPorts
sudo port install openssl3 json-c
```

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get install libssl-dev libjson-c-dev
```

#### Linux (CentOS/RHEL/Fedora)
```bash
# CentOS/RHEL
sudo yum install openssl-devel json-c-devel

# Fedora
sudo dnf install openssl-devel json-c-devel
```

### Build and Install

```bash
# 1. Build the C library
python3 setup.py build_ext --inplace

# 2. Test installation
python3 -c "import cmdset; print('CmdSet installed successfully!')"
```

## Basic Usage

### Import and Configuration

```python
from cmdset_python import CmdSet, Preset, CmdSetError

# Create CmdSet instance
with CmdSet() as cmdset:
    # Your code here
    pass
```

### Add Presets

```python
# Simple preset
cmdset.add('hello', 'echo "Hello World"')

# Encrypted preset
cmdset.add('secret', 'echo "Secret command"', encrypt=True)

# Preset with dynamic arguments
cmdset.add('grep-files', 'grep -r')
```

### Execute Presets

```python
# Execute simple preset
result = cmdset.execute('hello')

# Execute preset with additional arguments
result = cmdset.execute('grep-files', 'import', '.')

# The result is the command exit code
print(f"Command exited with code: {result}")
```

### List and Search Presets

```python
# List all presets
presets = cmdset.list_presets()
for preset in presets:
    print(f"{preset.name}: {preset.command}")

# Find specific preset
preset = cmdset.find('hello')
if preset:
    print(f"Found: {preset.name}")

# Check existence
if 'hello' in cmdset:
    print("Preset exists")

# Iterate over presets
for preset in cmdset:
    print(f"{preset.name}: {preset.command}")
```

### File Management

```python
# Save presets to disk
cmdset.save()

# Load presets from disk
cmdset.load()

# Export to JSON file
cmdset.export('backup.json')

# Import from JSON file
cmdset.import_presets('backup.json')
```

## Detailed API

### CmdSet Class

```python
class CmdSet:
    def __init__(self, working_dir: Optional[str] = None)
    def add(self, name: str, command: str, encrypt: bool = False) -> bool
    def remove(self, name: str) -> bool
    def execute(self, name: str, *args) -> int
    def list_presets(self) -> List[Preset]
    def find(self, name: str) -> Optional[Preset]
    def save(self) -> bool
    def load(self) -> bool
    def export(self, filename: str) -> bool
    def import_presets(self, filename: str) -> bool
    def encrypt_command(self, plaintext: str) -> str
    def decrypt_command(self, encrypted: str) -> str
    def close(self)
```

### Preset Class

```python
@dataclass
class Preset:
    name: str
    command: str
    encrypt: bool = False
    created_at: int = 0
    last_used: int = 0
    use_count: int = 0
    
    @property
    def is_encrypted(self) -> bool
    @property
    def age_days(self) -> int
    @property
    def days_since_last_use(self) -> int
```

### Exceptions

```python
class CmdSetError(Exception): pass
class PresetError(CmdSetError): pass
class EncryptionError(CmdSetError): pass
class FileError(CmdSetError): pass
```

## Advanced Examples

### Context Manager

```python
# Use with context manager (recommended)
with CmdSet('/path/to/project') as cmdset:
    cmdset.add('test', 'python -m pytest')
    result = cmdset.execute('test')
    print(f"Tests completed with code: {result}")
# Resources are automatically cleaned up
```

### Error Handling

```python
try:
    with CmdSet() as cmdset:
        cmdset.add('existing', 'echo "test"')
        cmdset.add('existing', 'echo "duplicate"')  # Error!
except PresetError as e:
    print(f"Preset error: {e}")
except CmdSetError as e:
    print(f"CmdSet error: {e}")
```

### Encryption

```python
with CmdSet() as cmdset:
    # Add encrypted command
    cmdset.add('secret', 'echo "sensitive data"', encrypt=True)
    
    # When executing, it will ask for master password
    result = cmdset.execute('secret')
    
    # Encrypted commands are shown as [ENCRYPTED]
    presets = cmdset.list_presets()
    for preset in presets:
        if preset.encrypt:
            print(f"{preset.name}: [ENCRYPTED]")
```

### Usage Statistics

```python
with CmdSet() as cmdset:
    # Execute some commands
    cmdset.execute('hello')
    cmdset.execute('hello')
    
    # Get statistics
    presets = cmdset.list_presets()
    for preset in presets:
        print(f"{preset.name}:")
        print(f"  Created: {time.ctime(preset.created_at)}")
        print(f"  Last used: {time.ctime(preset.last_used) if preset.last_used else 'Never'}")
        print(f"  Use count: {preset.use_count}")
        print(f"  Age: {preset.age_days} days")
```

### Backup and Restore

```python
# Full backup
with CmdSet() as cmdset:
    cmdset.export('full_backup.json')
    print("Backup created")

# Restore from backup
with CmdSet() as cmdset:
    cmdset.import_presets('full_backup.json')
    print("Backup restored")
```

## CLI Interface

A command-line interface is also included:

```bash
# Use cmdset-py as command
cmdset-py add hello "echo 'Hello World'"
cmdset-py list
cmdset-py exec hello
cmdset-py remove hello
cmdset-py export backup.json
cmdset-py import backup.json
```

## Development

### Project Structure

```
cmdset/
├── cmdset.h              # C header
├── cmdset_core.c         # C implementation
├── cmdset_python_wrapper.c # Python wrapper
├── setup.py              # Python setup
├── cmdset_python/        # Python module
│   ├── __init__.py
│   ├── cmdset.py        # Python wrapper
│   └── cli.py           # CLI interface
└── example.py           # Usage example
```

### Manual Build

```bash
# Build C library
python3 setup.py build_ext --inplace

# Install
pip install -e .
```

### Testing

```bash
# Run example
python3 example.py

# Test CLI
cmdset-py list
```

## Troubleshooting

### Error: "cmdset extension not found"

```bash
# Rebuild and install
python3 setup.py build_ext --inplace
```

### Error: "OpenSSL not found"

```bash
# macOS
brew install openssl@3

# Linux
sudo apt-get install libssl-dev
```

### Error: "json-c not found"

```bash
# macOS
brew install json-c

# Linux
sudo apt-get install libjson-c-dev
```

## Differences with Original CmdSet

| Feature | Original CmdSet | Python Extension |
|---------|------------------|------------------|
| Usage | `cmdset add name "command"` | `cmdset.add('name', 'command')` |
| Installation | `make && sudo make install` | `python3 setup.py build_ext --inplace` |
| Import | N/A | `import cmdset` |
| API | CLI only | Python API + CLI |
| Integration | Subprocess | Native Python |
| Type Safety | N/A | Full type hints |

## Advantages of Python Extension

1. **Native integration**: No subprocess required
2. **Python API**: Designed for Python developers
3. **Type Safety**: Fully typed
4. **Context Manager**: Automatic resource management
5. **Exception Handling**: Native Python error handling
6. **IDE Support**: Autocompletion and documentation
7. **Testing**: Easy to test with pytest
8. **Packaging**: Distribution with pip

## Contributing

1. Fork the repository
2. Create feature branch
3. Make changes
4. Test with `python3 example.py`
5. Commit and push
6. Create Pull Request

## License

Same license as original CmdSet.