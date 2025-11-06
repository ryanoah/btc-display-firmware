# Security Policy

## Overview

This document outlines the security measures implemented in the Bitcoin Display firmware and provides recommendations for secure deployment.

---

## Security Measures Implemented (v1.0.3)

### ✅ SSL Certificate Pinning (CoinGecko API)

**Status:** Implemented

The firmware now uses SSL certificate pinning for CoinGecko API requests:

```cpp
// DigiCert Global Root G2 certificate
client.setCACert(coingecko_root_ca);
```

**Benefits:**
- Prevents Man-in-the-Middle (MITM) attacks
- Ensures API responses come from legitimate CoinGecko servers
- No longer vulnerable to certificate spoofing on local networks

**Certificate Details:**
- **Issuer:** DigiCert Global Root G2
- **Valid Until:** 2038-01-15
- **Location:** `src/main.cpp` lines 57-79

---

### ⚠️ GitHub OTA Security

**Status:** Partial implementation

**Current Approach:**
```cpp
client.setInsecure(); // GitHub uses Let's Encrypt
```

**Why:**
- Let's Encrypt certificates can be difficult to validate on ESP32
- Root CA chain changes frequently
- Trade-off between functionality and security for OTA updates

**Mitigation:**
- Use **private GitHub repositories** with access tokens (optional)
- Only install firmware from **trusted releases**
- Verify release authenticity before publishing

**Recommendation for Production:**
If deploying at scale, consider:
1. Self-hosted firmware server with pinned certificates
2. Firmware signature verification (cryptographic signing)
3. Rollback protection mechanisms

---

### ✅ Credentials Management

**Status:** Implemented (v1.0.3)

**Protection:**
- `include/secrets.h` is now **excluded from git** by default
- Template file `include/secrets.h.template` provided for users
- No credentials committed to repository history (after cleanup)

**User Action Required:**

```bash
# 1. Copy template to create your secrets file
cp include/secrets.h.template include/secrets.h

# 2. Edit with your actual credentials
nano include/secrets.h

# 3. Verify secrets.h is in .gitignore
cat .gitignore | grep secrets.h
```

---

## Security Best Practices

### For End Users

#### 1. Change Your WiFi Password

⚠️ **IMPORTANT:** If you previously committed `secrets.h` with real credentials:

```bash
# Change your WiFi password immediately
# Update router settings with new password
# Update secrets.h with new credentials
```

#### 2. Use WPA3 WiFi Security

- Enable WPA3 on your router if available
- Minimum: WPA2-PSK with strong password (16+ characters)
- Disable WPS (WiFi Protected Setup)

#### 3. Network Isolation

Consider placing ESP32 devices on a separate VLAN or guest network:
- Limits damage if device is compromised
- Prevents access to sensitive home network resources

#### 4. Physical Security

- Device has no authentication mechanism
- Anyone with physical access can press the button
- Consider enclosure with tamper-evident seals for critical deployments

---

### For Developers

#### 1. GitHub Repository Security

**Public Repositories:**
- ✅ Never commit secrets
- ✅ Use `.gitignore` for `secrets.h`
- ✅ Review all commits before pushing
- ✅ Use GitHub secret scanning alerts

**Private Repositories:**
- Enable 2FA on GitHub account
- Use Personal Access Tokens with minimal scopes
- Rotate tokens regularly

#### 2. Firmware Release Process

**Before Publishing:**

```bash
# 1. Build firmware
pio run

# 2. Verify no secrets in binary (optional paranoia check)
strings .pio/build/esp32dev/firmware.bin | grep -i "password\|ssid\|secret"

# 3. Create release with clean binary
gh release create v1.0.3 .pio/build/esp32dev/firmware.bin
```

**Release Checklist:**
- [ ] Version number incremented
- [ ] CHANGELOG.md updated
- [ ] Code reviewed for vulnerabilities
- [ ] Dependencies up to date
- [ ] Binary tested on hardware

#### 3. Certificate Management

**Updating CoinGecko Certificate (when expired in 2038):**

1. Fetch new certificate:
   ```bash
   openssl s_client -showcerts -connect api.coingecko.com:443 < /dev/null
   ```

2. Extract root CA certificate

