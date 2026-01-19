# AriaX Branding & Default Backgrounds

## Overview

This document explains how AriaX backgrounds are set system-wide and will persist in the installer/ISO.

## Files to Include in Installer

### 1. Background Images
**Location**: `/usr/share/backgrounds/ariax/`

```
AriaCloseup.png       (11M) - Login screen background
AriaFullWithLogo.png  (15M) - Desktop background
```

These files go in `/usr/share/backgrounds/` which is the standard location for system backgrounds.

### 2. Login Screen Configuration
**Location**: `/etc/lightdm/slick-greeter.conf`

```ini
[Greeter]
draw-user-backgrounds=false
background=/usr/share/backgrounds/ariax/AriaCloseup.png
enable-hidpi=auto
theme-name=Mint-Y-Dark-Aqua
icon-theme-name=Mint-Y-Blue
```

This sets the LightDM/Slick Greeter background that users see at login.

### 3. Default Desktop Background
**Location**: `/usr/share/glib-2.0/schemas/90_ariax-defaults.gschema.override`

This file sets system-wide defaults for desktop backgrounds across multiple desktop environments:

```ini
[org.cinnamon.desktop.background]
picture-uri="file:///usr/share/backgrounds/ariax/AriaFullWithLogo.png"
picture-options="zoom"

[org.gnome.desktop.background]
picture-uri="file:///usr/share/backgrounds/ariax/AriaFullWithLogo.png"
picture-options="zoom"

[org.mate.background]
picture-filename="/usr/share/backgrounds/ariax/AriaFullWithLogo.png"
picture-options="zoom"
```

**Important**: After installing/modifying this file, you MUST run:
```bash
sudo glib-compile-schemas /usr/share/glib-2.0/schemas/
```

## How It Works

### Priority System

GSettings override files use a priority system based on filename prefix:
- `10_*.gschema.override` - Low priority (default Mint settings)
- `50_*.gschema.override` - Medium priority
- `90_*.gschema.override` - High priority (AriaX defaults) ✅

AriaX uses `90_` prefix to override Mint defaults while allowing user customizations to still take precedence.

### When Settings Apply

1. **Login Screen**: Immediately (controlled by `/etc/lightdm/slick-greeter.conf`)
2. **Desktop Background**: On new user creation or when user resets to defaults
3. **Existing Users**: Not affected (they keep their current settings)

### User Customization

Users can still change their wallpaper normally. The defaults only apply:
- When a new user account is created
- When a user clicks "Reset to defaults" in settings
- When a user hasn't customized their background yet

## For ISO/Installer Creation

### Method 1: Live Build / Cubic

When building an ISO with tools like Cubic or live-build:

1. **Copy files to chroot**:
   ```bash
   # In chroot environment
   cp AriaCloseup.png /usr/share/backgrounds/ariax/
   cp AriaFullWithLogo.png /usr/share/backgrounds/ariax/
   cp slick-greeter.conf /etc/lightdm/
   cp 90_ariax-defaults.gschema.override /usr/share/glib-2.0/schemas/
   ```

2. **Compile schemas**:
   ```bash
   glib-compile-schemas /usr/share/glib-2.0/schemas/
   ```

3. **Build ISO**: Files persist in `/usr/share/` and `/etc/`

### Method 2: Debian Package

Create a `.deb` package with:

```
ariax-branding/
├── DEBIAN/
│   ├── control
│   └── postinst  (contains: glib-compile-schemas /usr/share/glib-2.0/schemas/)
├── etc/
│   └── lightdm/
│       └── slick-greeter.conf
└── usr/
    └── share/
        ├── backgrounds/
        │   └── ariax/
        │       ├── AriaCloseup.png
        │       └── AriaFullWithLogo.png
        └── glib-2.0/
            └── schemas/
                └── 90_ariax-defaults.gschema.override
```

### Method 3: Systemback or Remastersys

1. **Set up current system** with all files in place
2. **Run Systemback** to create ISO
3. All files in `/usr/share/` and `/etc/` automatically included

## Testing New User Defaults

To test if backgrounds will apply to new users:

```bash
# Create a test user
sudo useradd -m testuser
sudo passwd testuser

# Login as testuser and check:
# - Login screen shows AriaCloseup.png
# - Desktop background shows AriaFullWithLogo.png
```

## Verification Commands

```bash
# Check files exist
ls -lh /usr/share/backgrounds/ariax/
ls -lh /etc/lightdm/slick-greeter.conf
ls -lh /usr/share/glib-2.0/schemas/90_ariax-defaults.gschema.override

# Check compiled schemas
ls -lh /usr/share/glib-2.0/schemas/gschemas.compiled

# View current login screen setting
grep background /etc/lightdm/slick-greeter.conf

# Check default desktop background
gsettings get org.cinnamon.desktop.background picture-uri
```

## Desktop Environment Coverage

| Desktop Environment | Supported | Settings Path |
|---------------------|-----------|---------------|
| Cinnamon | ✅ Yes | `org.cinnamon.desktop.background` |
| GNOME | ✅ Yes | `org.gnome.desktop.background` |
| MATE | ✅ Yes | `org.mate.background` |
| Xfce | ⚠️ Partial | Needs `/etc/skel/.config/xfce4/` config |
| KDE Plasma | ❌ No | Uses different config system |

## Troubleshooting

### Background not changing for new users?

1. **Check schema compilation**:
   ```bash
   sudo glib-compile-schemas /usr/share/glib-2.0/schemas/
   ```

2. **Check file permissions**:
   ```bash
   sudo chmod 644 /usr/share/backgrounds/ariax/*.png
   sudo chmod 644 /usr/share/glib-2.0/schemas/90_ariax-defaults.gschema.override
   ```

3. **Check file syntax**:
   ```bash
   # Should output no errors
   glib-compile-schemas --strict /usr/share/glib-2.0/schemas/
   ```

### Login screen not changing?

1. **Restart LightDM**:
   ```bash
   sudo systemctl restart lightdm
   ```

2. **Check file path**:
   ```bash
   test -f /usr/share/backgrounds/ariax/AriaCloseup.png && echo "File exists" || echo "File missing"
   ```

## Summary

✅ **What Persists in Installer**:
- Background images in `/usr/share/backgrounds/ariax/`
- Login screen config in `/etc/lightdm/slick-greeter.conf`
- Desktop defaults in `/usr/share/glib-2.0/schemas/90_ariax-defaults.gschema.override`
- Compiled schemas in `/usr/share/glib-2.0/schemas/gschemas.compiled`

✅ **What Happens on Install**:
- New installations get AriaX backgrounds by default
- Users can customize without breaking anything
- Login screen always shows AriaX branding

✅ **Maintenance**:
- To update: Replace files + recompile schemas
- No user data affected
- Works across system upgrades

---

**Status**: 🎨 Production-ready for AriaX v1.0.0 ISO
