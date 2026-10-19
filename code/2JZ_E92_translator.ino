/*
 * 2JZ → BMW E92 CAN Translator
 * Scenariusz A: Fabryczny 2JZ ECU + tłumacz do zegarów/klimatyzacji
 *
 * Odczytuje sygnały analogowe z fabrycznego ECU 2JZ (RPM, prędkość, temperatura)
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

// ─── Konfiguracja pinów ────────────────────────────────────────────
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

// ─── Setup ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }  // Czekaj na Serial (native USB)

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
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();

  // Filtruj tylko potrzebne ID z Toyota (opcjonalnie)
  // mcp2515.setFilterMask(0, true, 0x7FF);
  // mcp2515.setFilter(0, true, TOYOTA_ID_RPM);

  Serial.println(F("[OK] MCP2515 zainicjalizowany (500 kbps)"));
  Serial.println(F("[OK] Nasłuchiwanie magistraly 2JZ..."));
  Serial.println(F("[OK] Wysyłanie do magistraly BMW E92..."));
  Serial.println();
}

// ─── Loop ──────────────────────────────────────────────────────────
void loop() {
  // Odbierz ramkę CAN z magistraly 2JZ
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
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
}

// ─── Przetwarzanie przychodzących ramek ──────────────────────────
void processIncomingFrame() {
  switch (canMsg.can_id) {
    case TOYOTA_ID_RPM:
      // RPM z Toyota 2JZ — parsowanie zależy od formatu ECU
      // Wzór: RPM = (B2*256+B3) / 6.4  (dla Speeduino)
      // Dla fabrycznego ECU Toyota format może być inny — dostosuj!
      rpm_2jz = (canMsg.data[2] << 8) | canMsg.data[3];
      // Konwersja do formatu BMW (jeśli ECU Toyota użyje innej skali)
      rpm_2jz = rpm_2jz * 10 / 64;  // Odwrócenie skali BMW
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
  // Wyślij RPM
  if (rpmUpdated) {
    sendRPM();
    rpmUpdated = false;
  }

  // Wyślij prędkość
  if (speedUpdated) {
    sendSpeed();
    speedUpdated = false;
  }

  // Wyślij temperaturę
  if (tempUpdated) {
    sendCoolantTemp();
    tempUpdated = false;
  }
}

// ─── Ramka RPM (0x316 — DME1) ─────────────────────────────────────
void sendRPM() {
  uint16_t temp_RPM = rpm_2jz * 64UL / 10U;  // RPM * 6.4

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

// ─── Ramka prędkości (0x1A0 — Speed) ──────────────────────────────
void sendSpeed() {
  uint16_t temp_speed = speed_2jz_kmh * 100;  // km/h * 100

  outMsg.can_id  = BMW_ID_SPEED;
  outMsg.can_dlc = 8;
  outMsg.data[0] = lowByte(temp_speed);        // Speed low byte
  outMsg.data[1] = highByte(temp_speed);       // Speed high byte
  outMsg.data[2] = 0x00;
  outMsg.data[3] = 0x00;
  outMsg.data[4] = 0x00;
  outMsg.data[5] = 0x00;
  outMsg.data[6] = 0x00;
  outMsg.data[7] = 0x00;

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
  // Konwersja: ((celsius + 48) * 4) / 3
  uint8_t temp_coolant = ((coolant_temp_c + 48) * 4) / 3;

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

// ─── Debug info ───────────────────────────────────────────────────
void printDebugInfo() {
  Serial.println(F("───── Status ─────"));
  Serial.print(F("RPM: ")); Serial.println(rpm_2jz);
  Serial.print(F("Speed: ")); Serial.print(speed_2jz_kmh); Serial.println(F(" km/h"));
  Serial.print(F("Coolant: ")); Serial.print(coolant_temp_c); Serial.println(F(" °C"));
  Serial.print(F("CAN rx: "));
  Serial.println(mcp2515.getCanCtrlInterruptFlags() ? F("OK") : F("ERR"));
  Serial.println();
}
