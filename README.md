# CmdSet - Command Preset Manager

CmdSet is a simple CLI tool written in C that allows you to save frequently used commands as presets and execute them with shorter commands. This eliminates the need to type long commands repeatedly.

## Features

- **Add Presets**: Save any command with a short name
- **Execute Presets**: Run saved commands with simple names
- **List Presets**: View all saved command presets
- **Remove Presets**: Delete presets you no longer need
- **Persistent Storage**: Presets are saved to a file and persist between sessions
- **Usage Statistics**: Track when presets were created and how often they're used
- **Command Shortcuts**: Use short abbreviations for faster command entry
- **JSON Format**: Modern, human-readable storage format
- **Special Character Support**: Safely handles pipes, ampersands, and other special characters
- **🔐 Encryption Support**: Encrypt sensitive commands with AES-256 encryption
- **🔑 Master Password**: Secure master password protection for encrypted presets
- **📤 Export/Import**: Backup and restore presets with JSON export/import
- **🛡️ Cross-Platform**: Works on macOS, Linux, and Windows

## Building

### Prerequisites

CmdSet requires OpenSSL for encryption features. See [INSTALL.md](INSTALL.md) for detailed installation instructions for your platform.

### Quick Build

```bash
make
```

This will create the `cmdset` executable.

### Cross-Platform Support

The Makefile automatically detects your operating system and configures the build accordingly:

- **macOS**: Uses Homebrew or MacPorts OpenSSL
- **Linux**: Uses system OpenSSL or pkg-config
- **Windows**: Uses MSYS2, vcpkg, or manual OpenSSL installation

## Installation

### Global Installation (Recommended)

To install cmdset globally so you can use it from any directory:

```bash
# Using the setup script (recommended)
sudo ./setup.sh install

# Or using make
make install

# Check installation status
./setup.sh status
# or
make status
```

### Uninstallation

To remove the global installation:

```bash
# Using the setup script
sudo ./setup.sh uninstall

# Or using make
make uninstall
```

### Manual Installation

If you prefer manual installation:

```bash
# Create symlink
sudo ln -s $(pwd)/cmdset /usr/local/bin/cmdset

# Remove symlink
sudo rm /usr/local/bin/cmdset
```

## Usage

### Basic Commands

```bash
# Show help
cmdset help
cmdset h                    # Short version

# Add a new preset
cmdset add <name> <command>
cmdset a <name> <command>   # Short version

# Add an encrypted preset
cmdset add -e <name> <command>
cmdset add --encrypt <name> <command>
cmdset a -e <name> <command>        # Short version

# Execute a preset
cmdset exec <name> [args...]
cmdset e <name> [args...]             # Short version
cmdset run <name> [args...]           # Alternative short version

# List all presets
cmdset list
cmdset ls                   # Short version

# Remove a preset
cmdset remove <name>
cmdset rm <name>            # Short version

# Export presets to file
cmdset export [filename]
cmdset exp [filename]       # Short version

# Import presets from file
cmdset import [filename]
cmdset imp [filename]       # Short version
```

### Command Shortcuts

CmdSet supports convenient shortcuts for all commands:

| Full Command | Shortcut | Alternative | Description |
|--------------|----------|-------------|-------------|
| `help` | `h` | - | Show help |
| `add` | `a` | - | Add preset |
| `add -e` | `a -e` | `add --encrypt` | Add encrypted preset |
| `remove` | `rm` | - | Remove preset |
| `list` | `ls` | - | List presets |
| `exec` | `e` | `run` | Execute preset |
| `export` | `exp` | - | Export presets |
| `import` | `imp` | - | Import presets |

### Examples

```bash
# Add a git status preset
cmdset add git-status "git status --porcelain"

# Add a complex command with multiple options
cmdset add backup "tar -czf backup-$(date +%Y%m%d).tar.gz /important/data"

# Add sequential commands
cmdset add server-status "ssh user@server 'systemctl status nginx && systemctl status mysql'"

# Add encrypted commands for sensitive operations
cmdset add -e db-backup "mysqldump -u root -p'secret123' mydb > backup.sql"
cmdset add --encrypt api-deploy "curl -H 'Authorization: Bearer token123' -X POST https://api.example.com/deploy"

# Execute the git status preset
cmdset exec git-status

# Execute encrypted preset (prompts for master password)
cmdset exec db-backup

# List all presets
cmdset list

# Remove a preset
cmdset remove git-status

# Export all presets to a backup file
cmdset export backup.json

# Import presets from a backup file
cmdset import backup.json

# Interactive export (prompts for filename)
cmdset export
# Enter export file path: my_presets.json

# Interactive import (prompts for filename)
cmdset import
# Enter import file path: my_presets.json
```

