#include <Arduino.h>
#include <DHT.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

#define TFT_DC    18
#define TFT_CS    23
#define TFT_RST   19
#define TFT_CLK   17
#define TFT_MOSI  5
#define TFT_MISO  16

#define I2C_SDA   21
#define I2C_SCL   22
#define DHT_PIN   4
#define DHT_TYPE  DHT22

const char* ssid = "138SL-Residents.";
const char* password = "resident2020@138sl";

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);
DHT dht(DHT_PIN, DHT_TYPE);

struct Bmp280Calibration {
  uint16_t digT1;
  int16_t digT2;
  int16_t digT3;
  uint16_t digP1;
  int16_t digP2;
  int16_t digP3;
  int16_t digP4;
  int16_t digP5;
  int16_t digP6;
  int16_t digP7;
  int16_t digP8;
  int16_t digP9;
};

struct Bmp280State {
  bool isAvailable;
  uint8_t address;
  Bmp280Calibration calibration;
};

struct Dht22State {
  bool hasReading;
  unsigned long lastReadMs;
  float temperatureC;
  float humidityPct;
};

struct DashboardData {
  float temperatureC;
  float humidityPct;
  float pressureHpa;
  float soilMoisturePct;
  bool isOnline;
  unsigned long uptimeSeconds;
};

struct DashboardCache {
  String temperature;
  String humidity;
  String pressure;
  String soil;
  bool isOnline;
  String clock;
};

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

namespace Layout {
  constexpr int16_t kScreenW = 240;
  constexpr int16_t kScreenH = 320;
  constexpr int16_t kHeaderH = 24;
  constexpr int16_t kMargin = 6;
  constexpr int16_t kGutter = 4;

  constexpr int16_t kCardW = (kScreenW - (kMargin * 2) - kGutter) / 2;
  constexpr int16_t kCardH = (kScreenH - kHeaderH - (kMargin * 2) - kGutter) / 2;

  constexpr int16_t kRow0Y = kHeaderH + kMargin;
  constexpr int16_t kRow1Y = kRow0Y + kCardH + kGutter;

  constexpr int16_t kCol0X = kMargin;
  constexpr int16_t kCol1X = kCol0X + kCardW + kGutter;

  constexpr Rect kTempCard = {kCol0X, kRow0Y, kCardW, kCardH};
  constexpr Rect kHumidityCard = {kCol1X, kRow0Y, kCardW, kCardH};
  constexpr Rect kPressureCard = {kCol0X, kRow1Y, kCardW, kCardH};
  constexpr Rect kSoilCard = {kCol1X, kRow1Y, kCardW, kCardH};

  constexpr Rect kHeaderTime = {kScreenW - 60, 0, 54, kHeaderH};
}

namespace Colors {
  uint16_t background;
  uint16_t surface;
  uint16_t line;
  uint16_t label;
  uint16_t value;
  uint16_t accent;
  uint16_t good;
  uint16_t error;
}

static unsigned long lastRefreshMs = 0;
static const unsigned long kRefreshIntervalMs = 1000;
static const unsigned long kWiFiRetryIntervalMs = 10000;
static DashboardCache previousFrame;
static bool hasPreviousFrame = false;
static Bmp280State bmp280 = {};
static Dht22State dht22 = {};
static unsigned long lastWiFiRetryMs = 0;

