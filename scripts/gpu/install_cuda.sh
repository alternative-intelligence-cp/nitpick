#!/bin/bash
# Install CUDA toolkit (run after NVIDIA driver is working)

set -e

echo "=== CUDA Toolkit Installation ==="
echo ""

# Check if nvidia-smi works
if ! command -v nvidia-smi &> /dev/null; then
    echo "ERROR: nvidia-smi not found. Install NVIDIA driver first!"
    exit 1
fi

echo "Checking NVIDIA driver status..."
nvidia-smi

echo ""
echo "Driver looks good. Installing CUDA toolkit..."
echo ""

# Install CUDA toolkit from Ubuntu repos
sudo apt-get update
sudo apt-get install -y nvidia-cuda-toolkit

echo ""
echo "=== Installation Complete ==="
echo ""
echo "Verifying installation..."
nvcc --version

echo ""
echo "Testing compilation..."
cd /home/randy/Workspace/REPOS/aria
chmod +x run_gpu_test.sh
./run_gpu_test.sh
