/**
 * Bitcoin Live Price Display for LILYGO T-Display ESP32
 * Features: BTC/USD price, 7-day chart, WiFi, GitHub OTA updates
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
#define FIRMWARE_VERSION "1.0.0"

// ========== HARDWARE CONFIGURATION ==========
#define BUTTON_PIN    35
#define BACKLIGHT_PIN 4
#define BACKLIGHT_PWM_CHANNEL 0
#define BACKLIGHT_DIM  64   // ~25% brightness (0-255)
#define BACKLIGHT_FULL 255  // 100% brightness

// ========== DISPLAY CONFIGURATION ==========
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 135
#define HEADER_HEIGHT 28
#define CHART_X       0
#define CHART_Y       28
#define CHART_WIDTH   240
#define CHART_HEIGHT  100

// ========== UPDATE INTERVALS ==========
#define PRICE_UPDATE_INTERVAL    60000    // 60 seconds
#define CHART_UPDATE_INTERVAL    900000   // 15 minutes
#define FIRMWARE_UPDATE_INTERVAL 3600000  // 60 minutes (1 hour)

// ========== COLOR SCHEME ==========
#define COLOR_BG       0x0000  // Black
#define COLOR_HEADER   0x001F  // Blue
#define COLOR_TEXT     0xFFFF  // White
#define COLOR_GRID     0x2104  // Dark gray
#define COLOR_CHART    0x07E0  // Green
#define COLOR_ERROR    0xF800  // Red

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
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 200;

// ========== FUNCTION DECLARATIONS ==========
void connectWifi();
bool fetchCurrentPrice(float& out);
bool fetchWeekPrices(std::vector<float>& out);
void drawHeader(const String& price, bool netOk = true);
void drawGrid(int x, int y, int w, int h);
void drawSeries(const std::vector<float>& s, int x, int y, int w, int h);
bool checkForFirmwareUpdate();
void performFirmwareUpdate(const String& firmwareUrl);
void handleButton();
void setBacklight(bool bright);

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

// ========== COINGECKO API FETCHERS ==========

/**
 * Fetch current BTC/USD price from CoinGecko
 */
bool fetchCurrentPrice(float& out) {
  WiFiClientSecure client;
  client.setInsecure(); // Skip certificate validation for simplicity

  const char* host = "api.coingecko.com";
  const int httpsPort = 443;
  const String url = "/api/v3/simple/price?ids=bitcoin&vs_currencies=usd";

  Serial.println("[API] Fetching current price...");

  if (!client.connect(host, httpsPort)) {
    Serial.println("[API] Connection failed!");
    return false;
  }

  // Send HTTP GET request
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP32-Bitcoin-Display\r\n" +
               "Connection: close\r\n\r\n");

  // Wait for response
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println("[API] Timeout!");
      client.stop();
      return false;
    }
  }

  // Skip HTTP headers
  bool headersEnded = false;
  while (client.available()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      headersEnded = true;
      break;
    }
  }

  if (!headersEnded) {
    Serial.println("[API] Invalid response!");
    client.stop();
    return false;
  }

  // Read JSON body
  String payload = client.readString();
  client.stop();

  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.println("[API] JSON parsing failed: " + String(error.c_str()));
    return false;
  }

  if (doc.containsKey("bitcoin") && doc["bitcoin"].containsKey("usd")) {
    out = doc["bitcoin"]["usd"].as<float>();
    Serial.println("[API] Price: $" + String(out, 2));
    return true;
  }

  Serial.println("[API] Invalid JSON structure!");
  return false;
}

/**
 * Fetch 7-day hourly price data from CoinGecko
 */