void drawDashboardShell();
void renderDashboardValues(const DashboardData& data);
DashboardData getDashboardData();
String formatClock(unsigned long seconds);
void logDashboardData(const DashboardData& data);
void initializeWiFi();
void ensureWiFiConnected();
bool readDht22(float& temperatureC, float& humidityPct);
bool initializeBmp280();
bool readBmp280(float& pressureHpa);
bool readBmp280Calibration(Bmp280Calibration& calibration);
bool readBmp280Raw(int32_t& rawTemperature, int32_t& rawPressure);
bool readI2cRegister(uint8_t address, uint8_t reg, uint8_t& value);
bool readI2cRegisters(uint8_t address, uint8_t reg, uint8_t* buffer, size_t length);
bool writeI2cRegister(uint8_t address, uint8_t reg, uint8_t value);
String formatReading(float value, uint8_t decimals);
void drawCardLabel(const Rect& rect, const char* label);
void drawValueCard(const Rect& rect, const String& valueText, const char* unitText);
Rect makeValueArea(const Rect& card);
const GFXfont* selectValueFont(const Rect& rect, const String& valueText);
void drawFixedTextDiff(const Rect& area, const String& previous, const String& next, uint16_t color, uint16_t bg);

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  dht.begin();
  initializeWiFi();

  tft.begin();
  tft.setRotation(0);
  tft.setTextWrap(false);

  // Dark Theme Palette
  Colors::background = tft.color565(18, 18, 18);   // #121212 Charcoal
  Colors::surface = tft.color565(37, 37, 37);      // #252525 Dark Slate
  Colors::line = tft.color565(60, 60, 60);         // #3C3C3C Muted Line
  Colors::label = tft.color565(160, 160, 160);     // #A0A0A0 Dim Silver
  Colors::value = tft.color565(255, 255, 255);     // #FFFFFF Pure White
  Colors::accent = tft.color565(100, 150, 255);    // #6496FF Soft Blue
  Colors::good = tft.color565(0, 230, 118);        // #00E676 Mint Green
  Colors::error = tft.color565(255, 82, 82);       // #FF5252 Soft Red

  initializeBmp280();
  drawDashboardShell();
  const DashboardData initialData = getDashboardData();
  renderDashboardValues(initialData);
  logDashboardData(initialData);
  lastRefreshMs = millis();
}

void loop() {
  const unsigned long now = millis();
  ensureWiFiConnected();
  if (now - lastRefreshMs >= kRefreshIntervalMs) {
    const DashboardData data = getDashboardData();
    renderDashboardValues(data);
    logDashboardData(data);
    lastRefreshMs = now;
  }
}

void initializeWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  Serial.printf("Connecting to %s \n", ssid);
  WiFi.begin(ssid, password);

  const unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < 15000UL) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n\n***** Wi-Fi CONNECTED! *****");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
  } else {
    Serial.println("\nWi-Fi connection timed out. Running offline.");
    lastWiFiRetryMs = millis();
  }
}

void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if ((millis() - lastWiFiRetryMs) < kWiFiRetryIntervalMs) {
    return;
  }

  Serial.printf("Reconnecting to %s\n", ssid);
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  lastWiFiRetryMs = millis();
}

bool initializeBmp280() {
  constexpr uint8_t kBmp280ChipId = 0x58;
  constexpr uint8_t kBmp280Address = 0x76;

  bmp280 = {};

  uint8_t chipId = 0;
  if (!readI2cRegister(kBmp280Address, 0xD0, chipId)) {
    Serial.printf("BMP280 not responding at 0x%02X on SDA=%d SCL=%d\n", kBmp280Address, I2C_SDA, I2C_SCL);
    return false;
  }
  if (chipId != kBmp280ChipId) {
    Serial.printf("I2C device at 0x%02X returned chip ID 0x%02X, expected 0x%02X for BMP280\n",
                  kBmp280Address, chipId, kBmp280ChipId);
    return false;
  }

  bmp280.address = kBmp280Address;
  if (!readBmp280Calibration(bmp280.calibration)) {
    Serial.printf("BMP280 detected at 0x%02X but calibration read failed\n", kBmp280Address);
    bmp280 = {};
    return false;
  }

  // Configure normal mode with x1 temperature and x4 pressure oversampling.
  if (!writeI2cRegister(kBmp280Address, 0xF5, 0x10) || !writeI2cRegister(kBmp280Address, 0xF4, 0x2F)) {
    Serial.printf("BMP280 detected at 0x%02X but configuration write failed\n", kBmp280Address);
    bmp280 = {};
    return false;
  }

  bmp280.isAvailable = true;
  Serial.printf("BMP280 initialized on SDA=%d SCL=%d at address 0x%02X\n", I2C_SDA, I2C_SCL, kBmp280Address);
  return true;
}

