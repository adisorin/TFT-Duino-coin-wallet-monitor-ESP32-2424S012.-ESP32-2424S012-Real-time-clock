#include <SPI.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <time.h>
#include <Wire.h>   // pentru touchscreen CST816S

// --- Configurație WiFi ---
WiFiMulti wifiMulti;

// --- Duino-Coin ---
const char* ducoUser = "user account";// cange user account
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

// Variabile pentru ceas offline
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

// Poziția și dimensiunile butonului pe ecran
#define BTN_X 70
#define BTN_Y 195
#define BTN_W 95
#define BTN_H 30

// ---------------------------
// FUNCȚII
// ---------------------------

// Citește coordonatele atingerii de la CST816S
bool readTouch(TouchPoint &p) {
  Wire.beginTransmission(CST816S_ADDR);
  Wire.write(0x01);  // registru date touch
  Wire.endTransmission();
  Wire.requestFrom(CST816S_ADDR, 6);

  if (Wire.available() < 6) return false;

  Wire.read(); // Gesture (ignorat acum)
  uint8_t event = Wire.read(); // 0=down, 2=contact
  uint8_t xh = Wire.read();
  uint8_t xl = Wire.read();
  uint8_t yh = Wire.read();
  uint8_t yl = Wire.read();

  p.x = ((xh & 0x0F) << 8) | xl;
  p.y = ((yh & 0x0F) << 8) | yl;
  p.touched = (event != 0);// sau (event != 0 || event == 2);

  return true;
}

// Desenează butonul
void desenButon() {
  if (butonVizibil) {
    tft.fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 10, GC9A01A_BLUE);
    tft.setCursor(BTN_X + 10, BTN_Y + 9);
    tft.setTextSize(2);
    tft.setTextColor(GC9A01A_WHITE);
    tft.print("PAY-15'");
  } else {
    // Schimbare culoare buton
    tft.fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 10, GC9A01A_GREEN);
    tft.setCursor(BTN_X + 6, BTN_Y + 9);
    tft.setTextSize(2);
    tft.setTextColor(GC9A01A_BLACK);
    tft.print("GiftPay");
  }
}

// Verifică dacă butonul a fost apăsat
void verificaTouch() {
  TouchPoint p;

  if (butonVizibil && readTouch(p) && p.touched) {
    if (p.x > BTN_X && p.x < (BTN_X + BTN_W) &&
        p.y > BTN_Y && p.y < (BTN_Y + BTN_H)) {
      Serial.println("Buton apasat!");
      butonVizibil = false;             // ascundem butonul
      momentAscuns = millis();          // salvăm momentul
      desenButon();
    }
  }
}

// Actualizează timpul local prin NTP
void actualizeazaTimpLocal() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    lastSynced = mktime(&timeinfo);
    lastMillis = millis();
    timeValid = true;
    Serial.println("Timp NTP sincronizat!");
  }
}

// Desenează ceasul pe ecran
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

// Desenează cercul de contur pe marginea ecranului
void desenCercMargine() {
  tft.drawCircle(tft.width()/2, tft.height()/2, tft.width()/2 - 1, GC9A01A_BLUE);
}

// ---------------------------
// SETUP
// ---------------------------
void setup() {
  Serial.begin(115200);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  delay(100);

  tft.begin();
  tft.fillScreen(GC9A01A_BLACK);

  desenCercMargine(); // cerc pe margine

  tft.setTextColor(GC9A01A_BLUE);
  tft.setTextSize(2);
  tft.setCursor(20, 100);
  tft.println("Conectare WiFi...");

  // Inițializare touchscreen
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(50);
  Serial.println("CST816S init OK");

  // Adaugă rețele WiFi
  wifiMulti.addAP("SSID1", "password1");//Change here
  wifiMulti.addAP("SSID2", "password2");//Change here

  if (wifiMulti.run() == WL_CONNECTED) {
    Serial.println("\nConectat la WiFi!");
// Setează automat fusul orar pentru România cu DST ///////////////
    configTzTime("EET-2EEST,M3.5.0/3,M10.5.0/4", ntpServer);
    //setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/3", 1);//anulat
    tzset();

    //configTime(1 * 3600, 3600, "pool.ntp.org", "time.nist.gov");//anulat
    ///////////////////////////////////////////////////////////////
    actualizeazaTimpLocal();

    tft.fillScreen(GC9A01A_BLACK);
    desenCercMargine(); // redesenăm cercul după fillScreen

    tft.setTextColor(GC9A01A_GREEN);
    tft.setCursor(70, 100);
    tft.println("WiFi OK!");
  } else {
    Serial.println("Pornit fara WiFi!");
    tft.fillScreen(GC9A01A_BLACK);
    desenCercMargine(); // redesenăm cercul

    tft.setTextColor(GC9A01A_RED);
    tft.setCursor(50, 100);
    tft.println("No WiFi...");
  }

  desenButon(); // afișăm butonul la pornire
}

