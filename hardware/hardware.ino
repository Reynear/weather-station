#include <Arduino.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <string.h>

#include <Adafruit_GFX.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_ILI9341.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

#define TFT_DC 18
#define TFT_CS 23
#define TFT_RST 19
#define TFT_CLK 17
#define TFT_MOSI 5
#define TFT_MISO 16

#define I2C_SDA 21
#define I2C_SCL 22
#define DHT_PIN 4
#define DHT_TYPE DHT22
#define SOIL_PIN 34
#define SOIL_RAW_DRY 2300
#define SOIL_RAW_WET 400
#define SOIL_SAMPLES 8

#define BMP280_ADDR 0x76
#define SEA_LEVEL_HPA 1011.0f
#define SCREEN_W 240
#define SCREEN_H 320
#define HEADER_H 24
#define MARGIN 6
#define GUTTER 4
#define CARD_W ((SCREEN_W - (MARGIN * 2) - GUTTER) / 2)
#define CARD_H ((SCREEN_H - HEADER_H - (MARGIN * 2) - (GUTTER * 2)) / 3)

#define REFRESH_MS 1000UL
#define WIFI_RETRY_MS 10000UL
#define MQTT_RETRY_MS 10000UL
#define DHT_WAIT_MS 2000UL

static const char *ssid = "MonaConnect";
static const char *password = "";
static const char *mqtt_host = "www.yanacreations.com";
static const uint16_t mqtt_port = 1883;
static const char *mqtt_client_id = "620171008-hardware";
static const char *mqtt_publish_topic = "620171008_sub";
static const char *mqtt_subscribe_topic = "620171008_pub";

typedef struct {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
} Rect;

static const Rect TEMP_CARD = {MARGIN, HEADER_H + MARGIN, CARD_W, CARD_H};
static const Rect HUM_CARD = {MARGIN + CARD_W + GUTTER, HEADER_H + MARGIN, CARD_W, CARD_H};
static const Rect PRESS_CARD = {MARGIN, HEADER_H + MARGIN + CARD_H + GUTTER, CARD_W, CARD_H};
static const Rect SOIL_CARD = {MARGIN + CARD_W + GUTTER, HEADER_H + MARGIN + CARD_H + GUTTER, CARD_W, CARD_H};
static const Rect ALT_CARD = {MARGIN, HEADER_H + MARGIN + (CARD_H + GUTTER) * 2, CARD_W, CARD_H};
static const Rect HI_CARD = {MARGIN + CARD_W + GUTTER, HEADER_H + MARGIN + (CARD_H + GUTTER) * 2, CARD_W, CARD_H};
static const Rect CLOCK_AREA = {SCREEN_W - 60, 0, 54, HEADER_H};

static Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);
static Adafruit_BMP280 bmp;
static DHT dht(DHT_PIN, DHT_TYPE);
static WiFiClient wifi_client;
static PubSubClient mqtt_client(wifi_client);

static uint16_t bg_color;
static uint16_t card_color;
static uint16_t label_color;
static uint16_t value_color;
static uint16_t good_color;
static uint16_t bad_color;
static uint16_t hot_color;
static uint16_t cold_color;
static uint16_t warning_color;

static unsigned long last_refresh = 0;
static unsigned long last_wifi_retry = 0;
static unsigned long last_mqtt_retry = 0;
static unsigned long last_dht_read = 0;

static float last_dht_temp = NAN;
static float last_dht_hum = NAN;
static int bmp_ready = 0;

static float current_temp = NAN;
static float current_hum = NAN;
static float current_pressure = NAN;
static float current_altitude = NAN;
static float current_heat_index = NAN;
static float current_soil = NAN;
static int current_online = 0;
static unsigned long current_uptime = 0;

static char shown_temp[16] = "";
static char shown_hum[16] = "";
static char shown_press[16] = "";
static char shown_soil[16] = "";
static char shown_alt[16] = "";
static char shown_hi[16] = "";
static char shown_clock[16] = "";
static int shown_online = -1;
static int first_frame = 1;

