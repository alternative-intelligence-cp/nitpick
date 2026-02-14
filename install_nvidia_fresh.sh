#!/bin/bash
# Install latest NVIDIA driver (590 series) and CUDA toolkit
# Run this AFTER rebooting with nouveau driver

set -e

echo "=== NVIDIA Fresh Install (590 series + CUDA) ==="
echo ""

# Check if nouveau is loaded
if lsmod | grep -q nouveau; then
    echo "✓ Nouveau driver is loaded (good!)"
else
    echo "✗ WARNING: Nouveau is not loaded. Did you reboot?"
    read -p "Continue anyway? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo ""

# 1. Add graphics-drivers PPA for latest drivers
echo "Step 1: Adding graphics-drivers PPA..."
sudo add-apt-repository -y ppa:graphics-drivers/ppa
sudo apt-get update

# 2. Show available NVIDIA drivers
echo ""
echo "Available NVIDIA drivers:"
ubuntu-drivers list | grep nvidia
echo ""

# 3. Install nvidia-driver-590 (or latest available)
echo "Step 2: Installing NVIDIA driver 590..."
sudo apt-get install -y nvidia-driver-590

echo ""
echo "=== Driver Installed ==="
echo ""
echo "REBOOT NOW to load the NVIDIA driver."
echo ""
read -p "Reboot now? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    sudo reboot
else
    echo ""
    echo "After reboot:"
    echo "  1. Verify driver: nvidia-smi"
    echo "  2. Install CUDA: sudo apt install nvidia-cuda-toolkit"
    echo "  3. Verify CUDA: nvcc --version"
fi
