#!/bin/bash
# NVIDIA/CUDA Recovery Script
# Fixes broken apt/dpkg from NVIDIA runfile installer

set -x  # Show commands as they run

echo "=== NVIDIA/CUDA Recovery Script ==="
echo ""

# 1. Kill any hanging apt/dpkg processes
echo "Step 1: Killing hung package manager processes..."
sudo killall -9 apt apt-get dpkg 2>/dev/null || true
sleep 2

# 2. Remove lock files
echo "Step 2: Removing lock files..."
sudo rm -f /var/lib/dpkg/lock-frontend
sudo rm -f /var/lib/dpkg/lock
sudo rm -f /var/cache/apt/archives/lock
sudo rm -f /var/lib/apt/lists/lock

# 3. Backup dpkg status
echo "Step 3: Backing up dpkg database..."
sudo cp /var/lib/dpkg/status /var/lib/dpkg/status.backup-$(date +%Y%m%d-%H%M%S)

# 4. Remove broken CUDA packages from dpkg database
echo "Step 4: Removing CUDA packages from dpkg database..."
sudo dpkg --remove --force-all $(dpkg -l | grep '^iU' | grep cuda | awk '{print $2}') 2>/dev/null || true

# 5. Clean up package cache
echo "Step 5: Cleaning package cache..."
sudo apt-get clean
sudo rm -rf /var/lib/apt/lists/*
sudo mkdir -p /var/lib/apt/lists/partial

# 6. Rebuild package lists
echo "Step 6: Rebuilding package lists..."
sudo apt-get update

# 7. Fix broken dependencies
echo "Step 7: Fixing broken dependencies..."
sudo dpkg --configure -a
sudo apt-get install -f -y

# 8. Remove any leftover CUDA packages
echo "Step 8: Removing leftover CUDA packages..."
sudo apt-get autoremove -y --purge 'cuda-*' 'nvidia-cuda-*' 2>/dev/null || true

# 9. Final cleanup
echo "Step 9: Final cleanup..."
sudo apt-get autoremove -y
sudo apt-get autoclean

echo ""
echo "=== Recovery Complete ==="
echo ""
echo "Your NVIDIA driver (590.48.01) should still be intact."
echo "To install CUDA toolkit properly, use Ubuntu repos:"
echo "  sudo apt-get install nvidia-cuda-toolkit"
echo ""
echo "Or for newer version, add NVIDIA's official repo:"
echo "  wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2404/x86_64/cuda-keyring_1.1-1_all.deb"
echo "  sudo dpkg -i cuda-keyring_1.1-1_all.deb"
echo "  sudo apt-get update"
echo "  sudo apt-get install cuda-toolkit-13-1"
