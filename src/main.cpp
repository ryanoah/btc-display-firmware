/**
 * Bitcoin Live Price Display for LILYGO T-Display ESP32
 * Features: BTC/USD price display, WiFi, GitHub OTA updates
 * Version 1.3.1: 6-hour price updates, 24-hour firmware checks, WiFi power cycling
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include <Update.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include "secrets.h"

// ========== FIRMWARE VERSION ==========
#define FIRMWARE_VERSION "1.3.1"

// ========== POWER MANAGEMENT ==========
// NOTE: Device is encased without button access - display always on
// WiFi disconnects between updates to maximize battery life
#define CPU_FREQ_MHZ 80                // Run at 80MHz instead of 240MHz (saves ~30mA)

// ========== HARDWARE CONFIGURATION ==========
#define BACKLIGHT_PIN 4
#define BATTERY_PIN   34  // ADC pin for battery voltage
#define BACKLIGHT_PWM_CHANNEL 0
#define BACKLIGHT_FULL 16              // Very low brightness for maximum battery savings (was 32)
#define BATTERY_LOW_VOLTAGE 3.5       // Low battery warning threshold (volts)
#define BATTERY_CRITICAL_VOLTAGE 3.0  // Critical - shutdown to prevent damage (volts)
#define BATTERY_CHARGING_VOLTAGE 4.3  // Voltage indicating device is plugged in/charging
#define BATTERY_CHECK_INTERVAL 30000  // Check battery every 30 seconds

// ========== DISPLAY CONFIGURATION ==========
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 135

// ========== UPDATE INTERVALS ==========
#define PRICE_UPDATE_INTERVAL_BASE    21600000 // 6 hours
#define FIRMWARE_UPDATE_INTERVAL      86400000 // 24 hours

// ========== RETRY CONFIGURATION ==========
#define MAX_API_RETRIES 3
#define INITIAL_BACKOFF_MS 5000
#define MAX_BACKOFF_MS 60000

// ========== BUFFER SIZES ==========
#define PRICE_BUFFER_SIZE 512

// ========== COLOR SCHEME ==========
#define COLOR_BG       0x0000
#define COLOR_HEADER   0x0000  // Black header
#define COLOR_TEXT     0xFFFF
#define COLOR_GRID     0x2104
#define COLOR_CHART    0x07E0
#define COLOR_ERROR    0xF800
#define COLOR_WARNING  0xFD20

// ========== GLOBAL OBJECTS ==========
TFT_eSPI tft = TFT_eSPI();

// ========== STATE VARIABLES ==========
float currentPrice = 0.0;
unsigned long lastPriceUpdate = 0;
unsigned long lastFirmwareCheck = 0;
bool wifiConnected = false;
bool batteryLow = false;
bool batteryCritical = false;
float batteryVoltage = 0.0;
unsigned long lastBatteryCheck = 0;

// Plug-in detection (for display only)
bool isPluggedIn = false;
bool wasPluggedIn = false;

// Rate limiting state
unsigned long rateLimitBackoffUntil = 0;
int consecutiveApiFailures = 0;

// Dynamic intervals (to randomize slightly)
unsigned long PRICE_UPDATE_INTERVAL = PRICE_UPDATE_INTERVAL_BASE;

// ========== FUNCTION DECLARATIONS ==========
void connectWifi();
void disconnectWifi();
void stripChunkedEncoding(const char* raw, char* output, size_t maxLen);
bool fetchCurrentPrice(float& out);
void drawPrice(float price, bool netOk = true);
String formatPriceWithCommas(float price);
bool checkForFirmwareUpdate();
void performFirmwareUpdate(const String& firmwareUrl);
int calculateBackoff(int attempt);
void checkBattery();
bool checkIfPluggedIn();
void drawBatteryWarning();
void shutdownDevice(const String& reason);
void configurePowerSaving();
int compareSemanticVersion(const String& v1, const String& v2);

// ========== WIFI CONNECTION ==========
void connectWifi() {
  Serial.println("\n[WiFi] Connecting to " + String(WIFI_SSID));
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Connecting WiFi...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n[WiFi] Connected!");
    Serial.println("[WiFi] IP: " + WiFi.localIP().toString());

    tft.fillScreen(COLOR_BG);
    tft.drawString("WiFi Connected!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);
    delay(1000);
  } else {
    wifiConnected = false;
    Serial.println("\n[WiFi] Connection failed!");
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_ERROR, COLOR_BG);
    tft.drawString("WiFi Failed!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);
    delay(2000);
  }
}

void disconnectWifi() {
  if (wifiConnected) {
    Serial.println("[WiFi] Disconnecting to save power...");
    WiFi.disconnect(true);  // true = turn off WiFi radio
    WiFi.mode(WIFI_OFF);
    wifiConnected = false;
    Serial.println("[WiFi] Disconnected");
  }
}

// ========== EXPONENTIAL BACKOFF ==========
int calculateBackoff(int attempt) {
  if (attempt <= 0) return 0;

  int backoff = INITIAL_BACKOFF_MS * (1 << (attempt - 1)); // Exponential: 5s, 10s, 20s...
  if (backoff > MAX_BACKOFF_MS) backoff = MAX_BACKOFF_MS;

  // Add jitter (±20%)
  int jitter = random(-backoff / 5, backoff / 5);
  return backoff + jitter;
}

// ========== CHUNKED ENCODING PARSER (optimized, no String allocation) ==========
void stripChunkedEncoding(const char* raw, char* output, size_t maxLen) {
  if (!raw || !output || maxLen == 0) return;

  size_t rawLen = strlen(raw);
  if (rawLen == 0) {
    output[0] = '\0';
    return;
  }

  // Check if this looks like chunked encoding
  const char* firstLineEnd = strchr(raw, '\n');
  if (!firstLineEnd) {
    strncpy(output, raw, maxLen - 1);
    output[maxLen - 1] = '\0';
    return;
  }

  // Extract first line
  char firstLine[32];
  size_t firstLineLen = firstLineEnd - raw;
  if (firstLineLen >= sizeof(firstLine)) firstLineLen = sizeof(firstLine) - 1;
  strncpy(firstLine, raw, firstLineLen);
  firstLine[firstLineLen] = '\0';

  // Trim and check if it's a hex number
  char* endPtr;
  long firstChunkSize = strtol(firstLine, &endPtr, 16);
  if (endPtr == firstLine || (*endPtr != '\0' && *endPtr != '\r' && *endPtr != ' ')) {
    // Not chunked encoding, copy as-is
    strncpy(output, raw, maxLen - 1);
    output[maxLen - 1] = '\0';
    return;
  }

  // Parse chunked encoding
  size_t pos = 0;
  size_t outPos = 0;

  while (pos < rawLen && outPos < maxLen - 1) {
    const char* lineEnd = strchr(raw + pos, '\n');
    if (!lineEnd) break;

    // Extract chunk size line
    char chunkSizeLine[32];
    size_t lineLen = lineEnd - (raw + pos);
    if (lineLen >= sizeof(chunkSizeLine)) lineLen = sizeof(chunkSizeLine) - 1;
    strncpy(chunkSizeLine, raw + pos, lineLen);
    chunkSizeLine[lineLen] = '\0';

    long chunkSize = strtol(chunkSizeLine, NULL, 16);
    if (chunkSize == 0) break;

    pos = (lineEnd - raw) + 1;

    // Copy chunk data
    if (pos + chunkSize <= rawLen) {
      size_t copyLen = chunkSize;
      if (outPos + copyLen > maxLen - 1) copyLen = maxLen - 1 - outPos;
      memcpy(output + outPos, raw + pos, copyLen);
      outPos += copyLen;
      pos += chunkSize;
    } else {
      break;
    }

    // Skip CRLF after chunk
    if (pos < rawLen && raw[pos] == '\r') pos++;
    if (pos < rawLen && raw[pos] == '\n') pos++;
  }

  output[outPos] = '\0';
}

// ========== COINGECKO API FETCHERS (optimized with char buffers) ==========
bool fetchCurrentPrice(float& out) {
  // Check if we're in rate limit backoff period
  if (millis() < rateLimitBackoffUntil) {
    unsigned long remaining = (rateLimitBackoffUntil - millis()) / 1000;
    Serial.print("[API] Rate limit backoff active, ");
    Serial.print(remaining);
    Serial.println("s remaining");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();  // TODO: Fix certificate chain for CoinGecko

  const char* host = "api.coingecko.com";
  const int httpsPort = 443;
  const char* url = "/api/v3/simple/price?ids=bitcoin&vs_currencies=usd";

  Serial.println("[API] Fetching current price...");

  if (!client.connect(host, httpsPort)) {
    Serial.println("[API] Connection failed!");
    consecutiveApiFailures++;
    return false;
  }

  // Use char buffer for request
  char request[256];
  snprintf(request, sizeof(request),
           "GET %s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "User-Agent: ESP32-Bitcoin-Display/%s\r\n"
           "Accept: application/json\r\n"
           "Accept-Encoding: identity\r\n"
           "Connection: close\r\n\r\n",
           url, host, FIRMWARE_VERSION);
  client.print(request);

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println("[API] Timeout!");
      client.stop();
      consecutiveApiFailures++;
      return false;
    }
  }

  // Skip headers
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) break;
  }

  // Read response into char buffer (no String allocation)
  char rawPayload[PRICE_BUFFER_SIZE];
  size_t bytesRead = 0;

  while (client.available() && bytesRead < PRICE_BUFFER_SIZE - 1) {
    rawPayload[bytesRead++] = client.read();
  }
  rawPayload[bytesRead] = '\0';
  client.stop();

  // Strip chunked transfer encoding (in-place)
  char payload[PRICE_BUFFER_SIZE];
  stripChunkedEncoding(rawPayload, payload, PRICE_BUFFER_SIZE);

  // Check for rate limiting
  if (strstr(payload, "rate limit") != NULL || strstr(payload, "429") != NULL) {
    Serial.println("[API] ⚠️ Rate limit detected!");
    rateLimitBackoffUntil = millis() + 60000; // Back off for 60 seconds
    consecutiveApiFailures++;
    return false;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("[API] JSON parse failed: ");
    Serial.println(error.c_str());
    consecutiveApiFailures++;
    return false;
  }

  if (doc.containsKey("bitcoin") && doc["bitcoin"].containsKey("usd")) {
    out = doc["bitcoin"]["usd"].as<float>();
    Serial.print("[API] Price: $");
    Serial.println(out, 2);
    consecutiveApiFailures = 0; // Reset failure counter on success
    return true;
  }

  Serial.println("[API] Invalid JSON structure!");
  consecutiveApiFailures++;
  return false;
}


// ========== SEMANTIC VERSION COMPARISON ==========
/**
 * Compare two semantic version strings (e.g., "1.2.3" vs "1.10.0")
 * Returns: -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
 */
