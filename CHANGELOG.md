# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned Features
- Add button long-press to force firmware update check
- Add WiFi signal strength indicator
- Add last update timestamp display
- Add configurable price alerts
- Consider deep sleep mode for battery operation

---

## [1.0.3] - 2025-01-05

### Added
- **OTA firmware update functionality** - Fully implemented GitHub-based over-the-air updates
- **Certificate pinning for CoinGecko API** - Enhanced security with SSL verification using DigiCert root CA
- **Exponential backoff for API failures** - Intelligent retry mechanism (5s, 10s, 20s, max 60s with jitter)
- **Memory optimization** - Pre-allocated String buffers to reduce heap fragmentation
- **Display state tracking** - Avoids unnecessary redraws for better performance
- **Non-blocking rate limit handling** - Replaced blocking delays with state-based backoff
- **Firmware version display** - Shows version number on startup screen
- **Progress bar for OTA updates** - Visual feedback during firmware downloads

### Changed
- **Reduced JSON buffer size** - Chart data buffer reduced from 32KB to 16KB (still sufficient)
- **Improved User-Agent headers** - Now includes firmware version for better API tracking
- **Enhanced error recovery** - Consecutive failure tracking with automatic backoff
- **Better serial logging** - More detailed OTA and API status messages

### Fixed
- **Security: SSL certificate verification** - No longer using `setInsecure()` for CoinGecko API
- **Memory leaks** - String concatenation now uses `reserve()` to pre-allocate
- **Missing OTA implementation** - Functions `checkForFirmwareUpdate()` and `performFirmwareUpdate()` now fully working
- **Rate limit handling** - Non-blocking approach prevents device freezes

### Security
- ⚠️ **BREAKING**: Secrets management improved - `secrets.h` now excluded from git by default
- Added SSL certificate pinning for CoinGecko API (MITM attack prevention)
- GitHub OTA still uses `setInsecure()` due to Let's Encrypt compatibility issues on ESP32

---

## [1.0.0] - 2025-01-XX

### Added
- Initial release
- Live BTC/USD price display (updates every 60 seconds)
- 7-day price chart with auto-scaling
- GitHub OTA update support (checks every 60 minutes)
- WiFi connectivity with auto-reconnect
- Backlight brightness toggle (GPIO 35 button)
- TFT display driver configuration via build flags
- CoinGecko API integration (no API key required)
- Semantic versioning comparison for updates
- Progress bar during firmware downloads
- Serial monitor logging for debugging

### Technical Details
- Platform: ESP32 (LILYGO T-Display)
- Display: ST7789 135x240 pixels
- Partition: min_spiffs (1.9MB APP / 1.9MB OTA)
- Memory: ~300KB flash, ~50KB RAM
- Libraries: TFT_eSPI, ArduinoJson

---

## How to Use This Changelog

### When creating a new release:

1. Copy the "Unreleased" section
2. Change `[Unreleased]` to `[X.X.X] - YYYY-MM-DD`
3. Add your changes under the appropriate category:
   - **Added** - New features
   - **Changed** - Changes to existing functionality
   - **Deprecated** - Soon-to-be removed features
   - **Removed** - Removed features
   - **Fixed** - Bug fixes
   - **Security** - Security fixes

### Example for v1.1.0:

```markdown
## [1.1.0] - 2025-02-15

### Added
- WiFi signal strength indicator on display
- Manual firmware update via long button press (3+ seconds)

### Changed
- Increased chart update interval to 30 minutes (was 15)
- Improved error handling for failed API calls

### Fixed
- WiFi reconnection issue after prolonged disconnection
- Display flicker when updating chart
```

---

## Version Numbering Guide

- **MAJOR** version (X.0.0): Incompatible API changes or major redesign
- **MINOR** version (0.X.0): New features, backward compatible
- **PATCH** version (0.0.X): Bug fixes, backward compatible

### Examples:

- `1.0.0` → `1.0.1`: Fixed a bug
- `1.0.1` → `1.1.0`: Added new feature
- `1.1.0` → `2.0.0`: Major rewrite or breaking change
