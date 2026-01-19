# AriaX v1.0.0 ISO Creation Guide

## Overview

This guide covers creating a bootable AriaX ISO installer based on Linux Mint 22.3 with all your customizations baked in.

## Method 1: Cubic (Recommended - Most Control)

**Cubic** (Custom Ubuntu ISO Creator) works great for Mint too and gives you the most control.

### Installation

```bash
sudo apt-add-repository universe
sudo apt-add-repository ppa:cubic-wizard/release
sudo apt update
sudo apt install cubic
```

### Step-by-Step Process

#### 1. Prepare Base ISO

Download Linux Mint 22.3 Cinnamon ISO:
```bash
cd ~/Downloads
wget https://mirrors.layeronline.com/linuxmint/stable/22.3/linuxmint-22.3-cinnamon-64bit.iso
# Verify checksum from https://linuxmint.com/verify.php
```

#### 2. Launch Cubic

```bash
cubic
```

#### 3. Create Project

- Click "Next"
- **Project Directory**: `~/cubic/ariax-v1.0.0/`
- **Original ISO**: Select downloaded Mint 22.3 ISO
- Click "Next"

#### 4. Customize the Chroot Environment

Cubic opens a terminal in the chroot environment. This is where you customize:

##### A. Install AriaX Kernel

```bash
# Copy kernel packages to chroot
# (On host, before entering chroot)
cp /boot/vmlinuz-6.8.12-ariax /path/to/cubic/chroot/tmp/
cp -r /lib/modules/6.8.12-ariax /path/to/cubic/chroot/lib/modules/

# In chroot:
apt install linux-base
update-initramfs -c -k 6.8.12-ariax
update-grub
```

##### B. Copy AriaX Files

```bash
# Backgrounds
mkdir -p /usr/share/backgrounds/ariax
# Copy your backgrounds here (Cubic has a file browser)

# AriaX utilities
mkdir -p /usr/local/bin
# Copy all utilities from dist_package/bin/

# Workspace tools
# Already in /etc/skel from your VM

# Theme overrides
# Copy 90_ariax-defaults.gschema.override
glib-compile-schemas /usr/share/glib-2.0/schemas/

# Login screen config
# Edit /etc/lightdm/slick-greeter.conf
```

##### C. Install Additional Software

```bash
# Everything you installed on VM
apt update

# Editors & IDEs
apt install -y vim neovim emacs geany kate code

# Compilers
apt install -y build-essential gcc g++ clang llvm-20 \
    rustc cargo zig openjdk-25-jdk nasm

# Build tools
apt install -y cmake ninja-build autoconf automake

# Languages
apt install -y python3-dev python3-venv python3-pip \
    nodejs npm lua5.4

# Databases
apt install -y postgresql redis-server dbeaver-ce

# Web servers
apt install -y apache2 nginx docker.io

# Modern CLI tools
apt install -y ripgrep fzf bat exa tldr jq ncdu ranger \
    tmux screen htop neofetch

# Multimedia
apt install -y gimp inkscape audacity vlc kdenlive \
    handbrake ffmpeg

# Development tools
apt install -y valgrind git ack

# Fun stuff
apt install -y sl lolcat figlet cowsay
```

##### D. Configure System

```bash
# Set hostname
echo "ariax-live" > /etc/hostname

# Update /etc/issue for boot message
cat > /etc/issue << 'ISSUE'
AriaX Linux v1.0.0 - Alternative Intelligence Liberation Platform
Based on Linux Mint 22.3 LTS (Support until 2029)

Kernel: \r (\l)
ISSUE

# Clean up
apt autoremove -y
apt clean
rm -rf /tmp/*
rm -rf /var/tmp/*
```

##### E. Exit Chroot

Type `exit` or click "Next" in Cubic.

#### 5. Customize Live Session (Optional)

Cubic shows options for:
- **Boot Menu**: Customize GRUB menu
- **Preseed**: Auto-answer installer questions
- **Compression**: Choose filesystem compression

**Recommended settings:**
- Compression: `xz` (smaller ISO)
- Keep default boot menu (or customize with AriaX branding)

#### 6. Generate ISO

- Review your changes
- Click "Generate"
- Wait for ISO creation (10-30 minutes depending on size)
- **Output**: `~/cubic/ariax-v1.0.0/ariax-v1.0.0.iso`

---

## Method 2: Systemback (Easiest - Current System Snapshot)

**Systemback** creates an ISO from your current running system - perfect if your VM is already set up!

### Installation

```bash
sudo add-apt-repository ppa:nemh/systemback
sudo apt update
sudo apt install systemback
```

### Process

1. **Launch Systemback**:
   ```bash
   sudo systemback
   ```

2. **Create Live System**:
   - Click "Live system create"
   - **Name**: `ariax-v1.0.0`
   - **Compression**: `xz` (high compression)
   - Click "Create new"

3. **Wait for Creation**:
   - Creates ISO in `/home/Systemback/`
   - Usually 20-40 minutes

4. **Convert to ISO** (if needed):
   - Click "Convert to ISO"
   - Bootable ISO created

**Pros:**
- ✅ Captures exact current system state
- ✅ All your customizations automatically included
- ✅ Fastest method

**Cons:**
- ⚠️ Includes user data (clean VM recommended)
- ⚠️ Larger ISO size
- ⚠️ Less control over what's included

---

## Method 3: Remastersys Alternative (Manual but Flexible)

Since Remastersys is deprecated, use `mksquashfs` manually:

### Process

