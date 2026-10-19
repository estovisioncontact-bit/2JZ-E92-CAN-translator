# 2JZ → BMW E92 CAN Translator

## Scenariusz A: Fabryczny 2JZ ECU + tłumacz do zegarów/klimatyzacji

### Cel projektu

Tłumacz CAN (Controller Area Network) odczytuje sygnały analogowe z fabrycznego ECU silnika Toyota 2JZ (RPM, prędkość pojazdu, temperatura płynu chłodzącego) i przetwarza je na komunikaty CAN kompatybilne z magistralą BMW E92. Przetworzone dane są wysyłane do zegarów i systemu klimatyzacji BMW.

**Kluczowa zasada:** Fabryczny ECU 2JZ pozostaje nietknięty — nadal steruje silnikiem jak dotychczas. Tłumacz jedynie nasłuchuje (sniffruje) magistralę CAN i generuje nowe komunikaty dla peryferiów BMW. Systemy ABS/ESP działajają normalnie, ponieważ nie są modyfikowane.

### Dlaczego?

Wymiana silnika 2JZ do BMW E92 (lub odwrotnie) wymaga adaptera, który "przetłumaczy" sygnały z jednego systemu CAN na drugi. Fabryczne ECU Toyota nie rozumie magistry BMW, a zegary BMW oczekują specyficznych ramek CAN. Ten tłumacz rozwiązuje ten problem.

### Architektura systemu

```
┌─────────────────────┐     CAN Bus      ┌─────────────────────┐     CAN Bus      ┌─────────────────────┐
│   Fabryczny 2JZ ECU │◄────────────────►│  2JZ→E92 Translator  │◄────────────────►│  BMW E92 Cluster    │
│   (Toyota)          │   (magistrala     │  (ESP32 + MCP2515)   │   (magistrala     │  + A/C system       │
│                     │    silnikowa)     │                      │    BMW E9x)       │                     │
└─────────────────────┘                   └─────────────────────┘                   └─────────────────────┘
         ▲                                           ▲                                        ▲
         │                                           │                                        │
    Sygnały analogowe:                        Nasłuchuje CAN:                          Odbiera CAN:
    - RPM                                     - ID 0x316 (DME1)                        - ID 0x316 (RPM)
    - Prędkość                                 - ID 0x1A0 (Speed)                       - ID 0x1A0 (Speed)
    - Temperatura                             - Inne ID relevantne                    - Inne ID relevantne
                                              │
                                              ▼
                                         Generuje CAN:
                                         - ID 0x316 (RPM)
                                         - ID 0x1A0 (Speed)
                                         - Inne ID zegarów/klimatyzacji
```

### Komponenty sprzętowe (BOM)

| Komponent | Cena orientacyjna | Uwagi |
|-----------|-------------------|-------|
| ESP32 DevKitC (38-pin) | ~35-45 zł | Kontroler główny, obsługa CAN przez SPI |
| Moduł CAN MCP2515 + TJA1050 | ~10-15 zł | Kontroler CAN SPI + transceiver na jednej płytce |
| Rezystor 120Ω | ~2 zł | Tylko jeśli tworzymy nową magistralę |
| Przewody breadboard / goldpin | ~5-10 zł | Połączenia prototypowe |
| Obudowa / płytka prototypowa | ~5-10 zł | Opcjonalnie |
| Zasilacz USB 5V lub step-down 12V→5V | ~10-15 zł | Zasilanie z pojazdu lub USB |
| **RAZEM** | **~67-97 zł** | |

### Podłączenie (Wiring)

#### Pinout ESP32 DevKitC ↔ MCP2515/TJA1050

```
ESP32 DevKitC          MCP2515/TJA1050       BMW E92 CAN Bus
─────────────          ─────────────────       ─────────────────
GPIO26 (TX)      →      TXD
GPIO27 (RX)      →      RXD
GPIO5 (CS)       →      CS
GPIO18 (SCK)     →      SCK
GPIO19 (MISO)    →      SO
GPIO23 (MOSI)    →      SI
GND              →      GND
                        CANH ─────────────→ CAN High (pin 6 OBD-II)
                        CANL ─────────────→ CAN Low (pin 14 OBD-II)
```

#### CAN Speed

Magistrala CAN w BMW E9x działa z prędkością **500 kbps** (500 000 bitów/s).

#### OBD-II Pinout

- **Pin 6** = CAN High (CANH)
- **Pin 14** = CAN Low (CANL)

#### Uwagi techniczne

1. **TJA1050 a logika 5V/3.3V:** TJA1050 to układ logiki 5V — wyjście RXD daje 5V. ESP32 działa na 3.3V, ale większość pinów GPIO jest tolerancyjnych na 5V (sprawdź specyfikację swojego modułu ESP32 DevKitC). W razie wątpliwości użyj level shiftera 5V→3.3V na linii RXD.

2. **Oznaczenie pinów MCP2515:** Piny SPI mogą być oznaczone różnie w zależności od producenta modułu:
   - SCK = CLK
   - MOSI = DI lub SI
   - MISO = DO lub SO
   - Sprawdź silkscreen na swoim module!

3. **Terminacja 120Ω:** W autach zazwyczaj są już terminatory CAN na obu końcach magistrali. Dodawaj rezystor 120Ω **tylko** jeśli tworzymz nową magistralę od zera. Sprawdź opór między CANH a CANL — powinien wynosić ~60Ω (dwa rezystory 120Ω równolegle).

