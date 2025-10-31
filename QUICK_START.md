# Quick Start Guide - Action Items

This document lists **ONLY** the items that require your input or changes. All items are marked with **double asterisks**.

---

## Before You Start

### 1. Edit WiFi Settings

**File:** `include/secrets.h` **Lines 4-5**

```cpp
#define WIFI_SSID "**YOUR_WIFI_NETWORK_NAME**"     // ← Change this
#define WIFI_PASS "**YOUR_WIFI_PASSWORD**"         // ← Change this
```

**Example:**
```cpp
#define WIFI_SSID "MyHomeWiFi"
#define WIFI_PASS "SecurePass123"
```

---

### 2. Create GitHub Repository

1. Go to **https://github.com/new**
2. **Repository name:** `**btc-display-firmware**` (or any name you choose)
3. Set visibility to **Public** or **Private**
4. Click **"Create repository"**

**Remember your repository name!** You'll need it in the next step.

---

### 3. Edit GitHub Repository Setting

**File:** `include/secrets.h` **Line 10**

```cpp
#define GITHUB_REPO "**YOUR_GITHUB_USERNAME/YOUR_REPO_NAME**"  // ← Change this
```

**Example:**
```cpp
#define GITHUB_REPO "johndoe/btc-display-firmware"
```

**Format:** `username/repository` (no https://, no .git, just username/repo)

---

### 4. Upload Code to GitHub

Open terminal in project folder and run:

```bash
git init
git add .
git commit -m "Initial commit - Bitcoin Display v1.0.0"
git branch -M main
git remote add origin **https://github.com/YOUR_GITHUB_USERNAME/YOUR_REPO_NAME.git**
git push -u origin main
```

**Replace in the command above:**
- **`YOUR_GITHUB_USERNAME`** with your actual GitHub username
- **`YOUR_REPO_NAME`** with your repository name

---

### 5. Build and Upload Firmware (First Time)

Connect your ESP32 via USB and run:

```bash
pio run --target upload
```

**Done!** Your device is now running and will auto-update from GitHub releases.

---

## For Each Future Update

### 1. Edit Code

**Make your changes** to the firmware (bug fixes, new features, etc.)

### 2. Update Version Number

**File:** `src/main.cpp` **Line 17**

```cpp
#define FIRMWARE_VERSION "**1.1.0**"  // ← Increment this (was 1.0.0)
```

**Version rules:**
- **1.x.x** - Major changes
- **x.1.x** - New features
- **x.x.1** - Bug fixes

---

### 3. Build Firmware

```bash
pio run
```

This creates: `.pio/build/esp32dev/firmware.bin`

---

### 4. Create GitHub Release

#### Via Web Interface:

1. Go to **`https://github.com/YOUR_USERNAME/YOUR_REPO/releases`**
2. Click **"Draft a new release"**
3. **Tag version:** `**v1.1.0**` (must match your firmware version with 'v' prefix)
4. **Release title:** `**Version 1.1.0**`
5. **Describe changes:**
   ```
   **What's New:**
   - Added feature X
   - Fixed bug Y
   ```
6. Click **"Attach binaries by dropping them here or selecting them"**
7. **Upload:** `.pio/build/esp32dev/firmware.bin`
8. Click **"Publish release"**

#### Via GitHub CLI (if installed):

```bash
gh release create **v1.1.0** \
  --title "**Version 1.1.0**" \
  --notes "**Your release notes here**" \
  .pio/build/esp32dev/firmware.bin
```

---

### 5. Wait for Auto-Update

The device checks for updates **every 60 minutes**. When it finds your new release:

1. Display shows "FIRMWARE UPDATE"
2. Progress bar shows download progress
3. Device reboots automatically
4. New version is running!

---

## Summary Checklist

### One-Time Setup

- [ ] **Edit `include/secrets.h`** - Lines 4, 5, 10 (WiFi + GitHub repo)
- [ ] **Create GitHub repository**
- [ ] **Push code to GitHub**
- [ ] **Upload firmware via USB:** `pio run --target upload`

### For Each Update

- [ ] **Edit code** (make your changes)
- [ ] **Update version** in `src/main.cpp` line 17
- [ ] **Build:** `pio run`
- [ ] **Commit:** `git commit -am "Release vX.X.X"`
- [ ] **Push:** `git push`
- [ ] **Create GitHub release** with tag `vX.X.X`
- [ ] **Upload `firmware.bin`** to the release
- [ ] **Wait** for device to update (max 60 minutes)

---

## All Files You Need to Edit

| File | What to Change | Format |
|------|---------------|--------|
| **`include/secrets.h`** line 4 | **`WIFI_SSID`** | `"YourNetworkName"` |
| **`include/secrets.h`** line 5 | **`WIFI_PASS`** | `"YourPassword"` |
| **`include/secrets.h`** line 10 | **`GITHUB_REPO`** | `"username/repository"` |
| **`src/main.cpp`** line 17 | **`FIRMWARE_VERSION`** | `"1.0.0"` → `"1.1.0"` |

---

## Common Mistakes to Avoid

### ❌ Wrong GitHub Repo Format

```cpp
// WRONG:
#define GITHUB_REPO "https://github.com/user/repo.git"
#define GITHUB_REPO "github.com/user/repo"
#define GITHUB_REPO "user/repo.git"

// CORRECT:
#define GITHUB_REPO "user/repo"
```

### ❌ Wrong Release Tag Format

```
// WRONG:
Tag: 1.1.0
Tag: V1.1.0 (uppercase V)
Tag: version-1.1.0
Tag: release-1.1.0

// CORRECT:
Tag: v1.1.0 (lowercase v + version number)
```

### ❌ Version Mismatch

```cpp
// src/main.cpp
#define FIRMWARE_VERSION "1.1.0"

// GitHub Release Tag
v1.2.0  // ❌ WRONG - doesn't match!

// Should be:
v1.1.0  // ✅ CORRECT - matches code version
```

### ❌ Forgot to Upload .bin File

When creating a release, you MUST upload the `firmware.bin` file. The release won't work without it!

**Check:** Release page should show "Assets" section with `firmware.bin` (e.g., 350 KB)

---

## Need More Help?

See **[DEPLOYMENT.md](DEPLOYMENT.md)** for the complete step-by-step guide with troubleshooting.

---

**That's it! Just edit those 4 values and you're ready to go!** 🚀