3. Update in `src/main.cpp` lines 57-79

4. Test thoroughly before deploying

---

## Known Limitations

### 1. No Firmware Signature Verification

**Risk:** Malicious firmware could be installed if GitHub account is compromised

**Mitigation:**
- Enable 2FA on GitHub
- Use strong, unique passwords
- Monitor repository for unauthorized changes
- Consider implementing signature verification for production

### 2. No Encrypted Storage

**Risk:** WiFi credentials stored in plaintext in flash memory

**Impact:** Physical access to device allows credential extraction

**Mitigation:**
- Use strong WPA3 with unique password
- Enable flash encryption (advanced, requires ESP-IDF)
- Network isolation

### 3. No Authentication for Button Press

**Risk:** Anyone can toggle backlight

**Impact:** Minimal (only cosmetic)

### 4. API Key Exposure

**Current:** CoinGecko API doesn't require keys (good for privacy)

**If you add API keys in future:**
- Use ESP32 secure storage (NVS with encryption)
- Never hardcode in source
- Implement key rotation

---

## Vulnerability Reporting

### Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 1.0.3   | ✅ Yes             |
| 1.0.2   | ⚠️ Security issues |
| 1.0.1   | ❌ No              |
| < 1.0   | ❌ No              |

### Reporting a Vulnerability

**DO NOT** open a public GitHub issue for security vulnerabilities.

**Instead:**
1. Email: [Your email or create one for this project]
2. Include:
   - Description of vulnerability
   - Steps to reproduce
   - Potential impact
   - Suggested fix (if any)

**Response Time:**
- Acknowledgment: Within 48 hours
- Initial assessment: Within 7 days
- Fix timeline: Depends on severity

**Disclosure Policy:**
- Critical: Immediate patch, 30-day disclosure delay
- High: 60-day disclosure delay
- Medium/Low: 90-day disclosure delay

---

## Security Improvements Roadmap

### Planned (Future Versions)

- [ ] **Firmware signature verification** - Cryptographic signing of releases
- [ ] **Flash encryption** - Protect credentials at rest
- [ ] **Secure boot** - Prevent unauthorized firmware installation
- [ ] **Certificate pinning for GitHub** - Eliminate `setInsecure()` for OTA
- [ ] **Rate limiting for button presses** - Prevent abuse
- [ ] **Watchdog timer** - Automatic recovery from crashes
- [ ] **Encrypted configuration API** - Over-the-air credential updates

### Under Consideration

- [ ] **MQTT with TLS** - Secure remote monitoring
- [ ] **Web dashboard with authentication** - Configure device via HTTPS
- [ ] **Hardware security module (HSM)** - For critical deployments
- [ ] **Tamper detection** - Using ESP32 GPIO + enclosure switch

---

## Security Audit History

| Date | Version | Auditor | Findings | Status |
|------|---------|---------|----------|--------|
| 2025-01-05 | 1.0.3 | Internal | Certificate pinning added, secrets excluded | ✅ Fixed |
| 2025-01-XX | 1.0.0 | - | Initial release, no security review | - |

---

## Compliance & Certifications

### Current Status

- **FCC/CE:** Not certified (hobbyist project)
- **GDPR:** No personal data collected
- **CCPA:** No data collected
- **SOC2:** Not applicable

### Data Collection

**The firmware does NOT collect, store, or transmit:**
- Personal information
- User behavior analytics
- Location data
- Telemetry beyond necessary API calls

**External API Calls:**
1. **CoinGecko API** - Bitcoin price data (SSL pinned)
2. **GitHub API** - Firmware version checks (public API)

Both APIs can see your device's public IP address (standard for any HTTP request).

---

## Security Contact

For security-related questions or concerns:

- **GitHub Issues:** [General questions only, not vulnerabilities]
- **Email:** [Configure your security contact]
- **Security Policy:** This document (SECURITY.md)

---

## Acknowledgments

Security improvements thanks to:
- DigiCert for root CA certificates
- CoinGecko for free, no-auth API
- ESP32 community for security best practices

---

## License

This security policy is part of the Bitcoin Display project and follows the same license (MIT).

---

**Last Updated:** 2025-01-05
**Version:** 1.0.3
**Maintained By:** [Your name/username]
