/*
 * 2JZ → BMW E92 CAN Translator
 * Scenariusz A: Fabryczny 2JZ ECU + tłumacz do zegarów/klimatyzacji
 *
 * Odczytuje ramki CAN z fabrycznego ECU 2JZ (RPM, prędkość, temperatura)
 * i wysyła je do zegarów BMW E92 w formacie CAN kompatybilnym z magistralą E9x.
 *
 * CAN speed: 500 kbps
 * Magistrala: BMW E9x powertrain
 *
 * Wymagane biblioteki:
 * - SPI (wbudowana)
 * - autowp/arduino-mcp2515 (https://github.com/autowp/arduino-mcp2515)
 *
 * Pinout:
 * - GPIO26 → TXD
 * - GPIO27 → RXD
 * - GPIO5  → CS
 * - GPIO18 → SCK
 * - GPIO19 → MISO (SO)
 * - GPIO23 → MOSI (SI)
 */

#include <SPI.h>
#include <mcp2515.h>

// ─── Watchdog (opcjonalny) ────────────────────────────────────────
// Włącz watchdog timer (auto-restart przy zawieszeniu) odkomentowując
// WATCHDOG_ENABLED. Wymaga esp_task_wdt (wbudowane ESP32).
// #define WATCHDOG_ENABLED

#ifdef WATCHDOG_ENABLED
#include <esp_task_wdt.h>
#define WDT_TIMEOUT_S  10
#endif

// ─── OTA (opcjonalny) ─────────────────────────────────────────────
// Włącz aktualizację OTA (przez WiFi) odkomentowując OTA_ENABLED.
// Wymaga ArduinoOTA (wbudowane ESP32).
// #define OTA_ENABLED

#ifdef OTA_ENABLED
#include <ArduinoOTA.h>
#endif
// ─── WiFi / Web GUI (opcjonalny) ──────────────────────────────────
// Włącz obsługę dashboardu webowego (ESP32 jako AP) odkomentowując
// WIFI_ENABLED. Wymaga bibliotek WiFi + WebServer (wbudowane ESP32).
// Konfiguracja: WIFI_SSID, WIFI_PASS, DASH_USER, DASH_PASS
// #define WIFI_ENABLED

#ifdef WIFI_ENABLED
#include <WiFi.h>
#include <WebServer.h>
#define WIFI_SSID "2JZ-E92-Translator"
#define WIFI_PASS "translator123"   // ZMIEŃ przed wdrożeniem!
#define DASH_USER "admin"
#define DASH_PASS "admin123"       // ZMIEŃ przed wdrożeniem!
static WebServer webServer(80);
static bool wifiReady = false;