bool fetchWeekPrices(std::vector<float>& out) {
  WiFiClientSecure client;
  client.setInsecure();

  const char* host = "api.coingecko.com";
  const int httpsPort = 443;
  const String url = "/api/v3/coins/bitcoin/market_chart?vs_currency=usd&days=7&interval=hourly";

  Serial.println("[API] Fetching 7-day chart data...");

  if (!client.connect(host, httpsPort)) {
    Serial.println("[API] Connection failed!");
    return false;
  }

  // Send HTTP GET request
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP32-Bitcoin-Display\r\n" +
               "Connection: close\r\n\r\n");

  // Wait for response
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 10000) {
      Serial.println("[API] Timeout!");
      client.stop();
      return false;
    }
  }

  // Skip HTTP headers
  bool headersEnded = false;
  while (client.available()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      headersEnded = true;
      break;
    }
  }

  if (!headersEnded) {
    Serial.println("[API] Invalid response!");
    client.stop();
    return false;
  }

  // Read JSON body
  String payload = client.readString();
  client.stop();

  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.println("[API] JSON parsing failed: " + String(error.c_str()));
    return false;
  }

  if (!doc.containsKey("prices")) {
    Serial.println("[API] No 'prices' array in response!");
    return false;
  }

  JsonArray prices = doc["prices"].as<JsonArray>();
  out.clear();

  for (JsonArray pricePoint : prices) {
    if (pricePoint.size() >= 2) {
      float price = pricePoint[1].as<float>();
      out.push_back(price);
    }
  }

  Serial.println("[API] Got " + String(out.size()) + " data points");
  return out.size() > 0;
}

// ========== DISPLAY FUNCTIONS ==========

/**
 * Draw header bar with "BTC LIVE" and current price
 */
void drawHeader(const String& price, bool netOk) {
  tft.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COLOR_HEADER);

  tft.setTextColor(COLOR_TEXT, COLOR_HEADER);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("BTC LIVE", 5, HEADER_HEIGHT / 2, 2);

  tft.setTextDatum(MR_DATUM);
  if (netOk && price.length() > 0) {
    tft.drawString("$" + price, SCREEN_WIDTH - 5, HEADER_HEIGHT / 2, 2);
  } else {
    tft.setTextColor(COLOR_ERROR, COLOR_HEADER);
    tft.drawString("NO DATA", SCREEN_WIDTH - 5, HEADER_HEIGHT / 2, 2);
  }
}

/**
 * Draw grid lines in chart area
 */
void drawGrid(int x, int y, int w, int h) {
  // Draw 4 horizontal lines
  for (int i = 1; i < 4; i++) {
    int yPos = y + (h * i / 4);
    tft.drawLine(x, yPos, x + w - 1, yPos, COLOR_GRID);
  }

  // Draw 6 vertical lines (for 7 days)
  for (int i = 1; i < 7; i++) {
    int xPos = x + (w * i / 7);
    tft.drawLine(xPos, y, xPos, y + h - 1, COLOR_GRID);
  }
}

/**
 * Draw price series as a line chart
 */
void drawSeries(const std::vector<float>& s, int x, int y, int w, int h) {
  if (s.size() < 2) {
    return;
  }

  // Find min/max for scaling
  float minPrice = s[0];
  float maxPrice = s[0];
  for (float price : s) {
    if (price < minPrice) minPrice = price;
    if (price > maxPrice) maxPrice = price;
  }

  // Add 5% padding to min/max
  float range = maxPrice - minPrice;
  if (range < 1.0) range = 1.0; // Avoid division by zero
  minPrice -= range * 0.05;
  maxPrice += range * 0.05;
  range = maxPrice - minPrice;

  // Draw line segments
  for (size_t i = 1; i < s.size(); i++) {
    int x1 = x + (w * (i - 1)) / (s.size() - 1);
    int y1 = y + h - (int)((s[i - 1] - minPrice) / range * h);

    int x2 = x + (w * i) / (s.size() - 1);
    int y2 = y + h - (int)((s[i] - minPrice) / range * h);

    tft.drawLine(x1, y1, x2, y2, COLOR_CHART);
  }

  // Draw min/max labels
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("$" + String((int)maxPrice), x + 2, y + 2, 1);

  tft.setTextDatum(BL_DATUM);
  tft.drawString("$" + String((int)minPrice), x + 2, y + h - 2, 1);
}

// ========== GITHUB OTA FUNCTIONS ==========

/**
 * Compare two version strings (semantic versioning)
 * Returns: -1 if v1 < v2, 0 if equal, 1 if v1 > v2
 */
int compareVersions(String v1, String v2) {
  // Remove 'v' prefix if present
  v1.replace("v", "");
  v2.replace("v", "");

  int major1 = 0, minor1 = 0, patch1 = 0;
  int major2 = 0, minor2 = 0, patch2 = 0;

  sscanf(v1.c_str(), "%d.%d.%d", &major1, &minor1, &patch1);
  sscanf(v2.c_str(), "%d.%d.%d", &major2, &minor2, &patch2);

  if (major1 != major2) return (major1 > major2) ? 1 : -1;
  if (minor1 != minor2) return (minor1 > minor2) ? 1 : -1;
  if (patch1 != patch2) return (patch1 > patch2) ? 1 : -1;
  return 0;
}

