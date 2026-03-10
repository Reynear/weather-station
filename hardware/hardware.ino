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
  float lightLux;
  const char* networkStatus;
  unsigned long uptimeSeconds;
};

struct DashboardCache {
  String temperature;
  String humidity;
  String pressure;
  String soil;
  String light;
  String network;
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
  constexpr int16_t kHeaderH = 0;
  constexpr int16_t kGridY = kHeaderH;
  constexpr int16_t kCardH = (kScreenH - kHeaderH) / 3;
  constexpr int16_t kCol0W = 120;
  constexpr int16_t kCol1W = 120;

  constexpr Rect kTempCard = {0, kGridY, kCol0W, kCardH};
  constexpr Rect kHumidityCard = {kCol0W, kGridY, kCol1W, kCardH};
  constexpr Rect kPressureCard = {0, kGridY + kCardH, kCol0W, kCardH};
  constexpr Rect kSoilCard = {kCol0W, kGridY + kCardH, kCol1W, kCardH};
  constexpr Rect kLightCard = {0, kGridY + (kCardH * 2), kCol0W, kCardH};
  constexpr Rect kSystemCard = {kCol0W, kGridY + (kCardH * 2), kCol1W, kCardH};
}

namespace Colors {
  uint16_t background;
  uint16_t surface;
  uint16_t line;
  uint16_t label;
  uint16_t value;
  uint16_t accent;
  uint16_t good;
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
void drawSystemRows();
Rect makeValueArea(const Rect& card);
Rect makeUnitArea(const Rect& card);
Rect makeSystemValueArea(uint8_t rowIndex);
const GFXfont* selectValueFont(const Rect& rect, const String& valueText);
void drawRightAlignedText(const Rect& area, const String& text, uint16_t color, const GFXfont* font);
void drawFixedTextDiff(const Rect& area, const String& previous, const String& next, uint16_t color);

void setup() {
  Serial.begin(115200);

  tft.begin();
  tft.setRotation(0);
  tft.setTextWrap(false);

  Colors::background = tft.color565(242, 244, 247);
  Colors::surface = tft.color565(248, 249, 251);
  Colors::line = tft.color565(214, 219, 226);
  Colors::label = tft.color565(133, 143, 159);
  Colors::value = tft.color565(18, 33, 68);
  Colors::accent = tft.color565(54, 66, 88);
  Colors::good = tft.color565(34, 88, 61);

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

  tft.fillRect(0, 0, Layout::kScreenW, Layout::kScreenH, Colors::surface);
  tft.drawFastHLine(0, Layout::kGridY + Layout::kCardH, Layout::kScreenW, Colors::line);
  tft.drawFastHLine(0, Layout::kGridY + (Layout::kCardH * 2), Layout::kScreenW, Colors::line);
  tft.drawFastVLine(Layout::kCol0W, Layout::kGridY, Layout::kScreenH, Colors::line);

  drawCardLabel(Layout::kTempCard, "TEMPERATURE");
  drawCardLabel(Layout::kHumidityCard, "HUMIDITY");
  drawCardLabel(Layout::kPressureCard, "PRESSURE");
  drawCardLabel(Layout::kSoilCard, "SOIL");
  drawCardLabel(Layout::kLightCard, "LIGHT");
  drawCardLabel(Layout::kSystemCard, "SYSTEM");
  drawSystemRows();
}

void renderDashboardValues(const DashboardData& data) {
  DashboardCache nextFrame;
  nextFrame.temperature = String(data.temperatureC, 1);
  nextFrame.humidity = String(data.humidityPct, 1);
  nextFrame.pressure = String(data.pressureHpa, 1);
  nextFrame.soil = String(data.soilMoisturePct, 1);
  nextFrame.light = String(data.lightLux, 1);
  nextFrame.network = String(data.networkStatus);
  nextFrame.clock = formatClock(data.uptimeSeconds);

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
  if (!hasPreviousFrame || nextFrame.light != previousFrame.light) {
    drawValueCard(Layout::kLightCard, nextFrame.light, "lux");
  }

  drawFixedTextDiff(makeSystemValueArea(0), hasPreviousFrame ? previousFrame.network : "", nextFrame.network, Colors::value);
  drawFixedTextDiff(makeSystemValueArea(1), hasPreviousFrame ? previousFrame.clock : "", nextFrame.clock, Colors::value);

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
  data.lightLux = 648.4f + (wave * 6.0f);
  data.networkStatus = (WiFi.status() == WL_CONNECTED) ? "ONLINE" : "OFFLINE";
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

void drawSystemRows() {
  tft.setFont(nullptr);
  tft.setTextColor(Colors::label);
  tft.setTextSize(1);

  const int16_t left = Layout::kSystemCard.x + 8;
  const int16_t networkRowY = Layout::kSystemCard.y + 24;
  const int16_t timeRowY = Layout::kSystemCard.y + 56;

  tft.setCursor(left, networkRowY);
  tft.print("NETWORK");
  tft.drawFastHLine(Layout::kSystemCard.x + 8, networkRowY + 9, Layout::kSystemCard.w - 16, Colors::line);

  tft.setCursor(left, timeRowY);
  tft.print("TIME");
  tft.drawFastHLine(Layout::kSystemCard.x + 8, timeRowY + 9, Layout::kSystemCard.w - 16, Colors::line);
}

Rect makeValueArea(const Rect& card) {
  return {
    static_cast<int16_t>(card.x + 6),
    static_cast<int16_t>(card.y + 24),
    static_cast<int16_t>(card.w - 12),
    static_cast<int16_t>(card.h - 44)
  };
}

Rect makeUnitArea(const Rect& card) {
  return {
    static_cast<int16_t>(card.x + 6),
    static_cast<int16_t>(card.y + card.h - 14),
    static_cast<int16_t>(card.w - 12),
    10
  };
}

Rect makeSystemValueArea(uint8_t rowIndex) {
  const int16_t baseY = Layout::kSystemCard.y + ((rowIndex == 0) ? 40 : 72);
  return {
    static_cast<int16_t>(Layout::kSystemCard.x + 8),
    baseY,
    static_cast<int16_t>(Layout::kSystemCard.w - 16),
    12
  };
}

const GFXfont* selectValueFont(const Rect& rect, const String& valueText) {
  int16_t x1;
  int16_t y1;
  uint16_t textW;
  uint16_t textH;

  tft.setFont(&FreeSansBold18pt7b);
  tft.getTextBounds(valueText, 0, 0, &x1, &y1, &textW, &textH);
  if (textW <= static_cast<uint16_t>(rect.w - 18)) {
    return &FreeSansBold18pt7b;
  }

  tft.setFont(&FreeSansBold12pt7b);
  tft.getTextBounds(valueText, 0, 0, &x1, &y1, &textW, &textH);
  if (textW <= static_cast<uint16_t>(rect.w - 16)) {
    return &FreeSansBold12pt7b;
  }

  return &FreeSansBold9pt7b;
}

void drawRightAlignedText(const Rect& area, const String& text, uint16_t color, const GFXfont* font) {
  tft.fillRect(area.x, area.y, area.w, area.h, Colors::surface);
  tft.setFont(font);
  tft.setTextColor(color);
  tft.setTextSize(1);

  int16_t x1;
  int16_t y1;
  uint16_t textW;
  uint16_t textH;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &textW, &textH);
  const int16_t cursorX = area.x + area.w - static_cast<int16_t>(textW) - x1;
  const int16_t cursorY = area.y + area.h - 1;
  tft.setCursor(cursorX, cursorY);
  tft.print(text);
}

void drawFixedTextDiff(const Rect& area, const String& previous, const String& next, uint16_t color) {
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
    tft.drawChar(cellX, cellY, nextCh, color, Colors::surface, 1);
  }
}