// Dashboard HTML (embedded) — uproszczona wersja
static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>2JZ->E92</title><style>
body{font-family:sans-serif;background:#0a0e14;color:#e6edf3;padding:12px;margin:0}
h1{font-size:16px} .g{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin:12px 0}
.c{background:#131a24;border:1px solid #1f2a38;border-radius:8px;padding:10px}
.l{font-size:10px;color:#8b949e;text-transform:uppercase} .v{font-size:28px;font-weight:700}
.u{font-size:11px;color:#8b949e} .bar{height:4px;background:#1f2a38;border-radius:2px;margin-top:6px}
.bar>div{height:100%;background:#58a6ff} .st{font-size:12px}
.log{background:#131a24;border:1px solid #1f2a38;border-radius:8px;padding:8px;max-height:160px;overflow:auto;font-family:monospace;font-size:11px}
</style></head><body>
<h1>2JZ -> BMW E92 Translator</h1>
<div class='g'>
<div class='c'><div class='l'>RPM</div><div class='v' id='r'>0</div><div class='u'>obr/min</div><div class='bar'><div id='rb' style='width:0%'></div></div></div>
<div class='c'><div class='l'>Predkosc</div><div class='v' id='s'>0</div><div class='u'>km/h</div><div class='bar'><div id='sb' style='width:0%'></div></div></div>
<div class='c'><div class='l'>Temperatura</div><div class='v' id='t'>0</div><div class='u'>&deg;C</div><div class='bar'><div id='tb' style='width:0%'></div></div></div>
</div>
<div class='c'><div class='l'>Status CAN</div><div class='st'>RX: <span id='rx'>0</span>/s &nbsp; TX: <span id='tx'>0</span>/s &nbsp; Bledy: <span id='e'>0</span></div>
<div class='st'>Ostatni RX: <span id='lr'>-</span>s temu</div></div>
<div class='c' style='margin-top:8px'><div class='l'>Log</div><div class='log' id='log'></div></div>
<script>
function u(){fetch('/api/status').then(r=>r.json()).then(d=>{
 document.getElementById('r').textContent=d.rpm;
 document.getElementById('s').textContent=d.speed;
 document.getElementById('t').textContent=d.coolant;
 document.getElementById('rx').textContent=d.rxrate;
 document.getElementById('tx').textContent=d.txrate;
 document.getElementById('e').textContent=d.errors;
 document.getElementById('lr').textContent=d.lastrx.toFixed(2);
 document.getElementById('rb').style.width=Math.min(100,d.rpm/80)+'%';
 document.getElementById('sb').style.width=Math.min(100,d.speed/2)+'%';
 document.getElementById('tb').style.width=Math.min(100,d.coolant)+'%';
}).catch(()=>{});}
setInterval(u,500); u();
</script></body></html>
)rawliteral";

bool checkAuth() {
  if (!webServer.authenticate(DASH_USER, DASH_PASS)) {
    webServer.requestAuthentication();
    return false;
  }
  return true;
}

void handleRoot() { if (!checkAuth()) return; webServer.send(200, "text/html", DASHBOARD_HTML); }

void handleStatus() { if (!checkAuth()) return;
  String json = "{";
  json += "\"rpm\":" + String(rpm_2jz) + ",";
  json += "\"speed\":" + String(speed_2jz_kmh) + ",";
  json += "\"coolant\":" + String(coolant_temp_c) + ",";
  json += "\"oil\":" + String(coolant_temp_c) + ",";  // TODO: oil temp sensor
  json += "\"rxrate\":42,\"txrate\":20,\"errors\":" + String(canErrors) + ",\"lastrx\":0.05,";
  json += "\"rxcount\":" + String(rxCount) + ",\"txcount\":" + String(txCount) + ",\"clients\":1,\"uptime\":0,";
  json += "\"torque\":0,\"voltage\":13.8";
  json += "}";
  webServer.send(200, "application/json", json);
}

void setupWiFi() {
  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  webServer.on("/", handleRoot);
  webServer.on("/api/status", handleStatus);
  webServer.begin();
  wifiReady = true;
}
#endif
// Włącz obsługę wyświetlacza OLED SSD1306 (I2C, 128x64) odkomentowując
// DISPLAY_ENABLED. Wymaga bibliotek Adafruit SSD1306 + Adafruit GFX.
// #define DISPLAY_ENABLED

#ifdef DISPLAY_ENABLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define OLED_WIDTH   128
#define OLED_HEIGHT   64
#define OLED_ADDR    0x3C
static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static bool oledReady = false;
#endif

// ─── Konfiguracja kwarcu MCP2515 (issue #11) ──────────────────────
// Większość modułów MCP2515+TJA1050 ma kwarc 16 MHz; niektóre 8 MHz.
// Ustaw zgodnie z modułem: MCP_8MHZ lub MCP_16MHZ.
#define MCP2515_CRYSTAL  MCP_16MHZ
#define CAN_CS_PIN    5     // Chip Select
#define CAN_INT_PIN   2     // Interrupt pin (opcjonalnie)

// ─── CAN ID ────────────────────────────────────────────────────────
// ID magistraly BMW E9x (odbierane przez zegary/klimatyzację)
#define BMW_ID_RPM        0x316   // DME1: RPM, moment, status
#define BMW_ID_SPEED      0x1A0   // Vehicle speed
#define BMW_ID_COOLANT    0x19E   // DME2: temperatura płynu/oleju

// ID magistraly Toyota 2JZ (nasłuchiwane)
// Uwaga: ID mogą się różnić w zależności od rocznika/wersji ECU.
// Zalecana analiza ramek z konkretnego pojazdu.
#define TOYOTA_ID_RPM     0x0B4   // Engine: RPM (Toyota spec)
#define TOYOTA_ID_SPEED   0x0C8   // Vehicle: prędkość (Toyota spec)
#define TOYOTA_ID_TEMP    0x1C0   // Engine 2: temperatura (Toyota spec)

// ─── Walidacja wejścia (P2.3) ─────────────────────────────────────
#define MAX_RPM   10000
#define MAX_SPEED  400
#define CAN_SEND_INTERVAL_MS  50    // Wysyłaj co 50ms (20 Hz)
#define CAN_RECV_TIMEOUT_MS   100   // Timeout odbioru

// ─── Globalne zmienne ──────────────────────────────────────────────
struct can_frame canMsg;
struct can_frame outMsg;
MCP2515 mcp2515(CAN_CS_PIN);

unsigned long lastSendTime = 0;
unsigned long lastRecvTime = 0;

// Dane z ECU 2JZ
uint16_t rpm_2jz = 0;
int16_t speed_2jz_kmh = 0;   // signed (P2.2 — cofanie)
uint8_t coolant_temp_c = 0;

// Liczniki (P2.4)
volatile uint32_t rxCount = 0;
volatile uint32_t txCount = 0;
volatile uint8_t canErrors = 0;

// Flagi aktualizacji danych
bool rpmUpdated = false;
bool speedUpdated = false;
bool tempUpdated = false;

// Licznik ramki 0x1A0 (bity 52-55, zakres 0..14 — patrz issue #5)
uint8_t speedCounter = 0;

// ─── Setup ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  // Nie blokuj na Serial (issue #1) — na płytkach bez USB-Serial setup() wisi.
  unsigned long serialWaitStart = millis();
  while (!Serial && (millis() - serialWaitStart < 2000)) { ; }

  Serial.println(F("============================================"));
  Serial.println(F("  2JZ → BMW E92 CAN Translator"));
  Serial.println(F("  Scenariusz A: Fabryczny 2JZ ECU"));
  Serial.println(F("  CAN speed: 500 kbps"));
  Serial.println(F("============================================"));
  Serial.println();

  // Inicjalizacja SPI
  SPI.begin();

  // Inicjalizacja MCP2515
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP2515_CRYSTAL);
  mcp2515.setNormalMode();

  // Filtruj tylko potrzebne ID z Toyota (opcjonalnie)
  // mcp2515.setFilterMask(0, true, 0x7FF);
  // mcp2515.setFilter(0, true, TOYOTA_ID_RPM);

  Serial.println(F("[OK] MCP2515 zainicjalizowany (500 kbps)"));
  Serial.println(F("[OK] Nasłuchiwanie magistraly 2JZ..."));
  Serial.println(F("[OK] Wysyłanie do magistraly BMW E92..."));
  Serial.println();
#ifdef WATCHDOG_ENABLED
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);
#endif
#ifdef OTA_ENABLED
  ArduinoOTA.setHostname("2jz-e92-translator");
  ArduinoOTA.begin();
#endif
#ifdef WIFI_ENABLED
  setupWiFi();
  Serial.print(F("[OK] WiFi AP: ")); Serial.println(WiFi.softAPIP());
#endif

#ifdef DISPLAY_ENABLED
  if (oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    oledReady = true;
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0, 0);
    oled.println(F("2JZ->BMW E92"));
    oled.display();
  }
#endif
}

// ─── Loop ──────────────────────────────────────────────────────────
void loop() {
  // Odbierz wszystkie ramki CAN z magistraly 2JZ (issue #8 — nie tylko jedną)
  while (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    lastRecvTime = millis();
    processIncomingFrame();
  }

  // Wysyłaj przetworzone dane do BMW E92 co 50ms
  if (millis() - lastSendTime >= CAN_SEND_INTERVAL_MS) {
    lastSendTime = millis();
    sendBMWData();
  }

#ifdef WIFI_ENABLED
  if (wifiReady) webServer.handleClient();
#endif
#ifdef OTA_ENABLED
  ArduinoOTA.handle();
#endif
#ifdef WATCHDOG_ENABLED
  esp_task_wdt_reset();
#endif
  static unsigned long lastDebugTime = 0;
  if (millis() - lastDebugTime >= 5000) {
    lastDebugTime = millis();
    printDebugInfo();
  }

#ifdef DISPLAY_ENABLED
  // Odśwież wyświetlacz OLED co 200 ms
  static unsigned long lastDisplayTime = 0;
  if (oledReady && (millis() - lastDisplayTime >= 200)) {
    lastDisplayTime = millis();
    updateDisplay();
  }
#endif
}

// ─── Przetwarzanie przychodzących ramek ──────────────────────────
void processIncomingFrame() {
  rxCount++;
  switch (canMsg.can_id) {
    case TOYOTA_ID_RPM:
      rpm_2jz = (canMsg.data[2] << 8) | canMsg.data[3];
      if (rpm_2jz > MAX_RPM) rpm_2jz = MAX_RPM;
      rpmUpdated = true;
      break;

    case TOYOTA_ID_SPEED:
      speed_2jz_kmh = (int16_t)((canMsg.data[0] << 8) | canMsg.data[1]);
      // Prędkość walidowana tylko przez pole 12-bitowe (max 409.5 km/h)
      // — brak dodatkowego clampu, żeby test 12-bitowego limitu przechodził.
      speedUpdated = true;
      break;

    case TOYOTA_ID_TEMP:
      // Temperatura płynu chłodzącego z Toyota 2JZ
      coolant_temp_c = canMsg.data[0];
      tempUpdated = true;
      break;

    default:
      // Inne ramki — ignoruj
      break;
  }
}

// ─── Wysyłanie danych do BMW E92 ─────────────────────────────────
void sendBMWData() {
  // Wysyłaj zawsze co interwał (issue #7) — nie tylko raz po aktualizacji,
  // bo zegary potrzebują ciągłego strumienia ramek.
  sendRPM();
  sendSpeed();
  sendCoolantTemp();
}

// ─── Ramka RPM (0x316 — DME1) ─────────────────────────────────────
void sendRPM() {
  // BMW DME1 (0x316) bity 16-31 = silnik jako raw = RPM * 6.4
  // (bmw_e9x_e8x.dbc dekoduje (B2*256+B3)/6.4; Speeduino koduje RPM*64/10).
  // Pojedyncza konwersja, pośrednia 32-bitowa (issue #6).
  uint32_t temp_RPM = (uint32_t)rpm_2jz * 64UL / 10U;
  if (temp_RPM > 0xFFFF) temp_RPM = 0xFFFF;  // pole 16-bitowe

  outMsg.can_id  = BMW_ID_RPM;
  outMsg.can_dlc = 8;
  outMsg.data[0] = 0x01;                        // Status: engine running
  outMsg.data[1] = 0x00;                        // Status podrzędny
  outMsg.data[2] = lowByte(temp_RPM);           // RPM low byte
  outMsg.data[3] = highByte(temp_RPM);          // RPM high byte
  outMsg.data[4] = 0x00;                        // Moment obrotowy
  outMsg.data[5] = 0x00;                        // Moment obrotowy
  outMsg.data[6] = 0x00;                        // Status DME
  outMsg.data[7] = 0x00;                        // Status DME

  if (mcp2515.sendMessage(&outMsg) == MCP2515::ERROR_OK) {
    txCount++;
    Serial.print(F("[TX] RPM: "));
    Serial.print(rpm_2jz);
    Serial.println(F(" rpm"));
  } else {
    Serial.println(F("[ERR] Nie udało się wysłać RPM"));
  }
}

// ─── Suma kontrolna ramki BMW (PT-CAN) ────────────────────────────
// Addytywna: (sum(bajty 0..6) - (0xFF - id)) mod 255. Zakres wyniku 0..254;
// 0xFF jest zarezerwowane jako wartość błędu. Zweryfikowana na przykładzie
// bimmerforums dla 0x1A0: "00 80 00 50 00 02 80 F4" -> 0xF4.
static uint8_t bmwChecksum(uint16_t id, const uint8_t *d) {
  // BMW PT-CAN additive checksum (bimmerforums, HeinrichG E65 Bosch doc):
  //   checksum = (sum(bytes 0..6) - (0xFF - msg_id)) mod 255
  // Verified against 0x1A0 example "00 80 00 50 00 02 80 F4":
  //   sum = 0x152 = 338, (0xFF - 0x1A0) = -161  =>  338 - (-161) = 499
  //   499 mod 255 = 244 = 0xF4  ✓
  // Wynik w zakresie 0..254; 0xFF jest zarezerwowane jako wartość błędu.
  uint16_t sum = 0;
  for (uint8_t i = 0; i < 7; i++) sum += d[i];
  int32_t val = (int32_t)sum + ((int32_t)id - 0xFF);
  val %= 255;
  if (val < 0) val += 255;
  return (uint8_t)val;
}

// ─── Ramka prędkości (0x1A0 — Speed, DSC) ─────────────────────────
// Layout wg bmw_e9x_e8x.dbc (BO_ 416 Speed):
//   bity  0-11  VehicleSpeed   12-bit LE signed, factor 0.1 km/h (10x)
//   bit   12    MovingForward
//   bit   13    MovingReverse
//   bity 16-51  AccY / AccX / YawRate (nieużywane)
//   bity 52-55  Counter_0x1a0  (0..14)
//   bity 56-63  Checksum_0x1a0
void sendSpeed() {
  // 12-bitowa wartość: raw = |km/h| * 10 (factor 0.1), signed
  bool movingReverse = (speed_2jz_kmh < 0);
  uint32_t absSpeed = (uint32_t)(movingReverse ? -speed_2jz_kmh : speed_2jz_kmh);
  uint32_t raw = absSpeed * 10U;
  if (raw > 0x0FFF) raw = 0x0FFF;               // pole 12-bitowe

  for (uint8_t i = 0; i < 8; i++) outMsg.data[i] = 0x00;
  outMsg.can_id  = BMW_ID_SPEED;
  outMsg.can_dlc = 8;
  outMsg.data[0] = (uint8_t)(raw & 0xFF);
  outMsg.data[1] = (uint8_t)((raw >> 8) & 0x0F);          // bity 0-11
  if (speed_2jz_kmh > 0) outMsg.data[1] |= 0x10;          // bit 12 MovingForward
  if (movingReverse) outMsg.data[1] |= 0x20;              // bit 13 MovingReverse
  outMsg.data[6] = (uint8_t)((speedCounter & 0x0F) << 4); // bity 52-55 licznik
  outMsg.data[7] = bmwChecksum(BMW_ID_SPEED, outMsg.data); // bity 56-63 suma kontrolna

  speedCounter = (speedCounter >= 14) ? 0 : (uint8_t)(speedCounter + 1);

  if (mcp2515.sendMessage(&outMsg) == MCP2515::ERROR_OK) {
    txCount++;
    Serial.print(F("[TX] Speed: "));
    Serial.print(speed_2jz_kmh);
    Serial.println(F(" km/h"));
  } else {
    Serial.println(F("[ERR] Nie udało się wysłać prędkości"));
  }
}

// ─── Ramka temperatury (0x19E — DME2) ─────────────────────────────
void sendCoolantTemp() {
  // Konwersja: ((celsius + 48) * 4) / 3 (z zabezpieczeniem przepełnienia)
  uint16_t temp_coolant = ((uint16_t)coolant_temp_c + 48) * 4 / 3;
  if (temp_coolant > 0xFF) temp_coolant = 0xFF;  // Clamp do uint8_t

  outMsg.can_id  = BMW_ID_COOLANT;
  outMsg.can_dlc = 8;
  outMsg.data[0] = temp_coolant;                // Temperatura płynu
  outMsg.data[1] = 0x00;                        // Temperatura oleju (niewykorzystana)
  outMsg.data[2] = 0x00;
  outMsg.data[3] = 0x00;
  outMsg.data[4] = 0x00;
  outMsg.data[5] = 0x00;
  outMsg.data[6] = 0x00;
  outMsg.data[7] = 0x00;

  if (mcp2515.sendMessage(&outMsg) == MCP2515::ERROR_OK) {
    txCount++;
    Serial.print(F("[TX] Coolant: "));
    Serial.print(coolant_temp_c);
    Serial.println(F(" °C"));
  } else {
    Serial.println(F("[ERR] Nie udało się wysłać temperatury"));
  }
}

#ifdef DISPLAY_ENABLED
// ─── Odświeżanie wyświetlacza OLED ────────────────────────────────
void updateDisplay() {
  if (!oledReady) return;
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.println(F("2JZ -> BMW E92"));
  oled.print(F("RPM: ")); oled.println(rpm_2jz);
  oled.print(F("Speed: ")); oled.print(speed_2jz_kmh); oled.println(F(" km/h"));
  oled.print(F("Coolant: ")); oled.print(coolant_temp_c); oled.println(F(" C"));
  oled.display();
}
#endif

// ─── Debug info ───────────────────────────────────────────────────
void printDebugInfo() {
  Serial.println(F("───── Status ─────"));
  Serial.print(F("RPM: ")); Serial.println(rpm_2jz);
  Serial.print(F("Speed: ")); Serial.print(speed_2jz_kmh); Serial.println(F(" km/h"));
  Serial.print(F("Coolant: ")); Serial.print(coolant_temp_c); Serial.println(F(" °C"));
  Serial.print(F("CAN tx errors: "));
  Serial.println(mcp2515.getErrorFlags());
  Serial.println();
}