/**
 * Check GitHub for firmware updates
 * Returns true if update is available and starts the update process
 */
bool checkForFirmwareUpdate() {
  WiFiClientSecure client;
  client.setInsecure();

  const char* host = "api.github.com";
  const int httpsPort = 443;

  // Construct API URL for latest release
  String url = "/repos/" + String(GITHUB_REPO) + "/releases/latest";

  Serial.println("[OTA] Checking for firmware updates...");
  Serial.println("[OTA] Current version: " + String(FIRMWARE_VERSION));

  if (!client.connect(host, httpsPort)) {
    Serial.println("[OTA] Failed to connect to GitHub!");
    return false;
  }

  // Send HTTP GET request
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP32-Bitcoin-Display\r\n" +
               "Accept: application/vnd.github.v3+json\r\n" +
               "Connection: close\r\n\r\n");

  // Wait for response
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 10000) {
      Serial.println("[OTA] Timeout waiting for response!");
      client.stop();
      return false;
    }
  }

  // Skip HTTP headers
  bool headersEnded = false;
  while (client.available()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      headersEnded = true;
      break;
    }
  }

  if (!headersEnded) {
    Serial.println("[OTA] Invalid response!");
    client.stop();
    return false;
  }

  // Read JSON body
  String payload = client.readString();
  client.stop();

  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.println("[OTA] JSON parsing failed: " + String(error.c_str()));
    return false;
  }

  // Extract version and firmware URL
  if (!doc.containsKey("tag_name")) {
    Serial.println("[OTA] No tag_name in response!");
    return false;
  }

  String latestVersion = doc["tag_name"].as<String>();
  Serial.println("[OTA] Latest version: " + latestVersion);

  // Compare versions
  int versionComparison = compareVersions(String(FIRMWARE_VERSION), latestVersion);

  if (versionComparison >= 0) {
    Serial.println("[OTA] Firmware is up to date!");
    return false;
  }

  Serial.println("[OTA] New version available!");

  // Find firmware binary in assets
  if (!doc.containsKey("assets")) {
    Serial.println("[OTA] No assets found in release!");
    return false;
  }

  JsonArray assets = doc["assets"].as<JsonArray>();
  String firmwareUrl = "";

  for (JsonObject asset : assets) {
    String name = asset["name"].as<String>();
    if (name.endsWith(".bin")) {
      firmwareUrl = asset["browser_download_url"].as<String>();
      Serial.println("[OTA] Found firmware: " + name);
      break;
    }
  }

  if (firmwareUrl.length() == 0) {
    Serial.println("[OTA] No .bin file found in release assets!");
    return false;
  }

  // Perform update
  performFirmwareUpdate(firmwareUrl);
  return true;
}

/**
 * Download and install firmware from GitHub
 */
