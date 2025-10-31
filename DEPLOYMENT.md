# Bitcoin Display - Complete Deployment Roadmap

This guide walks you through deploying your Bitcoin Live Price Display with GitHub OTA updates. All items requiring your input or changes are marked with **double asterisks**.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Initial Configuration](#initial-configuration)
3. [GitHub Repository Setup](#github-repository-setup)
4. [First Upload (USB)](#first-upload-usb)
5. [Creating Firmware Releases](#creating-firmware-releases)
6. [OTA Update Process](#ota-update-process)
7. [Testing & Verification](#testing--verification)
8. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Required Software

- **PlatformIO** installed (via VS Code extension or CLI)
- **Git** installed on your computer
- **GitHub account** created

### Required Hardware

- **LILYGO T-Display ESP32** (4MB flash)
- **USB cable** (for initial upload)
- **WiFi network** (2.4GHz)

---

## Initial Configuration

### Step 1: Configure WiFi and GitHub Settings

**Open `include/secrets.h` and edit the following:**

```cpp
#pragma once

// WiFi credentials
#define WIFI_SSID "**YOUR_WIFI_NETWORK_NAME**"      // Replace with your WiFi SSID
#define WIFI_PASS "**YOUR_WIFI_PASSWORD**"          // Replace with your WiFi password

// GitHub OTA settings
// Format: "username/repository"
#define GITHUB_REPO "**YOUR_GITHUB_USERNAME/YOUR_REPO_NAME**"  // Replace with your GitHub repo
```

**Example:**
```cpp
#define WIFI_SSID "MyHomeWiFi"
#define WIFI_PASS "MySecurePassword123"
#define GITHUB_REPO "johndoe/btc-display-firmware"
```

### Step 2: Verify Firmware Version

The firmware version is defined in `src/main.cpp` at line 17:

```cpp
#define FIRMWARE_VERSION "1.0.0"
```

**For the initial upload, leave this as `"1.0.0"`**. You'll increment this for future releases.

---

## GitHub Repository Setup

### Step 1: Create a New GitHub Repository

1. Go to **https://github.com/new**
2. **Repository name:** `**YOUR_REPO_NAME**` (e.g., `btc-display-firmware`)
3. **Description:** "Bitcoin Live Price Display with OTA Updates"
4. **Visibility:** Public or Private (both work, but public is easier)
5. Click **"Create repository"**

### Step 2: Initialize Local Git Repository

Open a terminal in the project directory and run:

```bash
git init
git add .
git commit -m "Initial commit - Bitcoin Display v1.0.0"
git branch -M main
git remote add origin **https://github.com/YOUR_GITHUB_USERNAME/YOUR_REPO_NAME.git**
git push -u origin main
```

**Replace `YOUR_GITHUB_USERNAME` and `YOUR_REPO_NAME` with your actual values.**

**Example:**
```bash
git remote add origin https://github.com/johndoe/btc-display-firmware.git
```

---

## First Upload (USB)

### Step 1: Connect Your ESP32

**Connect your LILYGO T-Display ESP32 to your computer via USB.**

### Step 2: Build the Firmware

```bash
pio run
```

This will compile the firmware and create a `.bin` file at:
```
.pio/build/esp32dev/firmware.bin
```

### Step 3: Upload via USB

```bash
pio run --target upload
```

**If upload fails:**
- **Check USB cable** (some cables are charge-only)
- **Install CP210x drivers** if needed
- **Try a different USB port**
- **Hold BOOT button** on ESP32 while uploading

### Step 4: Monitor Serial Output

```bash
pio device monitor
```

You should see:
```
=== Bitcoin Live Display ===
[WiFi] Connecting to YOUR_WIFI_SSID
[WiFi] Connected!
[WiFi] IP: 192.168.x.x
[API] Fetching current price...
[API] Price: $98234.56
[INIT] Setup complete!
```

**If you see WiFi errors, verify your SSID and password in `secrets.h`.**

---

## Creating Firmware Releases

Every time you want to deploy a new version via OTA, follow these steps:

### Step 1: Update Firmware Version

**Edit `src/main.cpp` line 17:**

```cpp
#define FIRMWARE_VERSION "**1.1.0**"  // Increment from 1.0.0
```

**Version numbering:**
- **Major** (1.x.x): Breaking changes
- **Minor** (x.1.x): New features
- **Patch** (x.x.1): Bug fixes

### Step 2: Make Your Code Changes

**Edit the code as needed** (bug fixes, new features, etc.)

### Step 3: Build the New Firmware

```bash
pio run
```

### Step 4: Commit and Push Changes

```bash
git add .
git commit -m "**Release v1.1.0 - Your description here**"
git push
```

### Step 5: Create a GitHub Release

#### Option A: Via GitHub Web Interface

1. Go to **`https://github.com/YOUR_GITHUB_USERNAME/YOUR_REPO_NAME/releases`**
2. Click **"Draft a new release"**
3. **Tag version:** `**v1.1.0**` (must match FIRMWARE_VERSION)
4. **Release title:** `**Version 1.1.0**`
5. **Description:**
   ```
   **What's New:**
   - Feature 1
   - Bug fix 2
   - Improvement 3
   ```
6. **Upload firmware binary:**
   - Click **"Attach binaries"**
   - **Upload `.pio/build/esp32dev/firmware.bin`**
7. Click **"Publish release"**

#### Option B: Via GitHub CLI

```bash
# Install GitHub CLI first: https://cli.github.com/

gh release create **v1.1.0** \
  --title "**Version 1.1.0**" \
  --notes "**Your release notes here**" \
  .pio/build/esp32dev/firmware.bin
```

### Step 6: Verify Release

**Check that your release appears at:**
```
https://github.com/**YOUR_GITHUB_USERNAME**/**YOUR_REPO_NAME**/releases
```

**Ensure the `.bin` file is attached to the release!**

---

## OTA Update Process

### Automatic Updates

The ESP32 checks for firmware updates **every 60 minutes** automatically.

**When an update is found:**

1. Display shows **"FIRMWARE UPDATE"**
2. Display shows **"Downloading..."**
3. Progress bar appears (0-100%)
4. Display shows **"UPDATE COMPLETE!"**
5. Device reboots automatically
6. New firmware is running!

### Manual Update Check

To force an immediate update check:

1. **Power cycle the device** (unplug and replug)
2. **Wait 60 minutes** for the next automatic check
3. **OR** hold the button for 3+ seconds (if you add this feature)

### Update Flow Diagram

```
ESP32 Device                    GitHub API
     |                               |
     |---Check for new release------>|
     |<--Return latest tag_name------|
     |                               |
     | Compare versions              |
     | (1.0.0 vs 1.1.0)              |
     |                               |
     |---Download firmware.bin------>|
     |<--Send binary data------------|
     |                               |
     | Flash firmware                |
     | Reboot                        |
     | Now running v1.1.0!           |
```

---

## Testing & Verification

### Test Checklist

After initial deployment:

- [ ] **WiFi connects successfully**
- [ ] **BTC price displays correctly**
- [ ] **7-day chart appears**
- [ ] **Price updates every 60 seconds**
- [ ] **Chart updates every 15 minutes**
- [ ] **Backlight button toggles brightness**

After first OTA update:

- [ ] **Device detects new version**
- [ ] **Firmware downloads successfully**
- [ ] **Device reboots automatically**
- [ ] **New version is running** (check serial monitor)

### Serial Monitor Commands

Monitor the device during updates:

```bash
pio device monitor
```

**Look for these messages:**

```
[OTA] Checking for firmware updates...
[OTA] Current version: 1.0.0
[OTA] Latest version: 1.1.0
[OTA] New version available!
[OTA] Found firmware: firmware.bin
[OTA] Starting firmware download...
[OTA] Progress: 50%
[OTA] Progress: 100%
[OTA] Update successful!
```

---

## Troubleshooting

### WiFi Connection Issues

**Problem:** Display shows "WiFi Failed!"

**Solutions:**
- **Verify SSID and password** in `include/secrets.h`
- **Ensure 2.4GHz WiFi** (ESP32 doesn't support 5GHz)
- **Check WiFi signal strength** (move closer to router)
- **Disable MAC address filtering** on router temporarily

---

### GitHub OTA Update Issues

**Problem:** Updates never download

**Solutions:**
1. **Verify GitHub repository name** in `secrets.h`
   ```cpp
   #define GITHUB_REPO "**username/repo**"  // Must be exact!
   ```

2. **Check release tag format:**
   - **Correct:** `v1.1.0` (lowercase 'v' + version)
   - **Wrong:** `V1.1.0`, `1.1.0`, `release-1.1.0`

3. **Ensure `.bin` file is attached to release**
   - Go to release page
   - Look for "Assets" section
   - **Must contain `firmware.bin`**

4. **Check version comparison:**
   - Current: `1.0.0`
   - Release tag: `v1.1.0`
   - **Release must be newer!**

5. **Verify internet connectivity:**
   ```bash
   pio device monitor
   ```
   Look for:
   ```
   [OTA] Failed to connect to GitHub!
   ```

---

### Binary File Not Found

**Problem:** No `.bin` file in build directory

**Solution:**
```bash
# Clean and rebuild
pio run --target clean
pio run
```

**Verify file exists:**
```bash
ls .pio/build/esp32dev/firmware.bin
```

---

### Version Not Incrementing

**Problem:** OTA says "Firmware is up to date" but you created a new release

**Solutions:**
1. **Check version in code:**
   ```cpp
   #define FIRMWARE_VERSION "1.1.0"  // Must match release tag!
   ```

2. **Check release tag:**
   - Tag: `v1.1.0`
   - Code: `1.1.0`
   - **Must match (without 'v' in code)!**

3. **Rebuild firmware:**
   ```bash
   pio run --target clean
   pio run
   ```

4. **Re-upload `.bin` to release:**
   - Delete old asset
   - Upload new `firmware.bin`

---

### Serial Monitor Not Working

**Problem:** No output in serial monitor

**Solutions:**
- **Check baud rate:** Should be `115200`
  ```bash
  pio device monitor --baud 115200
  ```
- **Try different USB port**
- **Unplug and replug device**
- **Check USB cable** (must support data, not just power)

---

## Advanced Configuration

### Change Update Check Interval

**Edit `src/main.cpp` line 38:**

```cpp
#define FIRMWARE_UPDATE_INTERVAL **3600000**  // Default: 60 minutes (in milliseconds)
```

**Examples:**
- **30 minutes:** `1800000`
- **2 hours:** `7200000`
- **24 hours:** `86400000`

---

### Private Repository Setup

If using a **private GitHub repository**, you need a Personal Access Token:

1. **Generate token:** https://github.com/settings/tokens
2. **Permissions:** `repo` (full control)
3. **Copy token** (you won't see it again!)
4. **Add to `secrets.h`:**
   ```cpp
   #define GITHUB_TOKEN "**ghp_yourTokenHere**"
   ```
5. **Modify API request** in `main.cpp` line 396:
   ```cpp
   client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                "Host: " + host + "\r\n" +
                "User-Agent: ESP32-Bitcoin-Display\r\n" +
                "Authorization: token **" + String(GITHUB_TOKEN) + "**\r\n" +
                "Accept: application/vnd.github.v3+json\r\n" +
                "Connection: close\r\n\r\n");
   ```

---

## Summary Checklist

### Initial Setup

- [ ] **Edit `include/secrets.h`** with WiFi credentials
- [ ] **Edit `include/secrets.h`** with GitHub repo name
- [ ] **Create GitHub repository**
- [ ] **Push code to GitHub**
- [ ] **Upload firmware via USB**
- [ ] **Verify device works**

### For Each Update

- [ ] **Edit code** (add features, fix bugs)
- [ ] **Increment `FIRMWARE_VERSION`** in `main.cpp`
- [ ] **Build firmware:** `pio run`
- [ ] **Commit and push changes**
- [ ] **Create GitHub release** with matching tag
- [ ] **Upload `firmware.bin`** to release
- [ ] **Wait for device to auto-update** (max 60 minutes)
- [ ] **Verify new version** via serial monitor

---

## Quick Reference

### File Locations

| File | Purpose | **What to Change** |
|------|---------|-------------------|
| `include/secrets.h` | WiFi & GitHub config | **WIFI_SSID, WIFI_PASS, GITHUB_REPO** |
| `src/main.cpp` | Main firmware | **FIRMWARE_VERSION** (line 17) |
| `.pio/build/esp32dev/firmware.bin` | Compiled binary | Upload to GitHub releases |

### Commands

| Command | Purpose |
|---------|---------|
| `pio run` | Build firmware |
| `pio run --target upload` | Upload via USB |
| `pio device monitor` | View serial output |
| `pio run --target clean` | Clean build files |
| `git push` | Push code to GitHub |
| `gh release create vX.X.X firmware.bin` | Create release (CLI) |

### Update Intervals

| What | Interval |
|------|----------|
| BTC Price | 60 seconds |
| 7-day Chart | 15 minutes |
| Firmware Check | 60 minutes |

---

## Support & Resources

- **PlatformIO Docs:** https://docs.platformio.org/
- **GitHub Releases:** https://docs.github.com/en/repositories/releasing-projects-on-github
- **ESP32 OTA Guide:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html
- **CoinGecko API:** https://www.coingecko.com/en/api

---

**Your Bitcoin Display is now ready for deployment with automatic GitHub OTA updates!**

When you want to push an update, simply:
1. **Change code**
2. **Bump version**
3. **Create release**
4. **Device updates automatically!**

No more USB cables needed! 🚀
