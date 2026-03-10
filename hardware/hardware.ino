#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

#define TFT_DC    17
#define TFT_CS    5
#define TFT_RST   16
#define TFT_CLK   18
#define TFT_MOSI  23
#define TFT_MISO  19

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

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
static DashboardCache previousFrame;
static bool hasPreviousFrame = false;

void drawDashboardShell();
void renderDashboardValues(const DashboardData& data);
DashboardData getDashboardData();
String formatClock(unsigned long seconds);
void drawCardLabel(const Rect& rect, const char* label);
void drawValueCard(const Rect& rect, const String& valueText, const char* unitText);
Rect makeValueArea(const Rect& card);
Rect makeUnitArea(const Rect& card);
const GFXfont* selectValueFont(const Rect& rect, const String& valueText);
void drawFixedTextDiff(const Rect& area, const String& previous, const String& next, uint16_t color, uint16_t bg);

void setup() {
  Serial.begin(115200);

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

  drawDashboardShell();
  renderDashboardValues(getDashboardData());
  lastRefreshMs = millis();
}

void loop() {
  const unsigned long now = millis();
  if (now - lastRefreshMs >= kRefreshIntervalMs) {
    renderDashboardValues(getDashboardData());
    lastRefreshMs = now;
  }
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
  nextFrame.temperature = String(data.temperatureC, 1);
  nextFrame.humidity = String(data.humidityPct, 1);
  nextFrame.pressure = String(data.pressureHpa, 1);
  nextFrame.soil = String(data.soilMoisturePct, 1);
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
  const float wave = sinf(static_cast<float>(uptimeSeconds) * 0.08f);
  const float slowWave = cosf(static_cast<float>(uptimeSeconds) * 0.04f);

  DashboardData data;
  data.temperatureC = 24.2f + (wave * 0.3f);
  data.humidityPct = 52.4f + (slowWave * 0.5f);
  data.pressureHpa = 1013.5f + (wave * 0.8f);
  data.soilMoisturePct = 62.0f + (slowWave * 0.7f);
  data.isOnline = (WiFi.status() == WL_CONNECTED);
  data.uptimeSeconds = uptimeSeconds;
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

void drawCardLabel(const Rect& rect, const char* label) {
  tft.setFont(nullptr);
  tft.setTextColor(Colors::label);
  tft.setTextSize(1);
  tft.setCursor(rect.x + 8, rect.y + 8);
  tft.print(label);
}

void drawValueCard(const Rect& rect, const String& valueText, const char* unitText) {
  const Rect valueArea = makeValueArea(rect);
  const Rect unitArea = makeUnitArea(rect);
  tft.fillRect(valueArea.x, valueArea.y, valueArea.w, valueArea.h, Colors::surface);
  tft.fillRect(unitArea.x, unitArea.y, unitArea.w, unitArea.h, Colors::surface);

  const GFXfont* valueFont = selectValueFont(rect, valueText);
  tft.setFont(valueFont);
  tft.setTextColor(Colors::value);
  tft.setTextSize(1);

  int16_t x1;
  int16_t y1;
  uint16_t valueW;
  uint16_t valueH;
  tft.getTextBounds(valueText, 0, 0, &x1, &y1, &valueW, &valueH);

  const int16_t baseX = valueArea.x + ((valueArea.w - static_cast<int16_t>(valueW)) / 2) - x1;
  const int16_t baselineY = valueArea.y + ((valueArea.h - static_cast<int16_t>(valueH)) / 2) - y1;
  tft.setCursor(baseX, baselineY);
  tft.print(valueText);

  tft.setFont(nullptr);
  tft.setTextColor(Colors::label);
  tft.setTextSize(1);
  tft.setCursor(unitArea.x + unitArea.w - (static_cast<int16_t>(strlen(unitText)) * 6), unitArea.y + 1);
  tft.print(unitText);
}

Rect makeValueArea(const Rect& card) {
  return {
    static_cast<int16_t>(card.x + 4),
    static_cast<int16_t>(card.y + 20),
    static_cast<int16_t>(card.w - 8),
    static_cast<int16_t>(card.h - 36)
  };
}

Rect makeUnitArea(const Rect& card) {
  return {
    static_cast<int16_t>(card.x + 4),
    static_cast<int16_t>(card.y + card.h - 12),
    static_cast<int16_t>(card.w - 8),
    10
  };
}

const GFXfont* selectValueFont(const Rect& rect, const String& valueText) {
  int16_t x1;
  int16_t y1;
  uint16_t textW;
  uint16_t textH;

  tft.setFont(&FreeSansBold18pt7b);
  tft.getTextBounds(valueText, 0, 0, &x1, &y1, &textW, &textH);
  if (textW <= static_cast<uint16_t>(rect.w - 12)) {
    return &FreeSansBold18pt7b;
  }

  tft.setFont(&FreeSansBold12pt7b);
  tft.getTextBounds(valueText, 0, 0, &x1, &y1, &textW, &textH);
  if (textW <= static_cast<uint16_t>(rect.w - 8)) {
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
