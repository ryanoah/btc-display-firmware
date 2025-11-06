# Bitcoin Live Price Display with GitHub OTA

A Bitcoin price tracker for LILYGO T-Display ESP32 with automatic firmware updates from GitHub releases.

![ESP32 Bitcoin Display](https://img.shields.io/badge/ESP32-Bitcoin%20Display-blue)
![OTA Updates](https://img.shields.io/badge/OTA-GitHub-green)
![Version](https://img.shields.io/badge/version-1.0.3-orange)
![Security](https://img.shields.io/badge/security-improved-brightgreen)

## Features

- **Live BTC/USD Price** - Updates every 60 seconds
- **7-Day Price Chart** - Historical data with auto-scaling
- **GitHub OTA Updates** - Automatic firmware updates from releases
- **WiFi Connectivity** - Connects to your home network
- **Backlight Control** - Toggle brightness with button (GPIO 35)
- **Low Power** - Runs on ESP32 with 4MB flash

## Hardware

- **LILYGO T-Display ESP32** (ESP32 + 1.14" TFT Display)
- 4MB Flash (uses min_spiffs partition: 1.9MB APP / 1.9MB OTA)
- Display: ST7789 135x240 pixels
- Button: GPIO 35

## Quick Start

### 1. Configure Your Settings

**Edit `include/secrets.h`:**

```cpp
// WiFi credentials
#define WIFI_SSID "**YOUR_WIFI_NETWORK_NAME**"
#define WIFI_PASS "**YOUR_WIFI_PASSWORD**"

// GitHub repository (format: "username/repository")
#define GITHUB_REPO "**YOUR_GITHUB_USERNAME/YOUR_REPO_NAME**"
```

### 2. Build and Upload

```bash
# Build firmware
pio run

# Upload via USB (first time only)
pio run --target upload

# Monitor output
pio device monitor
```

### 3. Create GitHub Repository

1. Create a new repository on GitHub
2. **Name it:** `**YOUR_REPO_NAME**`
3. Push this code to the repository

```bash
git init
git add .
git commit -m "Initial commit"
git remote add origin **https://github.com/YOUR_USERNAME/YOUR_REPO_NAME.git**
git push -u origin main
```

## Deploying Updates

### Step 1: Update Version

**Edit `src/main.cpp` line 17:**

```cpp
#define FIRMWARE_VERSION "**1.1.0**"  // Change from 1.0.0
```

### Step 2: Build Firmware

```bash
pio run
```

### Step 3: Create GitHub Release

1. Go to your repository on GitHub
2. Click **"Releases"** → **"Draft a new release"**
3. **Tag:** `**v1.1.0**` (must start with 'v' and match your version)
4. **Title:** `**Version 1.1.0**`
5. **Upload:** `.pio/build/esp32dev/firmware.bin`
6. Click **"Publish release"**

### Step 4: Wait for Auto-Update

The device checks for updates **every 60 minutes**. When found:

1. Display shows "FIRMWARE UPDATE"
2. Progress bar appears
3. Device reboots with new firmware

## Display Layout

```
┌─────────────────────────────┐
│ BTC LIVE          $98,234.56│ ← Header (28px)
├─────────────────────────────┤
│  $99k                       │
│  ┌───────────────────────┐  │
│  │    7-Day Price Chart  │  │ ← Chart area (240x100px)
│  │       /\    /\        │  │
│  │      /  \  /  \       │  │
│  └───────────────────────┘  │
│  $95k                       │
└─────────────────────────────┘
      ↑ Backlight button (GPIO 35)
```

## Project Structure

```
btc-tdisplay-ota/
├── platformio.ini          # PlatformIO configuration
├── include/
│   └── secrets.h          # **WiFi & GitHub settings (EDIT THIS)**
├── src/
│   └── main.cpp           # Main firmware
├── DEPLOYMENT.md          # Complete deployment guide
└── README.md              # This file
```

## Configuration Summary

### Files You MUST Edit

| File | Line | What to Change |
|------|------|---------------|
| **`include/secrets.h`** | 4 | **`WIFI_SSID`** - Your WiFi network name |
| **`include/secrets.h`** | 5 | **`WIFI_PASS`** - Your WiFi password |
| **`include/secrets.h`** | 10 | **`GITHUB_REPO`** - Your GitHub username/repo |
| **`src/main.cpp`** | 17 | **`FIRMWARE_VERSION`** - Increment for updates |

### Update Intervals

| Feature | Interval | Configurable |
|---------|----------|--------------|
| BTC Price | 60 seconds | Line 36 in main.cpp |
| 7-Day Chart | 15 minutes | Line 37 in main.cpp |
| Firmware Check | 60 minutes | Line 38 in main.cpp |

## API & Data Sources

- **Price Data:** CoinGecko API (free, no key required)
- **Firmware Updates:** GitHub Releases API
- **Chart Data:** 7 days of hourly BTC/USD prices

## Hardware Pins

| Pin | Function |
|-----|----------|
| GPIO 19 | TFT_MOSI |
| GPIO 18 | TFT_SCLK |
| GPIO 16 | TFT_DC |
| GPIO 23 | TFT_RST |
| GPIO 5 | TFT_CS |
| GPIO 4 | TFT_BL (PWM backlight) |
| GPIO 35 | Button (backlight toggle) |

## Troubleshooting

### WiFi Won't Connect

- **Check:** SSID and password in `secrets.h`
- **Ensure:** 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- **Try:** Move closer to router

### OTA Updates Not Working

- **Verify:** `GITHUB_REPO` format is `username/repository`
- **Check:** Release tag starts with 'v' (e.g., `v1.1.0`)
- **Ensure:** `.bin` file is attached to the release
- **Confirm:** Version in code is older than release tag

### Display Not Working

- **Check:** USB cable supports data (not just power)
- **Try:** Different USB port
- **Verify:** All build flags in `platformio.ini`

## Full Documentation

See **[DEPLOYMENT.md](DEPLOYMENT.md)** for complete step-by-step deployment instructions.

## Dependencies

- **PlatformIO** - Build system
- **TFT_eSPI** - Display driver (configured via build flags)
- **ArduinoJson** - JSON parsing for APIs
- **WiFiClientSecure** - HTTPS connections
- **HTTPUpdate** - OTA updates (built-in)

## Memory Usage

- **Flash:** ~300-400KB per firmware (fits in 1.9MB partition)
- **RAM:** ~40-50KB (plenty of headroom on 520KB SRAM)

## License

MIT License - Feel free to modify and distribute

## Credits

- Display hardware: LILYGO T-Display ESP32
- Price data: CoinGecko API
- OTA updates: GitHub Releases

---

## Quick Reference Card

### First Time Setup

1. **Edit `secrets.h`** - WiFi + GitHub repo
2. **`pio run --target upload`** - Upload via USB
3. **Create GitHub repo** - Push code
4. **Done!** - Device is running

### Deploying Updates

1. **Edit code** - Make changes
2. **Bump version** - In `main.cpp` line 17
3. **`pio run`** - Build firmware
4. **Create release** - On GitHub with `.bin` file
5. **Wait** - Device updates automatically (max 60 min)

---

**Happy Bitcoin tracking!** 📈💰
