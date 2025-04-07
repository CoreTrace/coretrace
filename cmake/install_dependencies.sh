#!/bin/bash

# Script to install dependencies for CoreTrace project
set -e

echo "Installing dependencies for CoreTrace..."

# Detect distribution
if [ -f /etc/debian_version ]; then
    # Debian/Ubuntu
    echo "Detected Debian/Ubuntu system"
    
    # Check if running as root, if not use sudo
    if [ $(id -u) -ne 0 ]; then
        SUDO="sudo"
    else
        SUDO=""
    fi
    
    echo "Updating package lists..."
    $SUDO apt-get update
    
    echo "Installing Boost..."
    $SUDO apt-get install -y libboost-all-dev
    
    # Install other dependencies needed for IKOS
    echo "Installing other dependencies..."
    $SUDO apt-get install -y libgmp-dev libtbb-dev libsqlite3-dev python3-dev
    
    # LLVM 14 might need a specific repository
    if ! dpkg -l | grep -q "llvm-14"; then
        echo "LLVM 14 not found, adding LLVM repository..."
        $SUDO apt-get install -y software-properties-common wget
        wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | $SUDO apt-key add -
        $SUDO add-apt-repository "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs)-14 main"
        $SUDO apt-get update
        $SUDO apt-get install -y llvm-14 llvm-14-dev
    fi
    
elif [ -f /etc/redhat-release ]; then
    # RHEL/CentOS/Fedora
    echo "Detected RHEL/CentOS/Fedora system"
    
    if [ $(id -u) -ne 0 ]; then
        SUDO="sudo"
    else
        SUDO=""
    fi
    
    echo "Installing Boost..."
    $SUDO dnf install -y boost-devel
    
    # Install other dependencies
    echo "Installing other dependencies..."
    $SUDO dnf install -y gmp-devel tbb-devel sqlite-devel python3-devel
    
    # LLVM 14 might need special handling
    if ! rpm -qa | grep -q "llvm14"; then
        echo "LLVM 14 installation may require manual steps. Please visit https://releases.llvm.org/download.html"
    fi
    
else
    echo "Unsupported Linux distribution. Please install dependencies manually:"
    echo "- Boost (libboost-all-dev)"
    echo "- GMP (libgmp-dev)"
    echo "- TBB (libtbb-dev)"
    echo "- SQLite (libsqlite3-dev)"
    echo "- Python3 (python3-dev)"
    echo "- LLVM 14"
    exit 1
fi

echo "Dependencies installation completed!"
exit 0