bool readDht22(float& temperatureC, float& humidityPct) {
  constexpr unsigned long kMinReadIntervalMs = 2000;
  const unsigned long now = millis();

  if (dht22.hasReading && (now - dht22.lastReadMs) < kMinReadIntervalMs) {
    temperatureC = dht22.temperatureC;
    humidityPct = dht22.humidityPct;
    return true;
  }

  const float measuredHumidityPct = dht.readHumidity();
  const float measuredTemperatureC = dht.readTemperature();
  if (isnan(measuredHumidityPct) || isnan(measuredTemperatureC)) {
    return false;
  }

  dht22.hasReading = true;
  dht22.lastReadMs = now;
  dht22.temperatureC = measuredTemperatureC;
  dht22.humidityPct = measuredHumidityPct;
  temperatureC = measuredTemperatureC;
  humidityPct = measuredHumidityPct;
  return true;
}

bool readBmp280(float& pressureHpa) {
  if (!bmp280.isAvailable) {
    return false;
  }

  int32_t rawTemperature = 0;
  int32_t rawPressure = 0;
  if (!readBmp280Raw(rawTemperature, rawPressure)) {
    return false;
  }

  const Bmp280Calibration& c = bmp280.calibration;

  int32_t var1 = ((((rawTemperature >> 3) - (static_cast<int32_t>(c.digT1) << 1))) *
                  static_cast<int32_t>(c.digT2)) >> 11;
  int32_t var2 = (((((rawTemperature >> 4) - static_cast<int32_t>(c.digT1)) *
                    ((rawTemperature >> 4) - static_cast<int32_t>(c.digT1))) >> 12) *
                  static_cast<int32_t>(c.digT3)) >> 14;
  const int32_t tFine = var1 + var2;

  int64_t pressureVar1 = static_cast<int64_t>(tFine) - 128000;
  int64_t pressureVar2 = pressureVar1 * pressureVar1 * static_cast<int64_t>(c.digP6);
  pressureVar2 += (pressureVar1 * static_cast<int64_t>(c.digP5)) << 17;
  pressureVar2 += static_cast<int64_t>(c.digP4) << 35;
  pressureVar1 = ((pressureVar1 * pressureVar1 * static_cast<int64_t>(c.digP3)) >> 8) +
                 ((pressureVar1 * static_cast<int64_t>(c.digP2)) << 12);
  pressureVar1 = (((static_cast<int64_t>(1) << 47) + pressureVar1) *
                  static_cast<int64_t>(c.digP1)) >> 33;

  if (pressureVar1 == 0) {
    return false;
  }

  int64_t pressure = 1048576 - rawPressure;
  pressure = (((pressure << 31) - pressureVar2) * 3125) / pressureVar1;
  pressureVar1 = (static_cast<int64_t>(c.digP9) * (pressure >> 13) * (pressure >> 13)) >> 25;
  pressureVar2 = (static_cast<int64_t>(c.digP8) * pressure) >> 19;
  pressure = ((pressure + pressureVar1 + pressureVar2) >> 8) + (static_cast<int64_t>(c.digP7) << 4);

  pressureHpa = (pressure / 256.0f) / 100.0f;
  return true;
}

