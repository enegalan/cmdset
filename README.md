# CmdSet Python Extension

A Python extension for CmdSet that allows you to use the full functionality of CmdSet directly from Python as an importable library.

## Quick Installation

```bash
# Build Python extension (from root)
python3 setup.py build_ext --inplace

# Test
python3 -c "import cmdset; print('It works!')"
```

## Basic Usage

```python
import cmdset

# Initialize
manager = cmdset.init()

# Add preset
cmdset.add_preset(manager, "hello", "echo 'Hello World'")

# List presets
presets = cmdset.list_presets(manager)

# Execute preset
result = cmdset.execute_preset(manager, "hello")

# Cleanup
cmdset.cleanup(manager)
```

## Advanced Usage

```python
from cmdset_python import CmdSet

with CmdSet() as cmdset:
    cmdset.add('hello', 'echo "Hello World"')
    result = cmdset.execute('hello')
```

## Complete Documentation

See `cmdset_python/README.md` for detailed documentation, advanced examples and complete API.