4. **Zasilanie:** ESP32 DevKitC może być zasilany z portu USB (5V) lub z pinu VIN/5V. W samochodzie zalecany jest konwerter 12V→5V (step-down).

### Kod źródłowy

Kod ESP32 znajduje się w katalogu [`code/`](code/). Plik główny: [`2JZ_E92_translator.ino`](code/2JZ_E92_translator.ino).

#### Wymagane biblioteki Arduino

- **SPI** (wbudowana)
- **WiFi** (wbudowana — opcjonalnie)
- Biblioteka MCP2515 (np. autowp/arduino-mcp2515 lub podobna)

#### Konfiguracja w Arduino IDE

1. Pobierz i zainstaluj [Arduino IDE](https://www.arduino.cc/en/software)
2. Dodaj obsługę ESP32: `Plik → Preferencje → Dodatkowe adresy URL menedżera płytek:`
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Zainstaluj płytkę: `Narzędzia → Płytka → Menedżer płytek → esp32`
4. Zainstaluj bibliotekę MCP2515: `Szkic → Dołącz bibliotekę → Zarządzaj bibliotekami → MCP2515`
5. Wybierz płytkę: `Narzędzia → Płytka → ESP32 Dev Module`
6. Port COM: wybierz odpowiedni port USB
7. Załaduj szkic

#### Algorytm działania

```
1. Inicjalizacja SPI + MCP2515 (500 kbps)
2. Główna pętla:
   a. Odbiór ramki CAN z magistraly 2JZ (nasłuch)
   b. Parsowanie danych (RPM, prędkość, temperatura)
   c. Konwersja do formatu BMW E9x
   d. Wysłanie ramek CAN do magistraly BMW
3. Opcjonalnie: debug przez Serial Monitor
```

### Konwersja danych

#### RPM (Revolutions Per Minute)

Sygnał RPM z ECU 2JZ jest przetwarzany do formatu BMW E9x:

```cpp
uint16_t temp_RPM = currentStatus.RPM * 64UL / 10U;  // RPM * 6.4
outMsg.buf[2] = lowByte(temp_RPM);
outMsg.buf[3] = highByte(temp_RPM);
```

**Wzór:** `RPM_BMW = RPM_2JZ * 6.4`

#### Prędkość pojazdu

Prędkość jest odczytywana z odpowiedniego sygnału ECU 2JZ i konwertowana do formatu CAN BMW E9x (km/h).

#### Temperatura płynu chłodzącego

Temperatura w stopniach Celsjusza jest konwertowana do formatu BMW E9x:

```cpp
uint8_t temp_coolant = ((celsius + 48) * 4) / 3;
```

**Wzór:** `TEMP_BMW = ((TEMP_C + 48) * 4) / 3`

### CAN ID dla BMW E9x

Szczegółowa tabela znajduje się w pliku [`can_ids.md`](can_ids.md).

### Projekt referencyjne

- **Speeduino** — otwarty ECU z gotowym kodem CAN dla BMW E9x: https://github.com/speeduino/speeduino/blob/master/speeduino/comms_CAN.cpp
- **opendbc** — baza danych CAN ID/signal dla BMW E9x: https://github.com/commaai/opendbc/blob/master/bmw_e9x_e8x.dbc
- **CarCluster** — ESP32 + MCP2515 → sterowanie zegarami: https://github.com/r00li/CarCluster
- **OpenECU** — protokół BMW E8x/E9x powertrain: https://oecua.org/protocols/bmw/bmw-e8x-e9x-powertrain

### Bezpieczeństwo i zalecenia

1. **Nie modyfikuj fabrycznego ECU 2JZ** — tylko nasłuchuj magistrali CAN
2. **Używaj izolacji galwanicznej** jeśli możliwe (opcjonalnie)
3. **Testuj na stacie** przed podłączeniem do pojazdu
4. **Sprawdź terminację CAN** przed uruchomieniem
5. **Zasilanie** — upewnij się, że zasilacz jest stabilny (12V→5V)
6. **Bezpieczeństwo pojazdu** — tłumacz nie wpływa na ABS/ESP, ale zawsze testuj bezpiecznie

### Struktura repozytorium

```
2JZ-E92-CAN-translator/
├── README.md                  # Ten plik — pełna dokumentacja
├── bom.md                     # Lista części z cenami
├── wiring.txt                 # Diagram podłączenia (tekstowy)
├── can_ids.md                 # Tabela CAN ID dla BMW E9x
├── references.md              # Linki do projektów referencyjnych
└── code/
    └── 2JZ_E92_translator.ino # Kod ESP32 (Arduino IDE)
```

### Licencja

Projekt objęty licencją MIT — możesz swobodnie używać, modyfikować i rozpowszechniać.

### Autor

Stworzono jako projekt open-source. Zgłaszaj issues i pull requesty na GitHubie.

---

**Zastrzeżenie:** Ten projekt ma charakter eksperymentalny. Używasz na własną odpowiedzialność. Autor nie ponosi odpowiedzialności za jakiekolwiek szkody wynikłe z użytkowania tego projektu. Zawsze testuj w bezpiecznych warunkach przed użyciem w pojeździe.
