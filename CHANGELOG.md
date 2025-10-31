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