int compareSemanticVersion(const String& v1, const String& v2) {
  // Parse version components
  int v1Major = 0, v1Minor = 0, v1Patch = 0;
  int v2Major = 0, v2Minor = 0, v2Patch = 0;

  // Parse v1
  int firstDot = v1.indexOf('.');
  int secondDot = v1.indexOf('.', firstDot + 1);
  if (firstDot > 0) {
    v1Major = v1.substring(0, firstDot).toInt();
    if (secondDot > firstDot) {
      v1Minor = v1.substring(firstDot + 1, secondDot).toInt();
      v1Patch = v1.substring(secondDot + 1).toInt();
    } else {
      v1Minor = v1.substring(firstDot + 1).toInt();
    }
  } else {
    v1Major = v1.toInt();
  }

  // Parse v2
  firstDot = v2.indexOf('.');
  secondDot = v2.indexOf('.', firstDot + 1);
  if (firstDot > 0) {
    v2Major = v2.substring(0, firstDot).toInt();
    if (secondDot > firstDot) {
      v2Minor = v2.substring(firstDot + 1, secondDot).toInt();
      v2Patch = v2.substring(secondDot + 1).toInt();
    } else {
      v2Minor = v2.substring(firstDot + 1).toInt();
    }
  } else {
    v2Major = v2.toInt();
  }

  // Compare major version
  if (v1Major != v2Major) {
    return (v1Major < v2Major) ? -1 : 1;
  }

  // Compare minor version
  if (v1Minor != v2Minor) {
    return (v1Minor < v2Minor) ? -1 : 1;
  }

  // Compare patch version
  if (v1Patch != v2Patch) {
    return (v1Patch < v2Patch) ? -1 : 1;
  }

  // Versions are equal
  return 0;
}

