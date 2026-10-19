# Projekty referencyjne

## Speeduino

**Link:** https://github.com/speeduino/speeduino/blob/master/speeduino/comms_CAN.cpp

Otwarty firmware ECU dla Arduino/ESP32. Zawiera gotowy kod komunikacji CAN dla BMW E9x — wzorce konwersji RPM, prędkości i temperatury.

## opendbc

**Link:** https://github.com/commaai/opendbc/blob/master/bmw_e9x_e8x.dbc

Baza danych CAN ID i sygnałów dla pojazdów BMW E8x/E9x (i innych marek). Kompatybilna z narzędziami comma.ai, savvcan i innymi analizatorami CAN.

## CarCluster

**Link:** https://github.com/r00li/CarCluster

Projekt ESP32 + MCP2515 do sterowania zegarami samochodowymi. Podobna architektura sprzętowa do tłumacza 2JZ→E92.

## OpenECU — protokół BMW E8x/E9x

**Link:** https://oecua.org/protocols/bmw/bmw-e8x-e9x-powertrain

Dokumentacja protokołu powertrain dla BMW E8x/E9x. Szczegółowe informacje o ramkach CAN, mapowaniu sygnałów i formatach danych.

## Inne przydatne projekty

### ESP32 CAN

- **ESP32-Arduino-CAN** — https://github.com/miwagner/ESP32-Arduino-CAN
- **arduino-mcp2515** — https://github.com/autowp/arduino-mcp2515

### Analiza CAN

- **SavvyCAN** — https://github.com/collin80/SavvyCAN
- **python-can** — https://github.com/hardbyte/python-can

### Dokumentacja protokołu BMW

- **BMW CAN Bus Protocol** — https://github.com/eerimoq/cantools
- **opendbc** — baza DBC dla wielu marek

## Przydatne linki

- **Arduino ESP32** — https://github.com/espressif/arduino-esp32
- **ESP32 Arduino Core** — https://docs.espressif.com/projects/arduino-esp32/
- **MCP2515 datasheet** — https://ww1.microchip.com/downloads/en/DeviceDoc/20001801H.pdf
- **TJA1050 datasheet** — https://www.nxp.com/docs/en/data-sheet/TJA1050.pdf

## Inspiracje

| Projekt | Opis | Różnice do naszego |
|---------|------|-------------------|
| Speeduino | Otwarty ECU | My nie zastępujemy ECU, tylko tłumaczymy CAN |
| CarCluster | ESP32 + MCP2515 → zegary | Podobna architektura, inne źródło danych |
| OpenECU | Dokumentacja protokołu | Niezastąpione źródło wiedzy o CAN BMW |
| opendbc | Baza DBC | Gotowe definicje sygnałów |

## Jak z tego korzystać

1. **Zacznij od opendbc** — znajdź definicje sygnałów BMW E9x
2. **Sprawdź Speeduino** — zobacz jak konwertuje dane
3. **CarCluster** — zobacz jak ESP32 + MCP2515 steruje zegarami
4. **OpenECU** — pogłęb wiedzę o protokole

---

**Uwaga:** Wszystkie projekty referencyjne są objęte własnymi licencjami open-source. Przed użyciem kodu sprawdź licencję danego projektu.