bool readBmp280Calibration(Bmp280Calibration& calibration) {
  uint8_t buffer[24];
  if (!readI2cRegisters(bmp280.address, 0x88, buffer, sizeof(buffer))) {
    return false;
  }

  calibration.digT1 = static_cast<uint16_t>(buffer[1] << 8 | buffer[0]);
  calibration.digT2 = static_cast<int16_t>(buffer[3] << 8 | buffer[2]);
  calibration.digT3 = static_cast<int16_t>(buffer[5] << 8 | buffer[4]);
  calibration.digP1 = static_cast<uint16_t>(buffer[7] << 8 | buffer[6]);
  calibration.digP2 = static_cast<int16_t>(buffer[9] << 8 | buffer[8]);
  calibration.digP3 = static_cast<int16_t>(buffer[11] << 8 | buffer[10]);
  calibration.digP4 = static_cast<int16_t>(buffer[13] << 8 | buffer[12]);
  calibration.digP5 = static_cast<int16_t>(buffer[15] << 8 | buffer[14]);
  calibration.digP6 = static_cast<int16_t>(buffer[17] << 8 | buffer[16]);
  calibration.digP7 = static_cast<int16_t>(buffer[19] << 8 | buffer[18]);
  calibration.digP8 = static_cast<int16_t>(buffer[21] << 8 | buffer[20]);
  calibration.digP9 = static_cast<int16_t>(buffer[23] << 8 | buffer[22]);
  return true;
}

bool readBmp280Raw(int32_t& rawTemperature, int32_t& rawPressure) {
  uint8_t buffer[6];
  if (!readI2cRegisters(bmp280.address, 0xF7, buffer, sizeof(buffer))) {
    return false;
  }

  rawPressure = (static_cast<int32_t>(buffer[0]) << 12) |
                (static_cast<int32_t>(buffer[1]) << 4) |
                (static_cast<int32_t>(buffer[2]) >> 4);
  rawTemperature = (static_cast<int32_t>(buffer[3]) << 12) |
                   (static_cast<int32_t>(buffer[4]) << 4) |
                   (static_cast<int32_t>(buffer[5]) >> 4);
  return true;
}

bool readI2cRegister(uint8_t address, uint8_t reg, uint8_t& value) {
  return readI2cRegisters(address, reg, &value, 1);
}