// ========== GITHUB OTA FUNCTIONS ==========
bool checkForFirmwareUpdate() {
  Serial.println("\n[OTA] Checking for firmware updates...");

  WiFiClientSecure client;
  client.setInsecure(); // GitHub uses Let's Encrypt, which can be tricky on ESP32

  const char* host = "api.github.com";
  const int httpsPort = 443;
  const String url = "/repos/" + String(GITHUB_REPO) + "/releases/latest";

  if (!client.connect(host, httpsPort)) {
    Serial.println("[OTA] Failed to connect to GitHub API");
    return false;
  }

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP32-Bitcoin-Display/" + String(FIRMWARE_VERSION) + "\r\n" +
               "Accept: application/vnd.github.v3+json\r\n" +
               "Connection: close\r\n\r\n");

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 10000) {
      Serial.println("[OTA] Timeout!");
      client.stop();
      return false;
    }
  }

  // Skip headers
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) break;
  }

  // Read JSON payload
  String payload = "";
  payload.reserve(4096);
  while (client.available()) {
    payload += (char)client.read();
  }
  client.stop();

  // Parse JSON
  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.println("[OTA] JSON parse failed: " + String(error.c_str()));
    return false;
  }

  String latestVersion = doc["tag_name"].as<String>();

  // Remove 'v' prefix if present (e.g., "v1.0.3" -> "1.0.3")
  if (latestVersion.startsWith("v") || latestVersion.startsWith("V")) {
    latestVersion = latestVersion.substring(1);
  }

  Serial.println("[OTA] Current version: " + String(FIRMWARE_VERSION));
  Serial.println("[OTA] Latest version: " + latestVersion);

  // Semantic version comparison (handles versions like 1.2.0 vs 1.10.0 correctly)
  if (latestVersion.length() > 0) {
    int versionCompare = compareSemanticVersion(String(FIRMWARE_VERSION), latestVersion);

    if (versionCompare < 0) {
      // Current version is older than latest version
      Serial.println("[OTA] 🆕 New version available!");

    // Find the firmware.bin asset
    JsonArray assets = doc["assets"].as<JsonArray>();
    for (JsonObject asset : assets) {
      String name = asset["name"].as<String>();
      if (name == "firmware.bin") {
        String downloadUrl = asset["browser_download_url"].as<String>();
        Serial.println("[OTA] Found firmware: " + name);
        Serial.println("[OTA] URL: " + downloadUrl);
        performFirmwareUpdate(downloadUrl);
        return true;
      }
    }

      Serial.println("[OTA] ⚠️ No firmware.bin found in release assets!");
      return false;
    } else if (versionCompare > 0) {
      // Current version is newer than latest release (dev build?)
      Serial.println("[OTA] ℹ️ Current version is newer than latest release (development build?)");
      return false;
    } else {
      // Versions are equal
      Serial.println("[OTA] ✅ Firmware is up to date");
      return false;
    }
  } else {
    Serial.println("[OTA] ⚠️ Invalid version format from GitHub");
    return false;
  }
}

