/**
 * Bitcoin Live Price Display for LILYGO T-Display ESP32
 * Features: BTC/USD price, 7-day chart, WiFi, GitHub OTA updates
 * Fixes: JSON reliability + API rate limiting protection
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
#define FIRMWARE_VERSION "1.0.2"

// ========== HARDWARE CONFIGURATION ==========
#define BUTTON_PIN    35
#define BACKLIGHT_PIN 4
#define BACKLIGHT_PWM_CHANNEL 0
#define BACKLIGHT_DIM  64
#define BACKLIGHT_FULL 255

// ========== DISPLAY CONFIGURATION ==========
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 135
#define HEADER_HEIGHT 28
#define CHART_X       0
#define CHART_Y       28
#define CHART_WIDTH   240
#define CHART_HEIGHT  100

// ========== UPDATE INTERVALS ==========
#define PRICE_UPDATE_INTERVAL_BASE    60000    // 60 seconds
#define CHART_UPDATE_INTERVAL_BASE    900000   // 15 minutes
#define FIRMWARE_UPDATE_INTERVAL      3600000  // 1 hour

// ========== COLOR SCHEME ==========
#define COLOR_BG       0x0000
#define COLOR_HEADER   0x001F
#define COLOR_TEXT     0xFFFF
#define COLOR_GRID     0x2104
#define COLOR_CHART    0x07E0
#define COLOR_ERROR    0xF800

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

// Dynamic intervals (to randomize slightly)
unsigned long PRICE_UPDATE_INTERVAL = PRICE_UPDATE_INTERVAL_BASE;
unsigned long CHART_UPDATE_INTERVAL = CHART_UPDATE_INTERVAL_BASE;

// ========== FUNCTION DECLARATIONS ==========
void connectWifi();
String stripChunkedEncoding(const String& raw);
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

// ========== CHUNKED ENCODING PARSER ==========
String stripChunkedEncoding(const String& raw) {
  String result = "";
  int pos = 0;

  while (pos < raw.length()) {
    // Find the end of the chunk size line
    int lineEnd = raw.indexOf('\n', pos);
    if (lineEnd == -1) break;

    // Extract chunk size (hex string)
    String chunkSizeLine = raw.substring(pos, lineEnd);
    chunkSizeLine.trim();

    // Parse hex chunk size
    long chunkSize = strtol(chunkSizeLine.c_str(), NULL, 16);

    // If chunk size is 0, we're done
    if (chunkSize == 0) break;

    // Move past the chunk size line
    pos = lineEnd + 1;

    // Read the chunk data
    if (pos + chunkSize <= raw.length()) {
      result += raw.substring(pos, pos + chunkSize);
      pos += chunkSize;
    } else {
      break;
    }

    // Skip the trailing \r\n after chunk data
    if (pos < raw.length() && raw.charAt(pos) == '\r') pos++;
    if (pos < raw.length() && raw.charAt(pos) == '\n') pos++;
  }

  return result;
}

// ========== COINGECKO API FETCHERS ==========
bool fetchCurrentPrice(float& out) {
  WiFiClientSecure client;
  client.setInsecure();

  const char* host = "api.coingecko.com";
  const int httpsPort = 443;
  const String url = "/api/v3/simple/price?ids=bitcoin&vs_currencies=usd";

  Serial.println("[API] Fetching current price...");

  if (!client.connect(host, httpsPort)) {
    Serial.println("[API] Connection failed!");
    return false;
  }

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP32-Bitcoin-Display\r\n" +
               "Accept: application/json\r\n" +
               "Accept-Encoding: identity\r\n" +
               "Connection: close\r\n\r\n");

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println("[API] Timeout!");
      client.stop();
      return false;
    }
  }

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) break;
  }

  // FIXED: Character-by-character reading to avoid chunked encoding issues
  String rawPayload = "";
  while (client.available()) {
    char c = client.read();
    rawPayload += c;
  }
  client.stop();

  // DEBUG: Show raw response
  Serial.println("[DEBUG] Raw payload length: " + String(rawPayload.length()));
  Serial.println("[DEBUG] Raw payload: " + rawPayload);

  // Strip chunked transfer encoding
  String payload = stripChunkedEncoding(rawPayload);
  Serial.println("[DEBUG] Clean JSON length: " + String(payload.length()));
  Serial.println("[DEBUG] Clean JSON: " + payload);

  if (payload.indexOf("rate limit") != -1 || payload.indexOf("429") != -1) {
    Serial.println("[API] ⚠️ Rate limit detected — pausing for 60s...");
    delay(60000);
    return false;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.println("[API] JSON parse failed: " + String(error.c_str()));
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

bool fetchWeekPrices(std::vector<float>& out) {
  WiFiClientSecure client;
  client.setInsecure();

  const char* host = "api.coingecko.com";
  const int httpsPort = 443;
  const String url = "/api/v3/coins/bitcoin/market_chart?vs_currency=usd&days=7";

  Serial.println("[API] Fetching 7-day chart data...");

  if (!client.connect(host, httpsPort)) {
    Serial.println("[API] Connection failed!");
    return false;
  }

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP32-Bitcoin-Display\r\n" +
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

  // FIXED: Character-by-character reading to avoid chunked encoding issues
  String rawPayload = "";
  while (client.available()) {
    char c = client.read();
    rawPayload += c;
  }
  client.stop();

  // Strip chunked transfer encoding
  String payload = stripChunkedEncoding(rawPayload);
  Serial.println("[DEBUG] Chart clean JSON length: " + String(payload.length()));
  Serial.println("[DEBUG] First 200 chars: " + payload.substring(0, min(200, (int)payload.length())));

  if (payload.indexOf("rate limit") != -1 || payload.indexOf("429") != -1) {
    Serial.println("[API] ⚠️ Rate limit detected on chart endpoint — pausing...");
    delay(60000);
    return false;
  }

  DynamicJsonDocument doc(32768);
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
  for (JsonArray pricePoint : prices) {
    if (pricePoint.size() >= 2) {
      float price = pricePoint[1].as<float>();
      out.push_back(price);
    }
  }

  Serial.println("[API] Got " + String(out.size()) + " data points");
  return out.size() > 0;
}

// ========== DISPLAY ==========
void drawHeader(const String& price, bool netOk) {
  tft.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COLOR_HEADER);
  tft.setTextColor(COLOR_TEXT, COLOR_HEADER);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("BTC LIVE", 5, HEADER_HEIGHT / 2, 2);
  tft.setTextDatum(MR_DATUM);

  if (netOk && price.length() > 0)
    tft.drawString("$" + price, SCREEN_WIDTH - 5, HEADER_HEIGHT / 2, 2);
  else {
    tft.setTextColor(COLOR_ERROR, COLOR_HEADER);
    tft.drawString("NO DATA", SCREEN_WIDTH - 5, HEADER_HEIGHT / 2, 2);
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
    tft.drawLine(x1, y1, x2, y2, COLOR_CHART);
  }
}

// ========== MAIN ==========
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== Bitcoin Live Display ===");

  randomSeed(esp_random()); // for random interval jitter
  PRICE_UPDATE_INTERVAL = PRICE_UPDATE_INTERVAL_BASE + random(0, 10000);
  CHART_UPDATE_INTERVAL = CHART_UPDATE_INTERVAL_BASE + random(0, 30000);

  ledcSetup(BACKLIGHT_PWM_CHANNEL, 5000, 8);
  ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_PWM_CHANNEL);
  setBacklight(true);
  pinMode(BUTTON_PIN, INPUT);

  tft.init(); tft.setRotation(1); tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("BTC Display", SCREEN_WIDTH/2, SCREEN_HEIGHT/2-15, 4);
  tft.drawString("Initializing...", SCREEN_WIDTH/2, SCREEN_HEIGHT/2+15, 2);
  delay(1000);

  connectWifi();

  if (wifiConnected) {
    tft.fillScreen(COLOR_BG);
    tft.drawString("Loading data...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);
    fetchCurrentPrice(currentPrice);
    fetchWeekPrices(weekPrices);
  } else {
    tft.fillScreen(COLOR_BG);
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

  // --- Price update every minute (with backoff protection) ---
  if (now - lastPriceUpdate >= PRICE_UPDATE_INTERVAL) {
    Serial.println("\n[UPDATE] Fetching price...");
    bool success = fetchCurrentPrice(currentPrice);
    if (success) drawHeader(String(currentPrice, 2), true);
    else drawHeader("", false);

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
    }
    lastChartUpdate = now;
    CHART_UPDATE_INTERVAL = CHART_UPDATE_INTERVAL_BASE + random(0, 30000);
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
