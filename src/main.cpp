/**
 * Bitcoin Live Price Display for LILYGO T-Display ESP32
 * Features: BTC/USD price, 7-day chart, WiFi, GitHub OTA updates
 * Version 1.0.3: Security improvements, OTA implementation, optimizations
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include <Update.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <vector>
#include "secrets.h"

// ========== FIRMWARE VERSION ==========
#define FIRMWARE_VERSION "1.0.3"

// ========== HARDWARE CONFIGURATION ==========
#define BUTTON_PIN    35
#define BACKLIGHT_PIN 4
#define BATTERY_PIN   34  // ADC pin for battery voltage
#define BACKLIGHT_PWM_CHANNEL 0
#define BACKLIGHT_DIM  64
#define BACKLIGHT_FULL 255
#define BATTERY_LOW_VOLTAGE 3.3  // Low battery threshold (volts)

// ========== DISPLAY CONFIGURATION ==========
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 135
#define HEADER_HEIGHT 28
#define CHART_X       0
#define CHART_Y       28
#define CHART_WIDTH   240
#define CHART_HEIGHT  107  // Full screen minus header (135-28)

// ========== UPDATE INTERVALS ==========
#define PRICE_UPDATE_INTERVAL_BASE    60000    // 60 seconds
#define CHART_UPDATE_INTERVAL_BASE    900000   // 15 minutes
#define FIRMWARE_UPDATE_INTERVAL      3600000  // 1 hour

// ========== RETRY CONFIGURATION ==========
#define MAX_API_RETRIES 3
#define INITIAL_BACKOFF_MS 5000
#define MAX_BACKOFF_MS 60000

// ========== COLOR SCHEME ==========
#define COLOR_BG       0x0000
#define COLOR_HEADER   0x0000  // Black header
#define COLOR_TEXT     0xFFFF
#define COLOR_GRID     0x2104
#define COLOR_CHART    0x07E0
#define COLOR_ERROR    0xF800
#define COLOR_WARNING  0xFD20

// ========== SSL CERTIFICATE (CoinGecko) ==========
// Root CA certificate for api.coingecko.com (DigiCert Global Root G2)
const char* coingecko_root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n" \
"MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n" \
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n" \
"2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n" \
"1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n" \
"q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n" \
"tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n" \
"vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n" \
"BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n" \
"5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n" \
"1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n" \
"NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n" \
"Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n" \
"8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n" \
"pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n" \
"MrY=\n" \
"-----END CERTIFICATE-----\n";

// ========== GLOBAL OBJECTS ==========
TFT_eSPI tft = TFT_eSPI();

// ========== STATE VARIABLES ==========
float currentPrice = 0.0;
std::vector<float> weekPrices;
unsigned long lastPriceUpdate = 0;
unsigned long lastChartUpdate = 0;
unsigned long lastFirmwareCheck = 0;
bool wifiConnected = false;
bool backlightBright = true;
bool batteryLow = false;
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 200;

// Rate limiting state
unsigned long rateLimitBackoffUntil = 0;
int consecutiveApiFailures = 0;

// Display state tracking (to avoid unnecessary redraws)
bool chartNeedsRedraw = true;
bool priceNeedsRedraw = true;

// Dynamic intervals (to randomize slightly)
unsigned long PRICE_UPDATE_INTERVAL = PRICE_UPDATE_INTERVAL_BASE;
unsigned long CHART_UPDATE_INTERVAL = CHART_UPDATE_INTERVAL_BASE;

// ========== FUNCTION DECLARATIONS ==========
void connectWifi();
String stripChunkedEncoding(const String& raw);
bool fetchCurrentPrice(float& out);
bool fetchWeekPrices(std::vector<float>& out);
void drawHeader();
void drawPrice(const String& price, bool netOk = true);
void drawGrid(int x, int y, int w, int h);
void drawSeries(const std::vector<float>& s, int x, int y, int w, int h);
void drawChartLabels(const std::vector<float>& s, int x, int y, int w, int h);
String formatPrice(float price);
bool checkForFirmwareUpdate();
void performFirmwareUpdate(const String& firmwareUrl);
void handleButton();
void setBacklight(bool bright);
int calculateBackoff(int attempt);
void checkBattery();
void drawBatteryWarning();

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

// ========== EXPONENTIAL BACKOFF ==========
int calculateBackoff(int attempt) {
  if (attempt <= 0) return 0;

  int backoff = INITIAL_BACKOFF_MS * (1 << (attempt - 1)); // Exponential: 5s, 10s, 20s...
  if (backoff > MAX_BACKOFF_MS) backoff = MAX_BACKOFF_MS;

  // Add jitter (±20%)
  int jitter = random(-backoff / 5, backoff / 5);
  return backoff + jitter;
}

// ========== CHUNKED ENCODING PARSER ==========
String stripChunkedEncoding(const String& raw) {
  if (raw.length() == 0) return "";

  // Check if this looks like chunked encoding
  int firstLineEnd = raw.indexOf('\n');
  if (firstLineEnd == -1) return raw;

  String firstLine = raw.substring(0, firstLineEnd);
  firstLine.trim();

  char* endPtr;
  long firstChunkSize = strtol(firstLine.c_str(), &endPtr, 16);

  if (endPtr == firstLine.c_str() || (*endPtr != '\0' && *endPtr != '\r' && *endPtr != ' ')) {
    return raw;
  }

  // Looks like chunked encoding, proceed with parsing
  String result = "";
  result.reserve(raw.length()); // Pre-allocate to reduce reallocations
  int pos = 0;

  while (pos < raw.length()) {
    int lineEnd = raw.indexOf('\n', pos);
    if (lineEnd == -1) break;

    String chunkSizeLine = raw.substring(pos, lineEnd);
    chunkSizeLine.trim();

    long chunkSize = strtol(chunkSizeLine.c_str(), NULL, 16);

    if (chunkSize == 0) break;

    pos = lineEnd + 1;

    if (pos + chunkSize <= raw.length()) {
      result += raw.substring(pos, pos + chunkSize);
      pos += chunkSize;
    } else {
      break;
    }

    if (pos < raw.length() && raw.charAt(pos) == '\r') pos++;
    if (pos < raw.length() && raw.charAt(pos) == '\n') pos++;
  }

  return result;
}

// ========== COINGECKO API FETCHERS ==========
bool fetchCurrentPrice(float& out) {
  // Check if we're in rate limit backoff period
  if (millis() < rateLimitBackoffUntil) {
    unsigned long remaining = (rateLimitBackoffUntil - millis()) / 1000;
    Serial.println("[API] Rate limit backoff active, " + String(remaining) + "s remaining");
    return false;
  }

  WiFiClientSecure client;
  client.setCACert(coingecko_root_ca);  // Use certificate pinning

  const char* host = "api.coingecko.com";
  const int httpsPort = 443;
  const String url = "/api/v3/simple/price?ids=bitcoin&vs_currencies=usd";

  Serial.println("[API] Fetching current price...");

  if (!client.connect(host, httpsPort)) {
    Serial.println("[API] Connection failed!");
    consecutiveApiFailures++;
    return false;
  }

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP32-Bitcoin-Display/" + String(FIRMWARE_VERSION) + "\r\n" +
               "Accept: application/json\r\n" +
               "Accept-Encoding: identity\r\n" +
               "Connection: close\r\n\r\n");

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println("[API] Timeout!");
      client.stop();
      consecutiveApiFailures++;
      return false;
    }
  }

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) break;
  }

  // Optimized: Pre-allocate buffer to reduce memory fragmentation
  String rawPayload = "";
  rawPayload.reserve(512); // Most price responses are < 512 bytes

  while (client.available()) {
    char c = client.read();
    rawPayload += c;
  }
  client.stop();

  // Strip chunked transfer encoding
  String payload = stripChunkedEncoding(rawPayload);

  // Check for rate limiting
  if (payload.indexOf("rate limit") != -1 || payload.indexOf("429") != -1) {
    Serial.println("[API] ⚠️ Rate limit detected!");
    rateLimitBackoffUntil = millis() + 60000; // Back off for 60 seconds
    consecutiveApiFailures++;
    return false;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.println("[API] JSON parse failed: " + String(error.c_str()));
    consecutiveApiFailures++;
    return false;
  }

  if (doc.containsKey("bitcoin") && doc["bitcoin"].containsKey("usd")) {
    out = doc["bitcoin"]["usd"].as<float>();
    Serial.println("[API] Price: $" + String(out, 2));
    consecutiveApiFailures = 0; // Reset failure counter on success
    return true;
  }

  Serial.println("[API] Invalid JSON structure!");
  consecutiveApiFailures++;
  return false;
}

bool fetchWeekPrices(std::vector<float>& out) {
  // Check if we're in rate limit backoff period
  if (millis() < rateLimitBackoffUntil) {
    unsigned long remaining = (rateLimitBackoffUntil - millis()) / 1000;
    Serial.println("[API] Rate limit backoff active, " + String(remaining) + "s remaining");
    return false;
  }

  WiFiClientSecure client;
  client.setCACert(coingecko_root_ca);  // Use certificate pinning

  const char* host = "api.coingecko.com";
  const int httpsPort = 443;
  const String url = "/api/v3/coins/bitcoin/market_chart?vs_currency=usd&days=7&interval=daily";

  Serial.println("[API] Fetching 7-day daily chart data...");

  if (!client.connect(host, httpsPort)) {
    Serial.println("[API] Connection failed!");
    return false;
  }

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP32-Bitcoin-Display/" + String(FIRMWARE_VERSION) + "\r\n" +
               "Accept: application/json\r\n" +
               "Accept-Encoding: identity\r\n" +
               "Connection: close\r\n\r\n");

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 10000) {
      Serial.println("[API] Timeout!");
      client.stop();
      return false;
    }
  }

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) break;
  }

  // Optimized: Pre-allocate larger buffer for chart data
  String rawPayload = "";
  rawPayload.reserve(8192); // Chart data can be several KB

  while (client.available()) {
    char c = client.read();
    rawPayload += c;
  }
  client.stop();

  // Strip chunked transfer encoding
  String payload = stripChunkedEncoding(rawPayload);

  // Check for rate limiting
  if (payload.indexOf("rate limit") != -1 || payload.indexOf("429") != -1) {
    Serial.println("[API] ⚠️ Rate limit detected on chart endpoint!");
    rateLimitBackoffUntil = millis() + 60000; // Back off for 60 seconds
    return false;
  }

  DynamicJsonDocument doc(16384); // Reduced from 32768 (still plenty for daily data)
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.println("[API] JSON parse failed: " + String(error.c_str()));
    return false;
  }

  if (!doc.containsKey("prices")) {
    Serial.println("[API] No 'prices' array in response!");
    return false;
  }

  JsonArray prices = doc["prices"].as<JsonArray>();
  out.clear();

  // Get all daily data points
  for (JsonArray pricePoint : prices) {
    if (pricePoint.size() >= 2) {
      float price = pricePoint[1].as<float>();
      out.push_back(price);
    }
  }

  Serial.println("[API] Got " + String(out.size()) + " daily data points");
  return out.size() > 0;
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
  if (latestVersion.startsWith("v")) {
    latestVersion = latestVersion.substring(1);
  }

  Serial.println("[OTA] Current version: " + String(FIRMWARE_VERSION));
  Serial.println("[OTA] Latest version: " + latestVersion);

  // Simple version comparison (works for semantic versioning)
  if (latestVersion != String(FIRMWARE_VERSION) && latestVersion.length() > 0) {
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
  } else {
    Serial.println("[OTA] ✅ Firmware is up to date");
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
void drawHeader() {
  tft.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COLOR_HEADER);
  tft.setTextColor(COLOR_TEXT, COLOR_HEADER);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("BTC", SCREEN_WIDTH / 2, HEADER_HEIGHT / 2, 2);
  drawBatteryWarning();  // Draw battery warning if needed
}

void drawPrice(const String& price, bool netOk) {
  // Draw price slightly below center (transparent, over the chart background)
  tft.setTextDatum(MC_DATUM);

  if (netOk && price.length() > 0) {
    tft.setTextColor(COLOR_TEXT);  // No background - transparent text
    tft.drawString("$" + price, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 8, 6);  // Moved down 8px, font size 6
  } else {
    tft.setTextColor(COLOR_ERROR);  // No background - transparent text
    tft.drawString("NO DATA", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 8, 6);
  }
}

void drawGrid(int x, int y, int w, int h) {
  for (int i = 1; i < 4; i++) {
    int yPos = y + (h * i / 4);
    tft.drawLine(x, yPos, x + w - 1, yPos, COLOR_GRID);
  }
  for (int i = 1; i < 7; i++) {
    int xPos = x + (w * i / 7);
    tft.drawLine(xPos, y, xPos, y + h - 1, COLOR_GRID);
  }
}

void drawSeries(const std::vector<float>& s, int x, int y, int w, int h) {
  if (s.size() < 2) return;
  float minPrice = s[0], maxPrice = s[0];
  for (float p : s) { if (p < minPrice) minPrice = p; if (p > maxPrice) maxPrice = p; }
  float range = maxPrice - minPrice; if (range < 1.0) range = 1.0;
  minPrice -= range * 0.05; maxPrice += range * 0.05; range = maxPrice - minPrice;
  for (size_t i = 1; i < s.size(); i++) {
    int x1 = x + (w * (i - 1)) / (s.size() - 1);
    int y1 = y + h - (int)((s[i - 1] - minPrice) / range * h);
    int x2 = x + (w * i) / (s.size() - 1);
    int y2 = y + h - (int)((s[i] - minPrice) / range * h);

    // Color line based on price movement: green if up, red if down
    uint16_t lineColor = (s[i] >= s[i - 1]) ? COLOR_CHART : COLOR_ERROR;
    tft.drawLine(x1, y1, x2, y2, lineColor);
  }
}

String formatPrice(float price) {
  if (price >= 1000.0) {
    int k = (int)(price / 1000.0);
    return String(k) + "k";
  }
  return String((int)price);
}

void drawChartLabels(const std::vector<float>& s, int x, int y, int w, int h) {
  if (s.size() < 2) return;

  // Calculate min/max prices (same logic as drawSeries)
  float minPrice = s[0], maxPrice = s[0];
  for (float p : s) { if (p < minPrice) minPrice = p; if (p > maxPrice) maxPrice = p; }
  float range = maxPrice - minPrice; if (range < 1.0) range = 1.0;
  minPrice -= range * 0.05; maxPrice += range * 0.05;

  // Draw price labels on the left side
  tft.setTextColor(COLOR_GRID, COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);

  // Max price at top
  tft.drawString(formatPrice(maxPrice), x + 2, y + 2, 1);

  // Min price at bottom
  tft.drawString(formatPrice(minPrice), x + 2, y + h - 10, 1);

  // Day markers at bottom (1 data point per day for daily intervals)
  tft.setTextDatum(TC_DATUM);
  for (int day = 0; day < s.size() && day <= 7; day++) {
    int xPos = x + (w * day) / (s.size() - 1);
    tft.drawString(String(day), xPos, y + h - 9, 1);
  }
}

// ========== MAIN ==========
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== Bitcoin Live Display v" + String(FIRMWARE_VERSION) + " ===");

  randomSeed(esp_random());
  PRICE_UPDATE_INTERVAL = PRICE_UPDATE_INTERVAL_BASE + random(0, 10000);
  CHART_UPDATE_INTERVAL = CHART_UPDATE_INTERVAL_BASE + random(0, 30000);

  ledcSetup(BACKLIGHT_PWM_CHANNEL, 5000, 8);
  ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_PWM_CHANNEL);
  setBacklight(true);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(BATTERY_PIN, INPUT);  // Configure battery ADC pin
  analogReadResolution(12);      // 12-bit ADC resolution (0-4095)
  checkBattery();                // Initial battery check

  tft.init(); tft.setRotation(1); tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("BTC Display", SCREEN_WIDTH/2, SCREEN_HEIGHT/2-15, 4);
  tft.drawString("v" + String(FIRMWARE_VERSION), SCREEN_WIDTH/2, SCREEN_HEIGHT/2+15, 2);
  delay(1000);

  connectWifi();

  if (wifiConnected) {
    tft.fillScreen(COLOR_BG);
    tft.drawString("Loading data...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);

    // Fetch current price immediately
    if (fetchCurrentPrice(currentPrice)) {
      priceNeedsRedraw = true;
    }

    // Draw header with BTC label
    drawHeader();

    // Wait 20 seconds before fetching chart data (rate limiting)
    tft.drawString("Waiting for chart...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 20, 1);
    delay(20000);

    // Fetch 7-day chart data
    if (fetchWeekPrices(weekPrices)) {
      chartNeedsRedraw = true;
    }

    // Initial draw
    if (chartNeedsRedraw) {
      tft.fillRect(CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT, COLOR_BG);
      drawGrid(CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT);
      drawSeries(weekPrices, CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT);
      drawChartLabels(weekPrices, CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT);
      chartNeedsRedraw = false;
    }

    if (priceNeedsRedraw) {
      drawPrice(String(currentPrice, 2), currentPrice > 0);
      priceNeedsRedraw = false;
    }

    lastChartUpdate = millis();
    lastPriceUpdate = millis();
    lastFirmwareCheck = millis();

  } else {
    tft.fillScreen(COLOR_BG);
    drawHeader();
    tft.setTextColor(COLOR_ERROR, COLOR_BG);
    tft.drawString("WiFi Error!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 4);
  }

  Serial.println("\n[INIT] Setup complete!");
}

void loop() {
  handleButton();
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      Serial.println("[WiFi] Lost connection — reconnecting...");
      wifiConnected = false;
      connectWifi();
    }
    delay(1000);
    return;
  }

  unsigned long now = millis();

  // --- Price update with exponential backoff on failure ---
  if (now - lastPriceUpdate >= PRICE_UPDATE_INTERVAL) {
    Serial.println("\n[UPDATE] Fetching price...");

    // Apply exponential backoff if there have been consecutive failures
    if (consecutiveApiFailures > 0) {
      int backoff = calculateBackoff(consecutiveApiFailures);
      Serial.println("[API] Applying backoff: " + String(backoff / 1000) + "s (attempt " + String(consecutiveApiFailures + 1) + ")");
      delay(backoff);
    }

    bool success = fetchCurrentPrice(currentPrice);

    if (success) {
      // Only redraw chart + price when price updates
      tft.fillRect(CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT, COLOR_BG);

      if (weekPrices.size() > 0) {
        drawGrid(CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT);
        drawSeries(weekPrices, CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT);
        drawChartLabels(weekPrices, CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT);
      }

      // Draw large price on top of chart
      drawPrice(String(currentPrice, 2), success);
      drawBatteryWarning();  // Refresh battery warning
    }

    // Always update timestamp even if failed
    lastPriceUpdate = now;
    PRICE_UPDATE_INTERVAL = PRICE_UPDATE_INTERVAL_BASE + random(0, 10000);
  }

  // --- Chart update every 15 minutes ---
  if (now - lastChartUpdate >= CHART_UPDATE_INTERVAL) {
    Serial.println("\n[UPDATE] Fetching chart...");
    bool success = fetchWeekPrices(weekPrices);
    if (success) {
      tft.fillRect(CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT, COLOR_BG);
      drawGrid(CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT);
      drawSeries(weekPrices, CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT);
      drawChartLabels(weekPrices, CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT);
      drawPrice(String(currentPrice, 2), currentPrice > 0);
      drawBatteryWarning();  // Refresh battery warning
    }
    lastChartUpdate = now;
    CHART_UPDATE_INTERVAL = CHART_UPDATE_INTERVAL_BASE + random(0, 30000);
  }

  // --- Firmware update check every hour ---
  if (now - lastFirmwareCheck >= FIRMWARE_UPDATE_INTERVAL) {
    Serial.println("\n[UPDATE] Checking for firmware updates...");
    checkForFirmwareUpdate();
    lastFirmwareCheck = now;
  }

  delay(100);
}

void setBacklight(bool bright) {
  int brightness = bright ? BACKLIGHT_FULL : BACKLIGHT_DIM;
  ledcWrite(BACKLIGHT_PWM_CHANNEL, brightness);
  backlightBright = bright;
}

void handleButton() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long now = millis();
    if (now - lastButtonPress > debounceDelay) {
      lastButtonPress = now;
      setBacklight(!backlightBright);
    }
  }
}

void checkBattery() {
  // Read battery voltage from ADC
  // LILYGO T-Display has voltage divider (2:1), ADC range 0-4095 for 0-3.3V
  // Actual battery voltage = (ADC_value / 4095) * 3.3V * 2
  int adcValue = analogRead(BATTERY_PIN);
  float voltage = (adcValue / 4095.0) * 3.3 * 2.0;

  batteryLow = (voltage < BATTERY_LOW_VOLTAGE && voltage > 0.5);  // Ignore very low readings (disconnected)

  if (batteryLow) {
    Serial.println("[BATTERY] Low voltage: " + String(voltage, 2) + "V");
  }
}

void drawBatteryWarning() {
  if (batteryLow) {
    tft.setTextColor(COLOR_ERROR, COLOR_HEADER);
    tft.setTextDatum(TR_DATUM);  // Top-right alignment
    tft.drawString("CHARGE", SCREEN_WIDTH - 2, 2, 1);  // Small font, top-right corner
  } else {
    // Clear the warning area if battery is OK
    tft.fillRect(SCREEN_WIDTH - 40, 0, 40, 12, COLOR_HEADER);
  }
}