static int bmp280_init(void);
static int bmp280_read(void);
static int dht22_read(void);
static int soil_read(void);
static void wifi_connect(void);
static void wifi_check(void);
static void mqtt_setup(void);
static void mqtt_check(void);
static int mqtt_publish_snapshot(void);
static void mqtt_message_received(char *topic, byte *payload, unsigned int length);
static void format_json_number(char *dest, size_t size, float value, uint8_t decimals);
static void draw_shell(void);
static void draw_label(Rect r, const char *text);
static void draw_card(Rect r, const char *value, const char *unit, float raw_val, int is_percentage);
static void draw_diff(Rect r, const char *old_text, const char *new_text, uint16_t fg, uint16_t bg);
static void render_values(void);
static void log_values(void);

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  dht.begin();
  pinMode(SOIL_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_PIN, ADC_11db);
  wifi_connect();
  mqtt_setup();

  tft.begin();
  tft.setRotation(0);
  tft.setTextWrap(false);

  bg_color = tft.color565(18, 18, 18);
  card_color = tft.color565(37, 37, 37);
  label_color = tft.color565(160, 160, 160);
  value_color = tft.color565(255, 255, 255);
  good_color = tft.color565(0, 230, 118);
  bad_color = tft.color565(255, 82, 82);
  hot_color = tft.color565(255, 138, 101);
  cold_color = tft.color565(129, 212, 250);
  warning_color = tft.color565(255, 213, 79);

  bmp280_init();
  draw_shell();

  dht22_read();
  bmp280_read();
  soil_read();
  current_online = WiFi.status() == WL_CONNECTED;
  current_uptime = millis() / 1000UL;
  mqtt_check();
  mqtt_publish_snapshot();
  render_values();
  log_values();
  last_refresh = millis();
}

void loop() {
  unsigned long now = millis();
  wifi_check();
  mqtt_check();
  mqtt_client.loop();

  if (now - last_refresh < REFRESH_MS) {
    return;
  }

  dht22_read();
  bmp280_read();
  soil_read();
  current_online = WiFi.status() == WL_CONNECTED;
  current_uptime = now / 1000UL;
  mqtt_publish_snapshot();
  render_values();
  log_values();
  last_refresh = now;
}

static void wifi_connect(void) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  Serial.printf("Connecting to %s \n", ssid);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000UL) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n***** Wi-Fi CONNECTED! *****");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWi-Fi connection failed");
    last_wifi_retry = millis();
  }
}

static void wifi_check(void) {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  if (millis() - last_wifi_retry < WIFI_RETRY_MS) {
    return;
  }
  Serial.printf("Connecting to %s \n", ssid);
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  last_wifi_retry = millis();
}

static void mqtt_setup(void) {
  mqtt_client.setServer(mqtt_host, mqtt_port);
  mqtt_client.setCallback(mqtt_message_received);
}

static void mqtt_check(void) {
  if (WiFi.status() != WL_CONNECTED || mqtt_client.connected()) {
    return;
  }
  if (millis() - last_mqtt_retry < MQTT_RETRY_MS) {
    return;
  }

  Serial.printf("\nMQTT Server : %s   PORT : %d\n", mqtt_host, mqtt_port);
  Serial.printf("MQTT Connection ID: %s\n", mqtt_client_id);

  if (mqtt_client.connect(mqtt_client_id)) {
    Serial.println("\n\n***** MQTT CONNECTED! *****\n\n"); 
    mqtt_client.subscribe(mqtt_subscribe_topic);
  } else {
    Serial.printf(
        "\nConnection failed with status code : %d ,  re-trying in 10 seconds\n",
        mqtt_client.state());
  }

  last_mqtt_retry = millis();
}

static int mqtt_publish_snapshot(void) {
  if (!mqtt_client.connected()) {
    return 0;
  }

  char temp_text[16];
  char hum_text[16];
  char press_text[16];
  char soil_text[16];
  char alt_text[16];
  char hi_text[16];
  char payload[320];

  format_json_number(temp_text, sizeof(temp_text), current_temp, 1);
  format_json_number(hum_text, sizeof(hum_text), current_hum, 1);
  format_json_number(press_text, sizeof(press_text), current_pressure, 1);
  format_json_number(soil_text, sizeof(soil_text), current_soil, 1);
  format_json_number(alt_text, sizeof(alt_text), current_altitude, 1);
  format_json_number(hi_text, sizeof(hi_text), current_heat_index, 1);

  snprintf(
      payload,
      sizeof(payload),
      "{\"temperature\":%s,\"humidity\":%s,\"pressure\":%s,"
      "\"soil_moisture\":%s,\"altitude\":%s,\"heat_index\":%s}",
      temp_text,
      hum_text,
      press_text,
      soil_text,
      alt_text,
      hi_text);

  if (mqtt_client.publish(mqtt_publish_topic, payload)) {
    Serial.print("mqtt pub ");
    Serial.println(payload);
    return 1;
  }

  Serial.printf("Error (%d) >> Unable to publish message\n", mqtt_client.state());
  return 0;
}

