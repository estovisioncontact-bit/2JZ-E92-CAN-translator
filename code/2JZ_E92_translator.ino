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

// ─── Wyświetlacz OLED (opcjonalny) ────────────────────────────────
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

// ─── Timing ────────────────────────────────────────────────────────
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
uint16_t speed_2jz_kmh = 0;
uint8_t coolant_temp_c = 0;

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

  // Opcjonalnie: debug — co 5 sekund
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
  switch (canMsg.can_id) {
    case TOYOTA_ID_RPM:
      // Silnik z ramki 2JZ. Wartość zapisywana BEZ ZMIAN i konwertowana
      // tylko raz, na miejscu transmisji (issue #6) — bez stratnej rundy
      // raw→rpm→raw, której pośrednie `raw * 10` przepełniało uint16_t
      // powyżej ~1048 rpm.
      // UWAGA: format ramki 2JZ jest niezweryfikowany (issue #15).
      rpm_2jz = (canMsg.data[2] << 8) | canMsg.data[3];
      rpmUpdated = true;
      break;

    case TOYOTA_ID_SPEED:
      // Prędkość z Toyota 2JZ
      speed_2jz_kmh = (canMsg.data[0] << 8) | canMsg.data[1];
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
  // 12-bitowa wartość: raw = km/h * 10 (factor 0.1) — NIE km/h * 100 (issue #5)
  // Źródło: HeinrichG E65 (Bosch doc) + bimmerforums: 50 km/h -> 500 -> 0x1F4.
  // Uwaga: bmw_e9x_e8x.dbc podaje factor 0.103 — to przybliżenie; realna
  // wartość to 0.1 (10x).
  uint32_t raw = (uint32_t)speed_2jz_kmh * 10U;
  if (raw > 0x0FFF) raw = 0x0FFF;               // pole 12-bitowe

  for (uint8_t i = 0; i < 8; i++) outMsg.data[i] = 0x00;
  outMsg.can_id  = BMW_ID_SPEED;
  outMsg.can_dlc = 8;
  outMsg.data[0] = (uint8_t)(raw & 0xFF);
  outMsg.data[1] = (uint8_t)((raw >> 8) & 0x0F);          // bity 0-11
  if (speed_2jz_kmh > 0) outMsg.data[1] |= 0x10;          // bit 12 MovingForward
  outMsg.data[6] = (uint8_t)((speedCounter & 0x0F) << 4); // bity 52-55 licznik
  outMsg.data[7] = bmwChecksum(BMW_ID_SPEED, outMsg.data); // bity 56-63 suma kontrolna

  speedCounter = (speedCounter >= 14) ? 0 : (uint8_t)(speedCounter + 1);

  if (mcp2515.sendMessage(&outMsg) == MCP2515::ERROR_OK) {
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
