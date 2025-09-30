# CmdSet Installation with Encryption

## System Requirements

**CmdSet** with encryption requires OpenSSL to function. Here are the installation instructions for different operating systems:

## macOS

### Option 1: Homebrew (Recommended)
```bash
# Install OpenSSL
brew install openssl@3

# Compile cmdset
make clean && make
```

### Option 2: MacPorts
```bash
# Install OpenSSL
sudo port install openssl3

# Compile cmdset
make clean && make
```

## Linux

### Ubuntu/Debian
```bash
# Install dependencies
sudo apt-get update
sudo apt-get install build-essential libssl-dev

# Compile cmdset
make clean && make
```

### CentOS/RHEL/Fedora
```bash
# CentOS/RHEL
sudo yum install gcc openssl-devel

# Fedora
sudo dnf install gcc openssl-devel

# Compile cmdset
make clean && make
```

### Arch Linux
```bash
# Install dependencies
sudo pacman -S base-devel openssl

# Compile cmdset
make clean && make
```

## Windows

### Option 1: MSYS2 (Recommended)
```bash
# Install MSYS2 from https://www.msys2.org/
# Open MSYS2 terminal

# Install dependencies
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-openssl

# Compile cmdset
make clean && make
```

### Option 2: vcpkg
```bash
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat

# Install OpenSSL
./vcpkg install openssl:x64-windows

# Compile cmdset (adjust paths according to installation)
gcc -I./vcpkg/installed/x64-windows/include -L./vcpkg/installed/x64-windows/lib -o cmdset.exe cmdset.c -lcrypto
```

### Option 3: OpenSSL Binaries
1. Download OpenSSL from https://slproweb.com/products/Win32OpenSSL.html
2. Install to `C:\OpenSSL-Win64`
3. Compile:
```cmd
gcc -I"C:\OpenSSL-Win64\include" -L"C:\OpenSSL-Win64\lib" -o cmdset.exe cmdset.c -lcrypto
```

## Installation Verification

After compiling, you can verify that encryption works:

```bash
# Test normal command
./cmdset add test "echo 'Hello World'"

# Test encrypted command
./cmdset add -e secret "echo 'Secret Command'"

# List presets
./cmdset list
```

## Troubleshooting

### Error: "openssl/evp.h: No such file or directory"
- **Linux**: Install `libssl-dev` or `openssl-devel`
- **macOS**: Install OpenSSL with Homebrew
- **Windows**: Verify that OpenSSL is installed and paths are correct

### Error: "undefined reference to `EVP_*`"
- Verify that `-lcrypto` is in the link flags
- On Windows, you may also need `-lssl`

### Error: "stty: command not found" (Windows)
- This error is normal on Windows, the program will use the Windows API instead

## Manual Compilation

If the automatic Makefile doesn't work, you can compile manually:

```bash
# Linux/macOS
gcc -Wall -Wextra -std=c99 -O2 -o cmdset cmdset.c -lcrypto

# Windows (adjust paths according to installation)
gcc -Wall -Wextra -std=c99 -O2 -o cmdset.exe cmdset.c -lcrypto
```