void performFirmwareUpdate(const String& firmwareUrl) {
  Serial.println("[OTA] Starting firmware download...");
  Serial.println("[OTA] URL: " + firmwareUrl);

  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("FIRMWARE UPDATE", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 20, 2);
  tft.drawString("Downloading...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);

  WiFiClientSecure client;
  client.setInsecure();

  // Set up progress callback
  httpUpdate.onProgress([](int current, int total) {
    if (total > 0) {
      int percent = (current * 100) / total;
      Serial.printf("[OTA] Progress: %d%%\r\n", percent);

      // Draw progress bar
      int barWidth = 200;
      int barHeight = 10;
      int barX = (SCREEN_WIDTH - barWidth) / 2;
      int barY = SCREEN_HEIGHT / 2 + 20;

      tft.fillRect(barX, barY, barWidth, barHeight, COLOR_GRID);
      tft.fillRect(barX, barY, (barWidth * percent) / 100, barHeight, COLOR_CHART);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawString(String(percent) + "%", SCREEN_WIDTH / 2, barY + 20, 2);
    }
  });

  // Perform the update
  t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.println("[OTA] Update failed!");
      Serial.println("[OTA] Error: " + httpUpdate.getLastErrorString());

      tft.fillScreen(COLOR_BG);
      tft.setTextColor(COLOR_ERROR, COLOR_BG);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("UPDATE FAILED!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);
      delay(3000);
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("[OTA] No updates needed");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("[OTA] Update successful!");

      tft.fillScreen(COLOR_BG);
      tft.setTextColor(COLOR_CHART, COLOR_BG);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("UPDATE COMPLETE!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);
      tft.drawString("Rebooting...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 20, 2);
      delay(2000);
      ESP.restart();
      break;
  }
}

// ========== BACKLIGHT CONTROL ==========
void setBacklight(bool bright) {
  int brightness = bright ? BACKLIGHT_FULL : BACKLIGHT_DIM;
  ledcWrite(BACKLIGHT_PWM_CHANNEL, brightness);
  backlightBright = bright;
  Serial.println("[BL] Backlight set to " + String(brightness));
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

// ========== MAIN SETUP ==========
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== Bitcoin Live Display ===");

  // Initialize backlight with PWM
  ledcSetup(BACKLIGHT_PWM_CHANNEL, 5000, 8); // 5kHz, 8-bit resolution
  ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_PWM_CHANNEL);
  setBacklight(true); // Start at full brightness

  // Initialize button
  pinMode(BUTTON_PIN, INPUT);

  // Initialize TFT
  tft.init();
  tft.setRotation(1); // Landscape mode (240x135)
  tft.fillScreen(COLOR_BG);

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("BTC Display", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 15, 4);
  tft.drawString("Initializing...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 15, 2);

  delay(1000);

  // Connect to WiFi
  connectWifi();

  if (wifiConnected) {
    // Initial data fetch
    Serial.println("\n[INIT] Fetching initial data...");

    tft.fillScreen(COLOR_BG);
    tft.drawString("Loading data...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);

    bool priceOk = fetchCurrentPrice(currentPrice);
    bool chartOk = fetchWeekPrices(weekPrices);

    if (!priceOk || !chartOk) {
      Serial.println("[INIT] Failed to fetch initial data!");
    }

    lastPriceUpdate = millis();
    lastChartUpdate = millis();

    // Draw initial display
    tft.fillScreen(COLOR_BG);
    drawHeader(String(currentPrice, 2), priceOk);

    if (chartOk && weekPrices.size() > 0) {
      tft.fillRect(CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT, COLOR_BG);
      drawGrid(CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT);
      drawSeries(weekPrices, CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT);
    }
  } else {
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_ERROR, COLOR_BG);
    tft.drawString("WiFi Error!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 4);
  }

  Serial.println("\n[INIT] Setup complete!");
}

// ========== MAIN LOOP ==========
void loop() {
  // Handle button press
  handleButton();

  // Check if WiFi is still connected
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      Serial.println("[WiFi] Connection lost! Reconnecting...");
      wifiConnected = false;
      connectWifi();
    }
    delay(1000);
    return;
  }

  wifiConnected = true;
  unsigned long now = millis();

  // Update current price every 60 seconds
  if (now - lastPriceUpdate >= PRICE_UPDATE_INTERVAL) {
    Serial.println("\n[UPDATE] Fetching price...");

    if (fetchCurrentPrice(currentPrice)) {
      drawHeader(String(currentPrice, 2), true);
      lastPriceUpdate = now;
    } else {
      Serial.println("[UPDATE] Price fetch failed!");
      drawHeader("", false);
    }
  }

  // Update chart every 15 minutes
  if (now - lastChartUpdate >= CHART_UPDATE_INTERVAL) {
    Serial.println("\n[UPDATE] Fetching chart data...");

    if (fetchWeekPrices(weekPrices)) {
      // Redraw chart area only (avoid flicker)
      tft.fillRect(CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT, COLOR_BG);
      drawGrid(CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT);
      drawSeries(weekPrices, CHART_X, CHART_Y, CHART_WIDTH, CHART_HEIGHT);
      lastChartUpdate = now;
    } else {
      Serial.println("[UPDATE] Chart fetch failed!");
    }
  }

  // Check for firmware updates periodically
  if (now - lastFirmwareCheck >= FIRMWARE_UPDATE_INTERVAL) {
    Serial.println("\n[UPDATE] Checking for firmware updates...");
    checkForFirmwareUpdate();
    lastFirmwareCheck = now;
  }

  delay(100); // Small delay to prevent tight loop
}