```bash
# Install required tools
sudo apt install squashfs-tools xorriso isolinux

# Create workspace
mkdir -p ~/ariax-iso/{image,mnt}
cd ~/ariax-iso

# Extract base Mint ISO
sudo mount -o loop ~/Downloads/linuxmint-22.3-cinnamon-64bit.iso mnt/
sudo cp -a mnt/. image/
sudo umount mnt/

# Customize filesystem
sudo unsquashfs image/casper/filesystem.squashfs
sudo mv squashfs-root/ chroot/

# Chroot and customize
sudo chroot chroot/
# (Make all your changes here like in Cubic)
exit

# Repackage filesystem
sudo rm image/casper/filesystem.squashfs
sudo mksquashfs chroot/ image/casper/filesystem.squashfs -comp xz

# Update manifest
sudo chroot chroot/ dpkg-query -W --showformat='${Package} ${Version}\n' > image/casper/filesystem.manifest

# Create ISO
cd image/
sudo mkisofs -r -V "AriaX v1.0.0" \
    -cache-inodes -J -l \
    -b isolinux/isolinux.bin \
    -c isolinux/boot.cat \
    -no-emul-boot -boot-load-size 4 -boot-info-table \
    -o ../ariax-v1.0.0.iso .

# Make bootable
isohybrid --uefi ../ariax-v1.0.0.iso
```

---

## Pre-ISO Checklist

Before creating the ISO, verify your VM has:

- [ ] ✅ AriaX kernel (6.8.12-ariax) installed and working
- [ ] ✅ Fallback kernels (5.15, 6.8.0-generic)
- [ ] ✅ All backgrounds in `/usr/share/backgrounds/ariax/`
- [ ] ✅ Theme override compiled: `/usr/share/glib-2.0/schemas/90_ariax-defaults.gschema.override`
- [ ] ✅ Login screen configured: `/etc/lightdm/slick-greeter.conf`
- [ ] ✅ /etc/skel populated:
  - [ ] Workspace/ with mkwrk, mkweb, mkpye, sync-aria.sh
  - [ ] .scripts/ with my_notes.sh, xkill.sh
  - [ ] .bashrc_ariax
  - [ ] .config/cinnamon/spices/
- [ ] ✅ All 21 AriaX utilities in `/usr/local/bin/`
- [ ] ✅ All development tools installed
- [ ] ✅ System cleaned (apt clean, tmp files removed)
- [ ] ✅ No personal data in /home/
- [ ] ✅ Git repositories cloned to example Projects/ directory

---

## Testing the ISO

### 1. Test in VirtualBox/VMware

```bash
# Create new VM
# Attach ariax-v1.0.0.iso as boot media
# Test live session
# Test installation
# Verify all customizations present
```

### 2. Test on Real Hardware (Recommended)

- Boot from USB on test machine
- Verify kernel boots
- Test six-stream utilities
- Check theme applies correctly
- Verify Workspace tools work

### 3. Verification Checklist

After installation:
- [ ] AriaX boot splash shows
- [ ] Login screen has AriaX background
- [ ] Desktop has AriaX wallpaper
- [ ] Theme is Mint-Y-Dark-Teal with Cyan icons
- [ ] Two panels (top auto-hides, bottom intelligent hide)
- [ ] Workspace switcher on top panel
- [ ] ~/Workspace/ exists with all tools
- [ ] ~/.scripts/ exists with helper scripts
- [ ] Six-stream kernel boots (check `uname -r`)
- [ ] AriaX utilities work (`acat --help`)
- [ ] Welcome message shows on first login

---

## Recommended: Cubic Workflow for AriaX

**Best approach for Uncle Mike's machine:**

1. **Use Systemback on your current VM** to create initial ISO:
   - Captures everything you've set up
   - Fast and easy
   - Everything works exactly as configured

2. **Refine with Cubic** if needed:
   - Remove any test data
   - Optimize size
   - Add preseed for automated installs
   - Customize boot menu

3. **Test thoroughly**:
   - VirtualBox first
   - Real hardware second
   - Verify Uncle Mike workflow

4. **Burn and deploy**:
   - Create bootable USB with `dd` or Etcher
   - Install on Uncle Mike's workstation
   - Clone repos, configure git
   - Hand off machine ready to code!

---

## Creating Bootable USB

```bash
# Find USB device
lsblk

# Write ISO (replace sdX with your USB device!)
sudo dd if=ariax-v1.0.0.iso of=/dev/sdX bs=4M status=progress oflag=sync

# Or use Etcher (GUI, safer)
# Download from: https://etcher.balena.io/
```

---

## ISO Size Estimates

**Minimal AriaX** (base Mint + AriaX kernel + utilities):
- ~2.5-3GB

**Full AriaX** (everything you've installed):
- ~5-7GB (all editors, compilers, databases, tools)

**With pre-cloned repos**:
- Add ~500MB per repo

---

## Distribution

Once you have a working ISO:

1. **GitHub Release**:
   - Upload to GitHub Releases
   - Include checksums (SHA256)
   - Provide installation guide

2. **Torrent** (for large files):
   - Create torrent with transmission-cli
   - Reduces bandwidth costs

3. **Documentation**:
   - Release notes
   - Known issues
   - Installation instructions
   - Six-stream quick start guide

---

## Next Steps

1. Clean up your VM (remove test data)
2. Choose method (Systemback recommended for first ISO)
3. Create ISO
4. Test in VirtualBox
5. Test on real hardware
6. Deploy to Uncle Mike's machine
7. Publish AriaX v1.0.0! 🚀