static void mqtt_message_received(char *topic, byte *payload, unsigned int length) {
  Serial.print("mqtt recv ");
  Serial.print(topic);
  Serial.print(" ");

  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

static void format_json_number(char *dest, size_t size, float value, uint8_t decimals) {
  if (isnan(value)) {
    snprintf(dest, size, "null");
    return;
  }
  snprintf(dest, size, "%.*f", decimals, value);
}

static int dht22_read(void) {
  unsigned long now = millis();

  if (now - last_dht_read < DHT_WAIT_MS && !isnan(last_dht_temp) && !isnan(last_dht_hum)) {
    current_temp = last_dht_temp;
    current_hum = last_dht_hum;
    current_heat_index = dht.computeHeatIndex(current_temp, current_hum, false);
    return 1;
  }

  float hum = dht.readHumidity();
  float temp = dht.readTemperature();
  if (isnan(temp) || isnan(hum)) {
    Serial.println("dht fail");
    return 0;
  }

  last_dht_read = now;
  last_dht_temp = temp;
  last_dht_hum = hum;
  current_temp = temp;
  current_hum = hum;
  current_heat_index = dht.computeHeatIndex(current_temp, current_hum, false);
  return 1;
}

static int bmp280_init(void) {
  bmp_ready = 0;
  if (!bmp.begin(BMP280_ADDR)) {
    Serial.println("bmp280 fail");
    return 0;
  }
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X1,
                  Adafruit_BMP280::SAMPLING_X4,
                  Adafruit_BMP280::FILTER_OFF,
                  Adafruit_BMP280::STANDBY_MS_500);
  bmp_ready = 1;
  return 1;
}

static int bmp280_read(void) {
  if (!bmp_ready) {
    current_pressure = NAN;
    current_altitude = NAN;
    return 0;
  }
  current_pressure = bmp.readPressure() / 100.0f;
  if (isnan(current_pressure) || current_pressure <= 0.0f) {
    Serial.println("bmp280 fail");
    current_pressure = NAN;
    current_altitude = NAN;
    return 0;
  }
  current_altitude = bmp.readAltitude(SEA_LEVEL_HPA);
  return 1;
}

static int soil_read(void) {
  uint32_t total = 0;

  for (int i = 0; i < SOIL_SAMPLES; i++) {
    total += (uint32_t)analogRead(SOIL_PIN);
  }

  int raw = (int)(total / SOIL_SAMPLES);
  raw = constrain(raw, SOIL_RAW_WET, SOIL_RAW_DRY);
  current_soil = (float)map(raw, SOIL_RAW_DRY, SOIL_RAW_WET, 0, 100);
  return 1;
}

static void draw_shell(void) {
  tft.fillScreen(bg_color);
  tft.fillRoundRect(TEMP_CARD.x, TEMP_CARD.y, TEMP_CARD.w, TEMP_CARD.h, 6, card_color);
  tft.fillRoundRect(HUM_CARD.x, HUM_CARD.y, HUM_CARD.w, HUM_CARD.h, 6, card_color);
  tft.fillRoundRect(PRESS_CARD.x, PRESS_CARD.y, PRESS_CARD.w, PRESS_CARD.h, 6, card_color);
  tft.fillRoundRect(SOIL_CARD.x, SOIL_CARD.y, SOIL_CARD.w, SOIL_CARD.h, 6, card_color);
  tft.fillRoundRect(ALT_CARD.x, ALT_CARD.y, ALT_CARD.w, ALT_CARD.h, 6, card_color);
  tft.fillRoundRect(HI_CARD.x, HI_CARD.y, HI_CARD.w, HI_CARD.h, 6, card_color);

  draw_label(TEMP_CARD, "TEMP");
  draw_label(HUM_CARD, "HUMIDITY");
  draw_label(PRESS_CARD, "PRESSURE");
  draw_label(SOIL_CARD, "SOIL");
  draw_label(ALT_CARD, "ALTITUDE");
  draw_label(HI_CARD, "HEAT INDEX");

  tft.setFont(NULL);
  tft.setTextColor(label_color);
  tft.setTextSize(1);
  tft.setCursor(MARGIN + 12, 8);
  tft.print("WIFI");
}

