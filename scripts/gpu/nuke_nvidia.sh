#!/bin/bash
# Complete NVIDIA cleanup and reinstall
# This removes EVERYTHING NVIDIA and starts fresh

set -e

echo "=== NVIDIA Complete Cleanup and Reinstall ==="
echo ""
echo "WARNING: This will remove ALL NVIDIA packages and drivers."
echo "Your display may switch to software rendering temporarily."
echo ""
read -p "Continue? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    exit 1
fi

# 1. Kill hung processes
echo "Step 1: Killing hung processes..."
sudo killall -9 apt apt-get dpkg 2>/dev/null || true
sleep 2

# 2. Remove locks
echo "Step 2: Removing lock files..."
sudo rm -f /var/lib/dpkg/lock-frontend
sudo rm -f /var/lib/dpkg/lock
sudo rm -f /var/cache/apt/archives/lock
sudo rm -f /var/lib/apt/lists/lock

# 3. Backup dpkg status
echo "Step 3: Backing up dpkg database..."
sudo cp /var/lib/dpkg/status /var/lib/dpkg/status.backup-$(date +%Y%m%d-%H%M%S)

# 4. Force remove ALL NVIDIA packages (including drivers)
echo "Step 4: Force removing ALL NVIDIA packages..."
sudo dpkg --remove --force-remove-reinstreq --force-depends $(dpkg -l | grep -E '^(ii|iU|rc)' | grep -i nvidia | awk '{print $2}') 2>/dev/null || true
sudo dpkg --remove --force-remove-reinstreq --force-depends $(dpkg -l | grep -E '^(ii|iU|rc)' | grep -i cuda | awk '{print $2}') 2>/dev/null || true

# 5. Purge any remaining packages
echo "Step 5: Purging remaining packages..."
sudo apt-get purge -y --allow-remove-essential 'nvidia-*' 'cuda-*' 'libnvidia-*' 2>/dev/null || true

# 6. Remove NVIDIA runfile installation
echo "Step 6: Removing NVIDIA runfile installation..."
if [ -f /usr/bin/nvidia-uninstall ]; then
    sudo /usr/bin/nvidia-uninstall --silent || true
fi
sudo rm -rf /usr/local/cuda*
sudo rm -rf /usr/lib/cuda*

# 7. Clean package cache
echo "Step 7: Cleaning package cache..."
sudo apt-get clean
sudo rm -rf /var/lib/apt/lists/*
sudo mkdir -p /var/lib/apt/lists/partial

# 8. Update package lists
echo "Step 8: Updating package lists..."
sudo apt-get update

# 9. Fix broken dependencies
echo "Step 9: Fixing broken dependencies..."
sudo dpkg --configure -a || true
sudo apt-get install -f -y || true

# 10. Autoremove
echo "Step 10: Removing orphaned packages..."
sudo apt-get autoremove -y --purge

# 11. Remove nouveau blacklist (allow nouveau to load)
echo "Step 11: Removing nouveau blacklist..."
sudo rm -f /etc/modprobe.d/blacklist-nvidia.conf
sudo rm -f /etc/modprobe.d/nvidia-installer-disable-nouveau.conf
sudo rm -f /lib/modprobe.d/blacklist-nvidia.conf
sudo update-initramfs -u

echo ""
echo "=== Cleanup Complete ==="
echo ""
echo "System will now revert to nouveau (open-source) driver."
echo ""
echo "REBOOT NOW to load nouveau driver."
echo ""
read -p "Reboot now? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    sudo reboot
else
    echo ""
    echo "After reboot, install latest NVIDIA driver (590 series) with:"
    echo ""
    echo "  # Add graphics-drivers PPA for latest drivers:"
    echo "  sudo add-apt-repository ppa:graphics-drivers/ppa"
    echo "  sudo apt update"
    echo ""
    echo "  # Install latest 590 driver:"
    echo "  sudo apt install nvidia-driver-590"
    echo ""
    echo "  # Or check available versions:"
    echo "  ubuntu-drivers list"
    echo ""
    echo "  # Then install CUDA toolkit:"
    echo "  sudo apt install nvidia-cuda-toolkit"
    echo ""
    echo "IMPORTANT: Reboot after driver install before installing CUDA!"
fi