// ---------------------------
// LOOP
// ---------------------------
void loop() {
  unsigned long currentMillis = millis();

  // reapariția butonului după 15 minute (900000 ms) //Announce that the "Duino Coin Faucet - AMOGUS
" time has passed.
  if (!butonVizibil && (millis() - momentAscuns >= 900000)) {
    butonVizibil = true;
    Serial.println("Buton reapare!");
    desenButon();
  }

  // reconectare WiFi la fiecare 10 secunde
  if (currentMillis - lastWifiCheck > 10000) {
    wifiMulti.run();
    lastWifiCheck = currentMillis;
  }

  if (WiFi.status() == WL_CONNECTED) {
    // sincronizare NTP la fiecare minut
    if (currentMillis - lastSyncAttempt > 60000) {
      actualizeazaTimpLocal();
      lastSyncAttempt = currentMillis;
    }

    // cerere API la fiecare 30 secunde
    if (currentMillis - lastApiCheck > 30000) {
      HTTPClient http;
      http.begin(apiUrl);

      int httpCode = http.GET();
      if (httpCode == 200) {
        String payload = http.getString();
        Serial.println(payload);
        // afisare ”PAY” cu albastru atunci cand se cere API la 30 de secunde
        tft.setCursor(85, 20);
        tft.setTextSize(3);
        tft.setTextColor(GC9A01A_BLUE);
        tft.println("PAY!");
        delay(2000);

        StaticJsonDocument<512> doc;
        if (deserializeJson(doc, payload) == DeserializationError::Ok) {
          float balance = doc["result"]["balance"].as<float>();

          tft.fillScreen(GC9A01A_BLACK);
          desenCercMargine(); // redesenăm cercul
          tft.setCursor(50, 55);
          tft.setTextSize(2);
          tft.setTextColor(GC9A01A_WHITE);
          tft.println("DUCO Wallet:");
          tft.setCursor(35, 100);
          tft.setTextSize(3);
          tft.setTextColor(GC9A01A_GREEN);
          tft.println(balance, 4);

          afiseazaCeas();
          desenButon(); // redesenează butonul
        }
      } else {
        Serial.println("Eroare API DuinoCoin");
        tft.fillScreen(GC9A01A_BLACK);
        desenCercMargine(); // redesenăm cercul
        tft.setCursor(45, 100);
        tft.setTextSize(2);
        tft.setTextColor(GC9A01A_RED);
        tft.println("Eroare API !!!");
        desenButon(); // redesenează butonul
      }
      http.end();
      lastApiCheck = currentMillis;
    }
  } else {
    // Mod offline: doar ceas
    tft.fillScreen(GC9A01A_BLACK);
    desenCercMargine(); // redesenăm cercul
    tft.setTextColor(GC9A01A_RED);
    tft.setCursor(50, 100);
    tft.setTextSize(2);
    tft.println("No WiFi...");
    afiseazaCeas();
    desenButon();
    delay(1000);
  }

  // actualizare ceas la fiecare secundă
  static unsigned long lastClockUpdate = 0;
  if (currentMillis - lastClockUpdate > 1000) {
    afiseazaCeas();
    desenButon();
    lastClockUpdate = currentMillis;
  }

  // verificăm butonul tactil
  verificaTouch();
}