static void draw_label(Rect r, const char *text) {
  tft.setFont(NULL);
  tft.setTextColor(label_color);
  tft.setTextSize(1);
  tft.setCursor(r.x + 8, r.y + 8);
  tft.print(text);
}

static void draw_card(Rect r, const char *value, const char *unit, float raw_val, int is_percentage) {
  int16_t area_x = r.x + 4;
  int16_t area_y = r.y + 20;
  int16_t area_w = r.w - 8;
  int16_t area_h = r.h - 24;
  size_t len = strlen(value);

  GFXcanvas16 canvas(area_w, area_h);
  canvas.fillScreen(card_color);
  if (len <= 5) {
    canvas.setFont(&FreeSansBold18pt7b);
  } else if (len <= 7) {
    canvas.setFont(&FreeSansBold12pt7b);
  } else {
    canvas.setFont(&FreeSansBold9pt7b);
  }

  uint16_t text_color = value_color;
  if (!isnan(raw_val)) {
    if (strcmp(unit, "C") == 0) {
      if (raw_val >= 30.0f) text_color = hot_color;
      else if (raw_val <= 20.0f) text_color = cold_color;
      else text_color = value_color;
    }
  }
  canvas.setTextColor(text_color);
  canvas.setTextSize(1);

  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  canvas.getTextBounds(value, 0, 0, &x1, &y1, &w, &h);
  canvas.setCursor(((area_w - (int16_t)w) / 2) - x1, ((area_h - (int16_t)h) / 2) - y1 - 4);
  canvas.print(value);

  canvas.setFont(NULL);
  canvas.setTextColor(label_color);
  canvas.setTextSize(1);
  canvas.setCursor(area_w - ((int16_t)strlen(unit) * 6) - 4, area_h - 15);
  canvas.print(unit);

  if (is_percentage && !isnan(raw_val)) {
    int16_t bar_w = (int16_t)((raw_val / 100.0f) * area_w);
    if (bar_w > area_w) bar_w = area_w;
    if (bar_w < 0) bar_w = 0;
    uint16_t bar_color = (raw_val < 30.0f) ? warning_color : good_color;
    canvas.fillRect(0, area_h - 4, bar_w, 4, bar_color);
  }

  tft.drawRGBBitmap(area_x, area_y, canvas.getBuffer(), canvas.width(), canvas.height());
}

static void draw_diff(Rect r, const char *old_text, const char *new_text, uint16_t fg, uint16_t bg) {
  size_t old_len = strlen(old_text);
  size_t new_len = strlen(new_text);
  size_t n = old_len > new_len ? old_len : new_len;

  tft.setFont(NULL);
  tft.setTextSize(1);

  for (size_t i = 0; i < n && i < (size_t)(r.w / 6); i++) {
    char a = i < old_len ? old_text[i] : ' ';
    char b = i < new_len ? new_text[i] : ' ';
    if (a == b) {
      continue;
    }
    tft.drawChar(r.x + (int16_t)(i * 6), r.y + ((r.h - 8) / 2), b, fg, bg, 1);
  }
}

