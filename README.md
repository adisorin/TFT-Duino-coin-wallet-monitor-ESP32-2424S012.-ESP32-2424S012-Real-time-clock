


## 🎤 **Prezentare publică – Proiect: Display TFT cu Duino-Coin Balance și Ceas NTP**

### 🧩 1. Introducere

Bună ziua!
Astăzi vă voi prezenta un proiect realizat cu microcontrolerul **ESP32**, care afișează în timp real:

* soldul unui cont **Duino-Coin**,
* ora locală sincronizată automat prin **NTP (Network Time Protocol)**,
* și include un **buton tactil** pe ecranul TFT pentru interacțiune.

Sistemul are și funcționalitate offline — dacă nu există conexiune WiFi, ceasul continuă să funcționeze local, iar interfața rămâne activă.

---

### ⚙️ 2. Componente hardware utilizate

* **ESP32 / placa compatibilă**
* **Display TFT circular GC9A01A** (controlat prin SPI)
* **Touchscreen capacitiv CST816S** (prin I²C)
* **Conexiune WiFi**
* **Cont Duino-Coin** pentru acces la API-ul de balanță

---

### 🔌 3. Biblioteci incluse

Codul folosește mai multe biblioteci importante:

```cpp
#include <WiFi.h>          // Conectarea la rețea
#include <WiFiMulti.h>     // Conectare la mai multe SSID-uri
#include <HTTPClient.h>    // Cereri HTTP către server
#include <ArduinoJson.h>   // Parsarea datelor JSON de la API DuinoCoin
#include <Adafruit_GFX.h>  
#include <Adafruit_GC9A01A.h> // Controlul ecranului TFT
#include <time.h>          // Obținerea orei prin NTP
#include <Wire.h>          // Comunicare I2C pentru touchscreen
```

---

### 🌐 4. Conectarea la WiFi și sincronizarea orei

Programul adaugă mai multe rețele WiFi folosind `WiFiMulti`, pentru a se conecta automat la cea mai puternică:

```cpp
wifiMulti.addAP("SSID1", "password1");
wifiMulti.addAP("SSID2", "password2");
```

După conectare, fusul orar este setat automat pentru **România (EET/EEST)**, iar ora este sincronizată cu servere NTP:

```cpp
configTime(1 * 3600, 3600, "pool.ntp.org", "time.nist.gov");
```

Dacă nu există internet, ecranul afișează mesajul **“No WiFi...”**, dar ceasul continuă local pe baza ultimului timp sincronizat.

---

### 💰 5. Interfața Duino-Coin – afișarea balanței

La fiecare **30 de secunde**, dispozitivul trimite o cerere către API-ul oficial Duino-Coin:

```cpp
https://server.duinocoin.com/balances/USERNAME
```

Se primește un răspuns în format JSON care conține balanța curentă a utilizatorului:

```json
{
  "result": {
    "balance": 12.3456
  }
}
```

Aceste date sunt parsate și afișate pe ecran:

* cu text alb „DUCO Wallet”
* și valoarea soldului cu verde.

Dacă cererea eșuează, se afișează mesajul **“Eroare API !!!”** pe roșu.

---

### ⏰ 6. Ceas digital cu actualizare automată

Ceasul se actualizează:

* la fiecare secundă local (calculat din ultimul timp sincronizat);
* și la fiecare minut, se face o sincronizare NTP reală.

Timpul este afișat pe ecran mare, în galben:

```
HH:MM:SS
```

Dacă nu s-a reușit sincronizarea, apare mesajul **“No Time Sync!”**.

---

### 🖲️ 7. Touchscreen și butonul tactil

Touchscreen-ul CST816S permite detectarea atingerilor.
Pe ecran există un buton tactil care:

* apare cu textul **“PAY-15’”** (albastru);
* la atingere, se schimbă în **“GiftPay”** (verde) pentru 15 minute;
* după ce timpul expiră, reapare automat.

Funcția principală de verificare este:

```cpp
verificaTouch();
```

Aceasta citește coordonatele de pe touchscreen și compară cu zona butonului.

---

### 🔄 8. Actualizări și timere

Proiectul folosește mai multe cronometre interne (`millis()`):

* 10 secunde – verificare reconectare WiFi
* 30 secunde – actualizare API Duino-Coin
* 60 secunde – sincronizare NTP
* 15 minute – reapariția butonului tactil

Acest sistem de **timing asincron** permite rularea fără blocaje și fără `delay()` inutile.

---

### 🧠 9. Funcționare generală – flux logic

**1️⃣ Setup:**

* inițializează ecranul, touch-ul și WiFi-ul;
* sincronizează ora;
* afișează starea inițială.

**2️⃣ Loop:**

* actualizează ceasul în timp real;
* face cereri periodice la API;
* verifică WiFi și butonul tactil;
* reafișează interfața după fiecare actualizare.

---

### 🎨 10. Interfața grafică

Ecranul TFT este încadrat de un cerc albastru (funcția `desenCercMargine()`), iar textul este colorat diferit pentru claritate:

* **Verde** – conexiune reușită / balanță
* **Roșu** – eroare / lipsă conexiune
* **Albastru** – mesaj de actualizare „PAY!”
* **Galben** – afișarea timpului

---

### 💡 11. Concluzii și posibile extinderi

Acest proiect demonstrează:

* integrarea unei interfețe grafice circulare cu date online;
* utilizarea senzorului tactil pentru interactivitate;
* și gestionarea inteligentă a conexiunilor și timpului.

**Posibile îmbunătățiri:**

* adăugarea unui meniu interactiv complet pe touch;
* integrare cu portofele multiple Duino-Coin;
* salvarea datelor local pe SPIFFS sau SD card;
* afișarea unui grafic istoric al balanței.

---

### 🧾 12. Mesaj final

> “Un mic ecran care spune multe: ora exactă, starea rețelei și valoarea muncii tale digitale – totul într-un singur dispozitiv inteligent!”

---

Vrei să ți-o transform într-o **prezentare PowerPoint / PDF cu slide-uri** (cu titluri, imagini și text pentru vorbit)?
Pot genera automat fișierul și conținutul grafic (inclusiv imagini reprezentative cu ecranul TFT și diagrame de conexiuni).

![547219393_24601542792869844_709400794998571944_n](https://github.com/user-attachments/assets/b411efea-400a-4813-aef5-d62fa5a19a30)
![550724234_24601536986203758_6425173906330713527_n](https://github.com/user-attachments/assets/f1413db3-2559-4fe4-84c4-0ea897fd72a2)
![500132910-c25d793a-9292-432a-bd15-ab1f75898c87](https://github.com/user-attachments/assets/255fb637-58f4-4f71-8605-e386fb775e1b)