### Dynamic Arguments

CmdSet supports adding arguments to presets at execution time. This is particularly useful for database commands and other tools that accept dynamic parameters:

```bash
# Add a base database command
cmdset add mariadb-query "mariadb -e"

# Execute with dynamic SQL query
cmdset exec mariadb-query "SHOW TABLES;"
# This executes: mariadb -e "SHOW TABLES;"

# Add a base MySQL command
cmdset add mysql-query "mysql -e"

# Execute with different queries
cmdset exec mysql-query "SELECT COUNT(*) FROM users;"
cmdset exec mysql-query "DESCRIBE products;"

# Works with any command that accepts additional arguments
cmdset add ls-dir "ls -la"
cmdset exec ls-dir "/path/to/directory"
# This executes: ls -la /path/to/directory
```

### Encrypted Commands

For sensitive commands containing passwords, API keys, or other confidential information:

```bash
# Add an encrypted preset
cmdset add -e db-backup "mysqldump -u root -p'secret123' mydb > backup.sql"
cmdset add --encrypt api-call "curl -H 'Authorization: Bearer token123' https://api.example.com"

# List presets (encrypted commands show as [ENCRYPTED])
cmdset list

# Execute encrypted preset (prompts for master password)
cmdset exec db-backup
```

**Security Features:**
- **AES-256-CBC** encryption with random salt and IV
- **PBKDF2** key derivation with 10,000 iterations
- **Master password** protection
- **Memory clearing** after decryption
- **Base64 encoding** for safe JSON storage

### Import/Export

CmdSet supports backing up and restoring presets using JSON export/import functionality.

**Export Features:**
- Exports all active presets with full metadata
- Preserves encryption status and usage statistics
- Creates human-readable JSON format
- Includes export timestamp

**Import Features:**
- Validates JSON file format before importing
- Skips duplicate presets with clear warnings
- Preserves all preset metadata
- Handles conflicts gracefully
- Reports import statistics

### Testing

Run the test suite to verify everything works:

```bash
make test
```

## How It Works

- Presets are stored in a hidden file `.cmdset_presets` in the current directory
- Each preset consists of a name and the full command to execute
- Commands are executed using the system shell
- The program supports up to 100 presets with names up to 50 characters and commands up to 500 characters
- **Usage tracking**: The system tracks when presets were created, last used, and how many times they've been executed

## File Format

The preset file uses a modern JSON format that safely handles special characters and includes metadata:

```json
{
  "version": "2.0",
  "presets": [
    {
      "name": "command1",
      "command": "git status --porcelain",
      "encrypt": false,
      "created_at": 1758749561,
      "last_used": 1758749600,
      "use_count": 5
    },
    {
      "name": "command2",
      "command": "docker build -t myapp .",
      "encrypt": false,
      "created_at": 1758749500,
      "last_used": 0,
      "use_count": 0
    },
    {
      "name": "secret-command",
      "command": "U2FsdGVkX1+vupppZksvRf5pq5g5XjFRlipRkwB0K1Y=",
      "encrypt": true,
      "created_at": 1758749600,
      "last_used": 0,
      "use_count": 0
    }
  ]
}
```

### Export Format

When exporting presets, the JSON includes additional metadata:

```json
{
  "version": "2.0",
  "exported_at": 1758833494,
  "presets": [
    {
      "name": "git-status",
      "command": "git status --porcelain",
      "encrypt": false,
      "created_at": 1758749561,
      "last_used": 1758749600,
      "use_count": 5
    }
  ]
}
```

The export format is fully compatible with the import functionality and can be used to backup, restore, or share presets between different systems.

## Error Handling

The program includes comprehensive error handling for:
- Invalid command line arguments
- Preset name conflicts
- File I/O errors
- Command execution failures
- JSON parsing errors
- Memory allocation failures

## Limitations

- Maximum 100 presets
- Preset names limited to 50 characters
- Commands limited to 500 characters
- JSON file must be manually edited with care (use `cmdset` commands when possible)
- Encrypted commands require OpenSSL to be installed
- Master password is not stored and must be entered each time an encrypted command is executed
- Cross-platform compatibility requires OpenSSL installation on target system