static void render_values(void) {
  char temp[16];
  char hum[16];
  char press[16];
  char soil[16];
  char alt[16];
  char hi[16];
  char clock_text[16];

  if (isnan(current_temp)) snprintf(temp, sizeof(temp), "--");
  else snprintf(temp, sizeof(temp), "%.1f", current_temp);
  if (isnan(current_hum)) snprintf(hum, sizeof(hum), "--");
  else snprintf(hum, sizeof(hum), "%.1f", current_hum);
  if (isnan(current_pressure)) snprintf(press, sizeof(press), "--");
  else snprintf(press, sizeof(press), "%.1f", current_pressure);
  if (isnan(current_soil)) snprintf(soil, sizeof(soil), "--");
  else snprintf(soil, sizeof(soil), "%.1f", current_soil);
  if (isnan(current_altitude)) snprintf(alt, sizeof(alt), "--");
  else snprintf(alt, sizeof(alt), "%.0f", current_altitude);
  if (isnan(current_heat_index)) snprintf(hi, sizeof(hi), "--");
  else snprintf(hi, sizeof(hi), "%.1f", current_heat_index);

  snprintf(clock_text, sizeof(clock_text), "%02lu:%02lu:%02lu",
           (current_uptime / 3600UL) % 24UL,
           (current_uptime / 60UL) % 60UL,
           current_uptime % 60UL);

  if (first_frame || current_online != shown_online) {
    tft.fillCircle(MARGIN + 4, 11, 3, current_online ? good_color : bad_color);
    shown_online = current_online;
  }
  if (first_frame || strcmp(clock_text, shown_clock) != 0) {
    draw_diff(CLOCK_AREA, shown_clock, clock_text, value_color, bg_color);
    strncpy(shown_clock, clock_text, sizeof(shown_clock) - 1);
    shown_clock[sizeof(shown_clock) - 1] = 0;
  }
  if (first_frame || strcmp(temp, shown_temp) != 0) {
    draw_card(TEMP_CARD, temp, "C", current_temp, 0);
    strncpy(shown_temp, temp, sizeof(shown_temp) - 1);
    shown_temp[sizeof(shown_temp) - 1] = 0;
  }
  if (first_frame || strcmp(hum, shown_hum) != 0) {
    draw_card(HUM_CARD, hum, "%", current_hum, 1);
    strncpy(shown_hum, hum, sizeof(shown_hum) - 1);
    shown_hum[sizeof(shown_hum) - 1] = 0;
  }
  if (first_frame || strcmp(press, shown_press) != 0) {
    draw_card(PRESS_CARD, press, "hPa", current_pressure, 0);
    strncpy(shown_press, press, sizeof(shown_press) - 1);
    shown_press[sizeof(shown_press) - 1] = 0;
  }
  if (first_frame || strcmp(soil, shown_soil) != 0) {
    draw_card(SOIL_CARD, soil, "%", current_soil, 1);
    strncpy(shown_soil, soil, sizeof(shown_soil) - 1);
    shown_soil[sizeof(shown_soil) - 1] = 0;
  }
  if (first_frame || strcmp(alt, shown_alt) != 0) {
    draw_card(ALT_CARD, alt, "m", current_altitude, 0);
    strncpy(shown_alt, alt, sizeof(shown_alt) - 1);
    shown_alt[sizeof(shown_alt) - 1] = 0;
  }
  if (first_frame || strcmp(hi, shown_hi) != 0) {
    draw_card(HI_CARD, hi, "C", current_heat_index, 0);
    strncpy(shown_hi, hi, sizeof(shown_hi) - 1);
    shown_hi[sizeof(shown_hi) - 1] = 0;
  }

  first_frame = 0;
}

static void log_values(void) {
  Serial.print("Temp: ");
  if (isnan(current_temp)) {
    Serial.print("--");
  } else {
    Serial.print(current_temp, 1);
  }

  Serial.print(" C  Humidity: ");
  if (isnan(current_hum)) {
    Serial.print("--");
  } else {
    Serial.print(current_hum, 1);
  }

  Serial.print(" %  Pressure: ");
  if (isnan(current_pressure)) {
    Serial.print("--");
  } else {
    Serial.print(current_pressure, 1);
  }

  Serial.print(" hPa  Soil: ");
  if (isnan(current_soil)) {
    Serial.print("--");
  } else {
    Serial.print(current_soil, 1);
  }
  Serial.print(" %  Altitude: ");
  if (isnan(current_altitude)) {
    Serial.print("--");
  } else {
    Serial.print(current_altitude, 0);
  }
  Serial.print(" m  Heat Index: ");
  if (isnan(current_heat_index)) {
    Serial.print("--");
  } else {
    Serial.print(current_heat_index, 1);
  }
  Serial.println(" C");
}
