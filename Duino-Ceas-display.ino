#include <SPI.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <time.h>
#include <Wire.h>

// --- Configurație WiFi ---
WiFiMulti wifiMulti;

// --- Duino-Coin ---
const char* ducoUser = "my_cool_adis";
String apiUrl = String("https://server.duinocoin.com/balances/") + ducoUser;

// --- Pini TFT ---
#define TFT_CS   10
#define TFT_DC   2
#define TFT_RST  -1
#define TFT_BL   3
#define TFT_MOSI 7
#define TFT_SCLK 6
#define TFT_MISO -1

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

// --- Setări NTP ---
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600;
const int daylightOffset_sec = 0;

// Variabile ceas offline
time_t lastSynced = 0;   
unsigned long lastMillis = 0;
bool timeValid = false;  

// Timere
unsigned long lastWifiCheck   = 0;
unsigned long lastSyncAttempt = 0;
unsigned long lastApiCheck    = 0;

// --- CST816S Touch ---
#define I2C_SDA 4
#define I2C_SCL 5
#define CST816S_ADDR 0x15

struct TouchPoint {
  uint16_t x;
  uint16_t y;
  bool touched;
};

// --- Buton tactil ---
bool butonVizibil = true;
unsigned long momentAscuns = 0;
#define BTN_X 70
#define BTN_Y 195
#define BTN_W 95
#define BTN_H 30

// ------------------------------
//         AUTO DIMMING
// ------------------------------
unsigned long lastTouchTime = 0;
bool dimmed = false;

// ------------------------------
// PWM BACKLIGHT ESP32-C3 (analogWrite)
// ------------------------------
void initBacklightPWM() {
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, 255); // 100% luminozitate
}

void setBacklight(uint8_t val) {
  analogWrite(TFT_BL, val);  // val = 0–255
}

// ------------------------------
// Funcții generale
// ------------------------------

bool readTouch(TouchPoint &p) {
  Wire.beginTransmission(CST816S_ADDR);
  Wire.write(0x01);
  Wire.endTransmission();
  Wire.requestFrom(CST816S_ADDR, 6);

  if (Wire.available() < 6) return false;

  Wire.read(); // gesture ignorat
  uint8_t event = Wire.read();
  uint8_t xh = Wire.read();
  uint8_t xl = Wire.read();
  uint8_t yh = Wire.read();
  uint8_t yl = Wire.read();

  p.x = ((xh & 0x0F) << 8) | xl;
  p.y = ((yh & 0x0F) << 8) | yl;
  p.touched = (event != 0);
  return true;
}

void desenButon() {
  if (butonVizibil) {
    tft.fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 10, GC9A01A_BLUE);
    tft.setCursor(BTN_X + 10, BTN_Y + 9);
    tft.setTextSize(2);
    tft.setTextColor(GC9A01A_WHITE);
    tft.print("PAY-15'");
  } else {
    tft.fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 10, GC9A01A_GREEN);
    tft.setCursor(BTN_X + 6, BTN_Y + 9);
    tft.setTextSize(2);
    tft.setTextColor(GC9A01A_BLACK);
    tft.print("GiftPay");
  }
}

void verificaTouch() {
  TouchPoint p;

  if (readTouch(p) && p.touched) {
    lastTouchTime = millis(); // reset idle timer

    if (dimmed) {
      setBacklight(255);  // revenire 100%
      dimmed = false;
      Serial.println("Luminozitate 100% (atingere)");
    }

    if (butonVizibil &&
        p.x > BTN_X && p.x < (BTN_X + BTN_W) &&
        p.y > BTN_Y && p.y < (BTN_Y + BTN_H)) {

      Serial.println("Buton apasat!");
      butonVizibil = false;
      momentAscuns = millis();
      desenButon();
    }
  }
}

void actualizeazaTimpLocal() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    lastSynced = mktime(&timeinfo);
    lastMillis = millis();
    timeValid = true;
    Serial.println("Timp NTP sincronizat!");
  }
}

void afiseazaCeas() {
  if (!timeValid) {
    tft.setCursor(45, 150);
    tft.setTextSize(2);
    tft.setTextColor(GC9A01A_RED, GC9A01A_BLACK);
    tft.println("No Time Sync!");
    return;
  }

  time_t now = lastSynced + (millis() - lastMillis) / 1000;
  struct tm *timeinfo = localtime(&now);

  char timp[16];
  strftime(timp, sizeof(timp), "%H:%M:%S", timeinfo);

  tft.setCursor(27, 150);
  tft.setTextSize(4);
  tft.setTextColor(GC9A01A_YELLOW, GC9A01A_BLACK);
  tft.print(timp);
}

