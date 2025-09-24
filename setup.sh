#!/bin/bash

# CmdSet Setup Script
# This script installs or uninstalls cmdset globally

set -e  # Exit on any error

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

BINARY_NAME="cmdset"
INSTALL_DIR="/usr/local/bin"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY_PATH="$SCRIPT_DIR/$BINARY_NAME"
SYMLINK_PATH="$INSTALL_DIR/$BINARY_NAME"

print_info() {
    printf "${BLUE}[INFO]${NC} %s\n" "$1"
}

print_success() {
    printf "${GREEN}[SUCCESS]${NC} %s\n" "$1"
}

print_warning() {
    printf "${YELLOW}[WARNING]${NC} %s\n" "$1"
}

print_error() {
    printf "${RED}[ERROR]${NC} %s\n" "$1"
}

check_root() {
    if [[ $EUID -ne 0 ]]; then
        print_error "This script must be run as root (use sudo)"
        exit 1
    fi
}

check_binary() {
    if [[ ! -f "$BINARY_PATH" ]]; then
        print_error "Binary $BINARY_NAME not found at $BINARY_PATH"
        print_info "Please run 'make' first to build the binary"
        exit 1
    fi
    
    if [[ ! -x "$BINARY_PATH" ]]; then
        print_error "Binary $BINARY_NAME is not executable"
        exit 1
    fi
}

check_install_dir() {
    if [[ ! -d "$INSTALL_DIR" ]]; then
        print_error "Install directory $INSTALL_DIR does not exist"
        exit 1
    fi
}

install_cmdset() {
    print_info "Installing $BINARY_NAME globally..."
    # Check if already installed
    if [[ -L "$SYMLINK_PATH" ]]; then
        print_warning "$BINARY_NAME is already installed"
        read -p "Do you want to reinstall? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_info "Installation cancelled"
            exit 0
        fi
        print_info "Removing existing installation..."
        rm "$SYMLINK_PATH"
    fi
    # Create symlink
    print_info "Creating symlink from $BINARY_PATH to $SYMLINK_PATH"
    ln -s "$BINARY_PATH" "$SYMLINK_PATH"
    # Verify installation
    if [[ -L "$SYMLINK_PATH" ]] && [[ -x "$SYMLINK_PATH" ]]; then
        print_success "$BINARY_NAME installed successfully!"
        print_info "You can now use '$BINARY_NAME' from any directory"
        print_info "Try: $BINARY_NAME help"
    else
        print_error "Installation failed"
        exit 1
    fi
}

uninstall_cmdset() {
    print_info "Uninstalling $BINARY_NAME..."
    if [[ ! -L "$SYMLINK_PATH" ]]; then
        print_warning "$BINARY_NAME is not installed"
        exit 0
    fi
    # Check if it's our symlink
    if [[ "$(readlink "$SYMLINK_PATH")" == "$BINARY_PATH" ]]; then
        print_info "Removing symlink from $SYMLINK_PATH"
        rm "$SYMLINK_PATH"
        print_success "$BINARY_NAME uninstalled successfully!"
    else
        print_warning "Found symlink at $SYMLINK_PATH but it doesn't point to our binary"
        print_warning "This might be installed by another package"
        read -p "Do you want to remove it anyway? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            rm "$SYMLINK_PATH"
            print_success "Symlink removed"
        else
            print_info "Uninstall cancelled"
        fi
    fi
}

show_status() {
    print_info "Checking $BINARY_NAME installation status..."
    if [[ -L "$SYMLINK_PATH" ]]; then
        if [[ "$(readlink "$SYMLINK_PATH")" == "$BINARY_PATH" ]]; then
            print_success "$BINARY_NAME is installed and working correctly"
            print_info "Location: $SYMLINK_PATH -> $BINARY_PATH"
        else
            print_warning "$BINARY_NAME symlink exists but points to different location"
            print_info "Current symlink: $SYMLINK_PATH -> $(readlink "$SYMLINK_PATH")"
            print_info "Expected: $BINARY_PATH"
        fi
    else
        print_info "$BINARY_NAME is not installed"
    fi
}

show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "$0  install     Install cmdset globally (requires sudo)"
    echo "$0  uninstall   Uninstall cmdset (requires sudo)"
    echo "$0  status      Show installation status"
    echo "$0  help        Show this help message"
}

main() {
    case "${1:-}" in
        "install")
            check_root
            check_binary
            check_install_dir
            install_cmdset
            ;;
        "uninstall")
            check_root
            uninstall_cmdset
            ;;
        "status")
            show_status
            ;;
        "help"|"-h"|"--help")
            show_usage
            ;;
        "")
            print_error "No command specified"
            show_usage
            exit 1
            ;;
        *)
            print_error "Unknown command: $1"
            show_usage
            exit 1
            ;;
    esac
}

main "$@"
