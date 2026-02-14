#!/bin/bash
# Light cleanup - just fix broken CUDA packages, keep working driver

set -e

echo "=== NVIDIA Light Cleanup (Keep Driver) ==="
echo ""

# 1. Kill hung processes
sudo killall -9 apt apt-get dpkg 2>/dev/null || true
sleep 2

# 2. Remove locks
sudo rm -f /var/lib/dpkg/lock-frontend /var/lib/dpkg/lock /var/cache/apt/archives/lock /var/lib/apt/lists/lock

# 3. Force remove only CUDA packages (not driver)
echo "Removing broken CUDA packages..."
sudo dpkg --remove --force-all $(dpkg -l | grep '^iU' | grep cuda | awk '{print $2}') 2>/dev/null || true

# 4. Clean and update
echo "Cleaning package cache..."
sudo apt-get clean
sudo rm -rf /var/lib/apt/lists/*
sudo mkdir -p /var/lib/apt/lists/partial
sudo apt-get update

# 5. Fix dependencies
echo "Fixing broken dependencies..."
sudo dpkg --configure -a
sudo apt-get install -f -y

# 6. Remove leftover CUDA packages
echo "Removing CUDA packages..."
sudo apt-get autoremove -y --purge 'cuda-*' 2>/dev/null || true

# 7. Now install CUDA toolkit cleanly
echo "Installing CUDA toolkit..."
sudo apt-get install -y nvidia-cuda-toolkit

echo ""
echo "Done! Verify with: nvcc --version"