bool readI2cRegisters(uint8_t address, uint8_t reg, uint8_t* buffer, size_t length) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const size_t bytesRead = Wire.requestFrom(static_cast<int>(address), static_cast<int>(length));
  if (bytesRead != length) {
    return false;
  }

  for (size_t i = 0; i < length; ++i) {
    buffer[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

bool writeI2cRegister(uint8_t address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

void drawDashboardShell() {
  tft.fillScreen(Colors::background);

  // Draw Cards with rounded corners
  const uint8_t r = 6;
  tft.fillRoundRect(Layout::kTempCard.x, Layout::kTempCard.y, Layout::kTempCard.w, Layout::kTempCard.h, r, Colors::surface);
  tft.fillRoundRect(Layout::kHumidityCard.x, Layout::kHumidityCard.y, Layout::kHumidityCard.w, Layout::kHumidityCard.h, r, Colors::surface);
  tft.fillRoundRect(Layout::kPressureCard.x, Layout::kPressureCard.y, Layout::kPressureCard.w, Layout::kPressureCard.h, r, Colors::surface);
  tft.fillRoundRect(Layout::kSoilCard.x, Layout::kSoilCard.y, Layout::kSoilCard.w, Layout::kSoilCard.h, r, Colors::surface);

  drawCardLabel(Layout::kTempCard, "TEMP");
  drawCardLabel(Layout::kHumidityCard, "HUMIDITY");
  drawCardLabel(Layout::kPressureCard, "PRESSURE");
  drawCardLabel(Layout::kSoilCard, "SOIL");

  // Header Network Label
  tft.setFont(nullptr);
  tft.setTextColor(Colors::label);
  tft.setTextSize(1);
  tft.setCursor(Layout::kMargin + 12, 8);
  tft.print("WIFI");
}

void renderDashboardValues(const DashboardData& data) {
  DashboardCache nextFrame;
  nextFrame.temperature = formatReading(data.temperatureC, 1);
  nextFrame.humidity = formatReading(data.humidityPct, 1);
  nextFrame.pressure = formatReading(data.pressureHpa, 1);
  nextFrame.soil = formatReading(data.soilMoisturePct, 1);
  nextFrame.isOnline = data.isOnline;
  nextFrame.clock = formatClock(data.uptimeSeconds);

  // Network Status Dot
  if (!hasPreviousFrame || nextFrame.isOnline != previousFrame.isOnline) {
    tft.fillCircle(Layout::kMargin + 4, 11, 3, nextFrame.isOnline ? Colors::good : Colors::error);
  }

  // Header Clock
  if (!hasPreviousFrame || nextFrame.clock != previousFrame.clock) {
    drawFixedTextDiff(Layout::kHeaderTime, hasPreviousFrame ? previousFrame.clock : "", nextFrame.clock, Colors::value, Colors::background);
  }

  // Sensor Cards
  if (!hasPreviousFrame || nextFrame.temperature != previousFrame.temperature) {
    drawValueCard(Layout::kTempCard, nextFrame.temperature, "C");
  }
  if (!hasPreviousFrame || nextFrame.humidity != previousFrame.humidity) {
    drawValueCard(Layout::kHumidityCard, nextFrame.humidity, "%");
  }
  if (!hasPreviousFrame || nextFrame.pressure != previousFrame.pressure) {
    drawValueCard(Layout::kPressureCard, nextFrame.pressure, "hPa");
  }
  if (!hasPreviousFrame || nextFrame.soil != previousFrame.soil) {
    drawValueCard(Layout::kSoilCard, nextFrame.soil, "%");
  }

  previousFrame = nextFrame;
  hasPreviousFrame = true;
}

DashboardData getDashboardData() {
  const unsigned long uptimeSeconds = millis() / 1000UL;
  const float slowWave = cosf(static_cast<float>(uptimeSeconds) * 0.04f);

  DashboardData data;
  data.temperatureC = NAN;
  data.humidityPct = NAN;
  data.pressureHpa = NAN;
  data.soilMoisturePct = 62.0f + (slowWave * 0.7f);
  data.isOnline = (WiFi.status() == WL_CONNECTED);
  data.uptimeSeconds = uptimeSeconds;

  float measuredDhtTemperatureC = 0.0f;
  float measuredHumidityPct = 0.0f;
  if (readDht22(measuredDhtTemperatureC, measuredHumidityPct)) {
    data.temperatureC = measuredDhtTemperatureC;
    data.humidityPct = measuredHumidityPct;
  }

  float measuredPressureHpa = 0.0f;
  if (readBmp280(measuredPressureHpa)) {
    data.pressureHpa = measuredPressureHpa;
  }

  return data;
}

String formatClock(unsigned long seconds) {
  const unsigned long hours = (seconds / 3600UL) % 24UL;
  const unsigned long minutes = (seconds / 60UL) % 60UL;
  const unsigned long secs = seconds % 60UL;
  char buffer[9];
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu", hours, minutes, secs);
  return String(buffer);
}

void logDashboardData(const DashboardData& data) {
  Serial.print("Temp: ");
  if (isnan(data.temperatureC)) {
    Serial.print("--");
  } else {
    Serial.print(data.temperatureC, 1);
  }

  Serial.print(" C  Humidity: ");
  if (isnan(data.humidityPct)) {
    Serial.print("--");
  } else {
    Serial.print(data.humidityPct, 1);
  }

  Serial.print(" %  Pressure: ");
  if (isnan(data.pressureHpa)) {
    Serial.print("--");
  } else {
    Serial.print(data.pressureHpa, 1);
  }

  Serial.print(" hPa  Soil: ");
  if (isnan(data.soilMoisturePct)) {
    Serial.print("--");
  } else {
    Serial.print(data.soilMoisturePct, 1);
  }

  Serial.println(" %");
}

String formatReading(float value, uint8_t decimals) {
  if (isnan(value)) {
    return "--";
  }
  return String(static_cast<double>(value), static_cast<unsigned int>(decimals));
}

void drawCardLabel(const Rect& rect, const char* label) {
  tft.setFont(nullptr);
  tft.setTextColor(Colors::label);
  tft.setTextSize(1);
  tft.setCursor(rect.x + 8, rect.y + 8);
  tft.print(label);
}

void drawValueCard(const Rect& rect, const String& valueText, const char* unitText) {
  const Rect valueArea = makeValueArea(rect);

  // Create an off-screen 16-bit canvas sized to the inner value area
  // This requires about (W * H * 2) bytes of RAM (e.g. 106 * 98 * 2 = ~20KB)
  GFXcanvas16 canvas(valueArea.w, valueArea.h);

  // Fill canvas with the surface color (erasing previous content in memory)
  canvas.fillScreen(Colors::surface);

  // Measure and draw main value text
  const GFXfont* valueFont = selectValueFont(rect, valueText);
  canvas.setFont(valueFont);
  canvas.setTextColor(Colors::value);
  canvas.setTextSize(1);

  int16_t x1;
  int16_t y1;
  uint16_t valueW;
  uint16_t valueH;
  canvas.getTextBounds(valueText, 0, 0, &x1, &y1, &valueW, &valueH);

  // Coordinates are now relative to the canvas (0,0 is top-left of canvas)
  const int16_t baseX = ((valueArea.w - static_cast<int16_t>(valueW)) / 2) - x1;
  const int16_t baselineY = ((valueArea.h - static_cast<int16_t>(valueH)) / 2) - y1 - 4; // Shifted up slightly to make room for unit
  canvas.setCursor(baseX, baselineY);
  canvas.print(valueText);

  // Draw the unit text at the bottom right of the canvas
  canvas.setFont(nullptr);
  canvas.setTextColor(Colors::label);
  canvas.setTextSize(1);
  const int16_t unitLen = static_cast<int16_t>(strlen(unitText));
  canvas.setCursor(valueArea.w - (unitLen * 6) - 4, valueArea.h - 10);
  canvas.print(unitText);

  // Push the completed frame buffer to the physical screen in one swift transaction
  tft.drawRGBBitmap(valueArea.x, valueArea.y, canvas.getBuffer(), canvas.width(), canvas.height());
}

Rect makeValueArea(const Rect& card) {
  return {
    static_cast<int16_t>(card.x + 4),
    static_cast<int16_t>(card.y + 20),
    static_cast<int16_t>(card.w - 8),
    static_cast<int16_t>(card.h - 24) // Extended down to cover the old unitArea space
  };
}

const GFXfont* selectValueFont(const Rect& rect, const String& valueText) {
  // Avoid dynamic pixel width checking. Proportional fonts (like '1' vs '4')
  // cause the physical width to fluctuate, making the font randomly shrink.
  // Using string length provides a perfectly stable font size.
  const size_t len = valueText.length();

  if (len <= 5) {
    return &FreeSansBold18pt7b;
  }
  if (len <= 7) {
    return &FreeSansBold12pt7b;
  }
  return &FreeSansBold9pt7b;
}

void drawFixedTextDiff(const Rect& area, const String& previous, const String& next, uint16_t color, uint16_t bg) {
  const uint8_t charW = 6;
  const uint8_t charH = 8;
  const size_t maxChars = area.w / charW;
  const size_t previousLen = previous.length();
  const size_t nextLen = next.length();
  const size_t loopLen = max(previousLen, nextLen);

  tft.setFont(nullptr);
  tft.setTextSize(1);

  for (size_t i = 0; i < loopLen && i < maxChars; ++i) {
    const char prevCh = (i < previousLen) ? previous[i] : ' ';
    const char nextCh = (i < nextLen) ? next[i] : ' ';
    if (prevCh == nextCh) {
      continue;
    }

    const int16_t cellX = area.x + static_cast<int16_t>(i * charW);
    const int16_t cellY = area.y + ((area.h - charH) / 2);
    tft.drawChar(cellX, cellY, nextCh, color, bg, 1);
  }
}