void desenCercMargine() {
  tft.drawCircle(tft.width()/2, tft.height()/2, tft.width()/2 - 1, GC9A01A_BLUE);
}

// ------------------------------
// SETUP
// ------------------------------
void setup() {
  Serial.begin(115200);

  initBacklightPWM();
  lastTouchTime = millis();

  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  delay(100);

  tft.begin();
  tft.fillScreen(GC9A01A_BLACK);
  desenCercMargine();

  tft.setTextColor(GC9A01A_BLUE);
  tft.setTextSize(2);
  tft.setCursor(20, 100);
  tft.println("Conectare WiFi...");

  Wire.begin(I2C_SDA, I2C_SCL);
  delay(50);

  wifiMulti.addAP("%netrunoff%power ", "05111971232826#A#b#cc");
  wifiMulti.addAP("%secretclub%power1", "19711944x..");

  if (wifiMulti.run() == WL_CONNECTED) {

    configTzTime("EET-2EEST,M3.5.0/3,M10.5.0/4", ntpServer);
    actualizeazaTimpLocal();

    tft.fillScreen(GC9A01A_BLACK);
    desenCercMargine();

    tft.setTextColor(GC9A01A_GREEN);
    tft.setCursor(70, 100);
    tft.println("WiFi OK!");

  } else {
    tft.fillScreen(GC9A01A_BLACK);
    desenCercMargine();
    tft.setTextColor(GC9A01A_RED);
    tft.setCursor(50, 100);
    tft.println("No WiFi...");
  }

  desenButon();
}

// ------------------------------
// LOOP
// ------------------------------
void loop() {
  unsigned long currentMillis = millis();

  // Reapare buton dupa 15 minute
  if (!butonVizibil && (millis() - momentAscuns >= 900000)) {
    butonVizibil = true;
    desenButon();
  }

  // Reconnect WiFi
  if (currentMillis - lastWifiCheck > 10000) {
    wifiMulti.run();
    lastWifiCheck = currentMillis;
  }

  if (WiFi.status() == WL_CONNECTED) {

    if (currentMillis - lastSyncAttempt > 60000) {
      actualizeazaTimpLocal();
      lastSyncAttempt = currentMillis;
    }

    if (currentMillis - lastApiCheck > 30000) {

      HTTPClient http;
      http.begin(apiUrl);

      int httpCode = http.GET();
      if (httpCode == 200) {

        String payload = http.getString();

        tft.setCursor(85, 20);
        tft.setTextSize(3);
        tft.setTextColor(GC9A01A_BLUE);
        tft.println("PAY!");
        delay(2000);

        StaticJsonDocument<512> doc;
        if (deserializeJson(doc, payload) == DeserializationError::Ok) {

          float balance = doc["result"]["balance"].as<float>();

          tft.fillScreen(GC9A01A_BLACK);
          desenCercMargine();
          tft.setCursor(50, 55);
          tft.setTextSize(2);
          tft.setTextColor(GC9A01A_WHITE);
          tft.println("DUCO Wallet:");

          tft.setCursor(35, 100);
          tft.setTextSize(3);
          tft.setTextColor(GC9A01A_GREEN);
          tft.println(balance, 4);

          afiseazaCeas();
          desenButon();
        }

      } else {
        tft.fillScreen(GC9A01A_BLACK);
        desenCercMargine();
        tft.setCursor(45, 100);
        tft.setTextSize(2);
        tft.setTextColor(GC9A01A_RED);
        tft.println("Eroare API !!!");
        desenButon();
      }

      http.end();
      lastApiCheck = currentMillis;
    }

  } else {
    tft.fillScreen(GC9A01A_BLACK);
    desenCercMargine();
    tft.setTextColor(GC9A01A_RED);
    tft.setCursor(50, 100);
    tft.setTextSize(2);
    tft.println("No WiFi...");
    afiseazaCeas();
    desenButon();
    delay(1000);
  }

  // Actualizare ceas la fiecare secundă
  static unsigned long lastClockUpdate = 0;
  if (currentMillis - lastClockUpdate > 1000) {
    afiseazaCeas();
    desenButon();
    lastClockUpdate = currentMillis;
  }

  // Touch
  verificaTouch();

  // AUTO-DIMMING 3 MIN
  unsigned long idleTime = millis() - lastTouchTime;
  if (!dimmed && idleTime > 60000) { // 3 minute
    setBacklight(15);  // 20%
    dimmed = true;
    Serial.println("Dimming ON (20%)");
  }
}