void performFirmwareUpdate(const String& firmwareUrl) {
  Serial.println("[OTA] Starting firmware update...");
  Serial.println("[OTA] URL: " + firmwareUrl);

  // Show update screen
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_WARNING, COLOR_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("FIRMWARE UPDATE", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 20, 4);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.drawString("Downloading...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 10, 2);
  tft.setTextColor(COLOR_ERROR, COLOR_BG);
  tft.drawString("DO NOT POWER OFF", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 35, 2);

  WiFiClientSecure client;
  client.setInsecure(); // GitHub uses Let's Encrypt

  // Update progress callback
  httpUpdate.onProgress([](int current, int total) {
    if (total > 0) {
      int percent = (current * 100) / total;
      Serial.printf("[OTA] Progress: %d%%\n", percent);

      // Draw progress bar
      int barWidth = 200;
      int barHeight = 10;
      int barX = (SCREEN_WIDTH - barWidth) / 2;
      int barY = SCREEN_HEIGHT / 2 + 50;

      tft.drawRect(barX, barY, barWidth, barHeight, COLOR_TEXT);
      tft.fillRect(barX + 2, barY + 2, (barWidth - 4) * percent / 100, barHeight - 4, COLOR_CHART);
    }
  });

  // Perform the update
  t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("[OTA] ❌ Update failed. Error (%d): %s\n",
                    httpUpdate.getLastError(),
                    httpUpdate.getLastErrorString().c_str());

      tft.fillScreen(COLOR_BG);
      tft.setTextColor(COLOR_ERROR, COLOR_BG);
      tft.drawString("UPDATE FAILED!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 10, 4);
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString(httpUpdate.getLastErrorString(), SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 20, 2);
      delay(5000);
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("[OTA] ℹ️ No update needed");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("[OTA] ✅ Update successful! Rebooting...");
      tft.fillScreen(COLOR_BG);
      tft.setTextColor(COLOR_CHART, COLOR_BG);
      tft.drawString("UPDATE COMPLETE!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 10, 4);
      tft.drawString("Rebooting...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 20, 2);
      delay(2000);
      ESP.restart();
      break;
  }
}

// ========== DISPLAY ==========
String formatPriceWithCommas(float price) {
  // Convert price to integer
  int priceInt = (int)price;

  // Convert to string
  String priceStr = String(priceInt);
  String result = "";

  int len = priceStr.length();
  for (int i = 0; i < len; i++) {
    if (i > 0 && (len - i) % 3 == 0) {
      result += ",";
    }
    result += priceStr.charAt(i);
  }

  return "$" + result;
}

void drawPrice(float price, bool netOk) {
  // Clear screen
  tft.fillScreen(COLOR_BG);

  if (netOk && price > 0) {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    String priceWithCommas = formatPriceWithCommas(price);

    // Remove the $ from the formatted price
    String priceNumbers = priceWithCommas.substring(1);  // Remove leading "$"

    int centerY = SCREEN_HEIGHT / 2;

    // Draw price numbers centered in font 6
    tft.setTextDatum(MC_DATUM);  // Middle-center alignment
    tft.drawString(priceNumbers, SCREEN_WIDTH / 2, centerY, 6);

  } else {
    tft.setTextColor(COLOR_ERROR, COLOR_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("NO DATA", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 4);
  }

  // Draw battery warning if needed
  drawBatteryWarning();
}

// ========== MAIN ==========
void setup() {
  Serial.begin(115200);
  Serial.print("\n\n=== Bitcoin Live Display v");
  Serial.print(FIRMWARE_VERSION);
  Serial.println(" ===");
  Serial.println("=== BATTERY OPTIMIZED MODE (Low backlight + WiFi power cycling) ===");

  // Configure power saving FIRST (before any high-power operations)
  configurePowerSaving();

  randomSeed(esp_random());
  PRICE_UPDATE_INTERVAL = PRICE_UPDATE_INTERVAL_BASE + random(0, 10000);

  ledcSetup(BACKLIGHT_PWM_CHANNEL, 5000, 8);
  ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_PWM_CHANNEL);
  ledcWrite(BACKLIGHT_PWM_CHANNEL, BACKLIGHT_FULL);  // Turn on backlight at low brightness
  pinMode(BATTERY_PIN, INPUT);  // Configure battery ADC pin
  analogReadResolution(12);      // 12-bit ADC resolution (0-4095)
  checkBattery();                // Initial battery check

  tft.init(); tft.setRotation(1); tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("BTC Display", SCREEN_WIDTH/2, SCREEN_HEIGHT/2-15, 4);
  char versionStr[32];
  snprintf(versionStr, sizeof(versionStr), "v%s", FIRMWARE_VERSION);
  tft.drawString(versionStr, SCREEN_WIDTH/2, SCREEN_HEIGHT/2+15, 2);
  delay(1000);

  connectWifi();

  if (wifiConnected) {
    tft.fillScreen(COLOR_BG);
    tft.drawString("Loading...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);

    // Fetch current price immediately
    Serial.println("[INIT] Fetching current price...");
    if (fetchCurrentPrice(currentPrice)) {
      Serial.println("[INIT] Price fetched successfully");
      drawPrice(currentPrice, true);
    } else {
      Serial.println("[INIT] Price fetch failed");
      drawPrice(0, false);
    }

    // Disconnect WiFi to save power
    disconnectWifi();

    lastPriceUpdate = millis();
    lastFirmwareCheck = millis();

  } else {
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_ERROR, COLOR_BG);
    tft.drawString("WiFi Error!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 4);
  }

  Serial.println("\n[INIT] Setup complete!");
  Serial.println("[INIT] Device ready - WiFi will reconnect for updates");
  Serial.print("[INIT] Price update interval: ");
  Serial.print(PRICE_UPDATE_INTERVAL_BASE / 3600000);
  Serial.println(" hours");
  Serial.print("[INIT] Firmware update interval: ");
  Serial.print(FIRMWARE_UPDATE_INTERVAL / 3600000);
  Serial.println(" hours");
}

void loop() {
  unsigned long now = millis();

  // --- Battery check every 30 seconds ---
  if (now - lastBatteryCheck >= BATTERY_CHECK_INTERVAL) {
    checkBattery();
    lastBatteryCheck = now;
    if (batteryLow || batteryCritical) {
      drawBatteryWarning();
    }

    // Check if device is plugged in (for battery display indicator)
    wasPluggedIn = isPluggedIn;
    isPluggedIn = checkIfPluggedIn();

    // Detect plug-in event (transition from unplugged to plugged)
    if (isPluggedIn && !wasPluggedIn) {
      Serial.println("\n[POWER] 🔌 Device plugged in detected!");
      Serial.print("[POWER] Voltage: ");
      Serial.print(batteryVoltage, 2);
      Serial.println("V (charging)");
      drawBatteryWarning();  // Update display to show charging indicator
    }

    // Detect unplug event
    if (!isPluggedIn && wasPluggedIn) {
      Serial.println("\n[POWER] 🔋 Device unplugged - running on battery");
      Serial.print("[POWER] Voltage: ");
      Serial.print(batteryVoltage, 2);
      Serial.println("V");
      drawBatteryWarning();  // Update display to clear charging indicator
    }
  }

  // Check if any updates are needed
  bool needsPriceUpdate = (now - lastPriceUpdate >= PRICE_UPDATE_INTERVAL);
  bool needsFirmwareUpdate = (now - lastFirmwareCheck >= FIRMWARE_UPDATE_INTERVAL);

  // --- Price update ---
  if (needsPriceUpdate) {
    Serial.println("\n[UPDATE] Fetching price...");

    // Reconnect WiFi for price update
    if (!wifiConnected) {
      connectWifi();
    }

    if (wifiConnected) {
      // Apply exponential backoff if there have been consecutive failures
      if (consecutiveApiFailures > 0) {
        int backoff = calculateBackoff(consecutiveApiFailures);
        Serial.print("[API] Applying backoff: ");
        Serial.print(backoff / 1000);
        Serial.print("s (attempt ");
        Serial.print(consecutiveApiFailures + 1);
        Serial.println(")");

        // Non-blocking backoff
        unsigned long backoffUntil = millis() + backoff;
        while (millis() < backoffUntil) {
          checkBattery();
          delay(100);
        }
      }

      bool success = fetchCurrentPrice(currentPrice);

      if (success) {
        drawPrice(currentPrice, true);
      } else {
        drawPrice(currentPrice, false);
      }

      // Disconnect WiFi to save power
      disconnectWifi();
    }

    lastPriceUpdate = now;
    PRICE_UPDATE_INTERVAL = PRICE_UPDATE_INTERVAL_BASE + random(0, 10000);
  }

  // --- Firmware update check ---
  if (needsFirmwareUpdate) {
    Serial.println("\n[UPDATE] Checking for firmware updates...");

    // Reconnect WiFi for firmware update check
    if (!wifiConnected) {
      connectWifi();
    }

    if (wifiConnected) {
      checkForFirmwareUpdate();

      // Disconnect WiFi to save power (only if update didn't happen - update restarts device)
      disconnectWifi();
    }

    lastFirmwareCheck = now;
  }

  delay(100);  // Small delay to prevent busy-waiting
}

// ========== CPU AND POWER OPTIMIZATION ==========
void configurePowerSaving() {
  Serial.println("\n[POWER] Configuring power-saving features...");

  // Set CPU frequency to save power
  setCpuFrequencyMhz(CPU_FREQ_MHZ);
  Serial.print("[POWER] CPU frequency set to ");
  Serial.print(CPU_FREQ_MHZ);
  Serial.println("MHz (saves ~30mA vs 240MHz)");

  Serial.println("[POWER] Power management initialized");
  Serial.println("[POWER] Display always-on mode (encased device)");
  Serial.println("[POWER] WiFi disconnects between updates to save power");
}

void checkBattery() {
  // Read battery voltage from ADC
  // LILYGO T-Display has voltage divider (2:1), ADC range 0-4095 for 0-3.3V
  // Actual battery voltage = (ADC_value / 4095) * 3.3V * 2
  int adcValue = analogRead(BATTERY_PIN);
  batteryVoltage = (adcValue / 4095.0) * 3.3 * 2.0;

  // Sanity check - LiPo batteries never exceed 4.2V when fully charged
  if (batteryVoltage > 4.5) {
    Serial.println("[BATTERY] ERROR: Invalid voltage reading: " + String(batteryVoltage, 2) + "V");
    Serial.println("[BATTERY] ADC value: " + String(adcValue) + " (possible hardware issue)");
    return;  // Ignore erroneous readings
  }

  // Ignore very low readings (< 0.5V indicates disconnected or no battery)
  if (batteryVoltage < 0.5) {
    batteryLow = false;
    batteryCritical = false;
    return;
  }

  // Check for critical battery level (immediate shutdown required)
  if (batteryVoltage < BATTERY_CRITICAL_VOLTAGE) {
    batteryCritical = true;
    Serial.println("[BATTERY] ⚠️ CRITICAL: " + String(batteryVoltage, 2) + "V - Shutting down to prevent damage!");
    shutdownDevice("Critical battery voltage: " + String(batteryVoltage, 2) + "V");
  }
  // Check for low battery warning
  else if (batteryVoltage < BATTERY_LOW_VOLTAGE) {
    if (!batteryLow) {  // Only log once when transitioning to low state
      Serial.println("[BATTERY] ⚠️ LOW: " + String(batteryVoltage, 2) + "V - Please charge soon!");
    }
    batteryLow = true;
    batteryCritical = false;
  }
  // Battery OK
  else {
    if (batteryLow) {  // Only log when recovering from low state
      Serial.println("[BATTERY] ✅ OK: " + String(batteryVoltage, 2) + "V - Battery recovered");
    }
    batteryLow = false;
    batteryCritical = false;
  }
}

// ========== PLUG-IN DETECTION ==========
bool checkIfPluggedIn() {
  // When plugged in via USB, voltage rises above normal battery max (4.2V)
  // Charging voltage typically reads 4.3-5.0V
  if (batteryVoltage > BATTERY_CHARGING_VOLTAGE) {
    return true;
  }
  return false;
}

void drawBatteryWarning() {
  if (batteryCritical) {
    // Critical: Red, blinking (will shut down)
    tft.setTextColor(COLOR_ERROR, COLOR_BG);
    tft.setTextDatum(TR_DATUM);
    tft.drawString("CRITICAL", SCREEN_WIDTH - 2, 2, 1);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(String(batteryVoltage, 2) + "V", SCREEN_WIDTH - 2, 12, 1);
  } else if (isPluggedIn) {
    // Plugged in: Green, show charging indicator
    tft.setTextColor(COLOR_CHART, COLOR_BG);
    tft.setTextDatum(TR_DATUM);
    tft.drawString("CHARGING", SCREEN_WIDTH - 2, 2, 1);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(String(batteryVoltage, 2) + "V", SCREEN_WIDTH - 2, 12, 1);
  } else if (batteryLow) {
    // Low: Yellow/Orange warning
    tft.setTextColor(COLOR_WARNING, COLOR_BG);
    tft.setTextDatum(TR_DATUM);
    tft.drawString("LOW", SCREEN_WIDTH - 2, 2, 1);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(String(batteryVoltage, 2) + "V", SCREEN_WIDTH - 2, 12, 1);
  } else {
    // Clear the warning area if battery is OK
    tft.fillRect(SCREEN_WIDTH - 50, 0, 50, 22, COLOR_BG);
  }
}

void shutdownDevice(const String& reason) {
  Serial.println("\n[SHUTDOWN] Device shutting down!");
  Serial.println("[SHUTDOWN] Reason: " + reason);
  Serial.println("[SHUTDOWN] Battery voltage: " + String(batteryVoltage, 2) + "V");
  Serial.println("[SHUTDOWN] To restart: Press RESET button or charge battery above " + String(BATTERY_CRITICAL_VOLTAGE, 1) + "V");

  // Display shutdown warning
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_ERROR, COLOR_BG);
  tft.setTextDatum(MC_DATUM);

  // Main warning
  tft.drawString("BATTERY CRITICAL", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 30, 4);

  // Voltage display
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.drawString("Voltage: " + String(batteryVoltage, 2) + "V", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);
  tft.drawString("Minimum: " + String(BATTERY_CRITICAL_VOLTAGE, 1) + "V", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 20, 2);

  // Instructions
  tft.setTextColor(COLOR_WARNING, COLOR_BG);
  tft.drawString("Shutting down in 5s...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 45, 2);

  // Countdown
  for (int i = 5; i > 0; i--) {
    tft.fillRect(0, SCREEN_HEIGHT / 2 + 65, SCREEN_WIDTH, 20, COLOR_BG);
    tft.setTextColor(COLOR_ERROR, COLOR_BG);
    tft.drawString(String(i), SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 65, 4);
    delay(1000);
  }

  // Final message
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.drawString("SHUTDOWN", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 15, 4);
  tft.drawString("Charge battery", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 15, 2);
  tft.drawString("Press RESET to restart", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 35, 2);

  delay(2000);

  // Turn off backlight to save power
  ledcWrite(BACKLIGHT_PWM_CHANNEL, 0);

  Serial.println("[SHUTDOWN] Entering deep sleep mode...");
  Serial.println("[SHUTDOWN] Device will wake only on RESET button press");
  Serial.flush();  // Ensure all serial data is sent

  // Enter deep sleep (effectively shuts down - requires RESET button or power cycle to wake)
  // Note: On ESP32, deep sleep draws ~10µA vs ~80mA active, protecting the battery
  esp_deep_sleep_start();
}
