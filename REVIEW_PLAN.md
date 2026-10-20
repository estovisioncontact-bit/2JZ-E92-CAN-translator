# Review i Plan Ulepszeń — 2JZ→E92 CAN Translator

## 1. Przegląd dotychczasowej pracy — błędy i ryzyka

### Błędy (potwierdzone)
| # | Błąd | Status |
|---|------|--------|
| B1 | RPM round-trip overflow (raw→rpm→raw przepełniał uint16_t >1048 rpm) | ✅ Naprawiony (#6) |
| B2 | Speed 100x za duża, 16-bit zamiast 12-bit, factor 0.103 | ✅ Naprawiony (#5) |
| B3 | Coolant overflow uint8_t (150°C→8 zamiast 255) | ✅ Naprawiony (#2) |
| B4 | getCanCtrlInterruptFlags() nie istnieje w bibliotece | ✅ Naprawiony (#9) |
| B5 | Serial hang bez USB | ✅ Naprawiony (#1) |
| B6 | TX tylko raz (flagi czyszczone) | ✅ Naprawiony (#7) |
| B7 | Loop drenuje 1 ramkę (przepełnienie bufora) | ✅ Naprawiony (#8) |
| B8 | Kwarc 8 MHz (większość modułów 16 MHz) | ✅ Naprawiony (#11) |
| B9 | Duplikat nagłówka sekcji | ✅ Naprawiony |

### Ryzyka (nie naprawione)
| # | Ryzyko | Priorytet | Wpływ |
|---|--------|-----------|-------|
| R1 | **Input premise niezweryfikowany** (#15): 2JZ ECU może nie mieć CAN bus — cały projekt opiera się na niepotwierdzonym założeniu | 🔴 Krytyczny | Projekt może być niewykonalny |
| R2 | **Coolant ID 0x19E** (#4): może być DSC StatusDSC_KCAN, powinno być 0x329 (DME2) | 🔴 Wysoki | Zegary mogą nie pokazać temperatury |
| R3 | **Single bus** (#10): jeden MCP2515 nasłuchuje Toyota I nadaje BMW na tym samym busie — kolizje | 🔴 Wysoki | Niebezpieczne w pojeździe |
| R4 | **RPM semantyka** (issue #6): założenie że ramka 2JZ przenosi RPM (nie RPM×6.4) — niezweryfikowane | 🟡 Średni | Błędne RPM na zegarach |
| R5 | **Checksum** zweryfikowana na 1 przykładzie — może nie pasować do wszystkich ramek | 🟡 Średni | Odrzucenie ramek przez zegary |
| R6 | **WiFi hasło** hardcoded ("translator123") — słabe, brak konfiguracji | 🟡 Średni | Bezpieczeństwo sieci |
| R7 | **Brak autentykacji** na dashboardzie — każdy w zasięgu AP ma dostęp | 🟡 Średni | Bezpieczeństwo |
| R8 | **Placeholder data** w API: oil temp = coolant temp, torque=0, voltage=13.8, rxcount/txcount hardcoded | 🟢 Niski | Nieprawdziwe dane w GUI |
| R9 | **Brak watchdog** — jeśli loop się zawiesi, urządzenie nie restartuje się | 🟢 Niski | Brak auto-recovery |
| R10 | **Brak OTA** — aktualizacja wymaga fizycznego dostępu USB | 🟢 Niski | Trudność aktualizacji |
| R11 | **Speed signed 12-bit** — kod traktuje jako unsigned (clamp 0x0FFF), ale DBC mówi signed | 🟡 Średni | Ujemna prędkość (cofanie) nie obsługiwana |
| R12 | **Brak walidacji** wejścia speed (uint16_t, max 65535 km/h — nierealne ale brak guard) | 🟢 Niski | Marginalny |
| R13 | **XSS w clearLogs** (innerHTML) — lokalne urządzenie, niskie ryzyko | 🟢 Niski | Marginalny |
| R14 | **CAN error reporting** hardcoded 0 w API — nie pokazuje realnych błędów | 🟢 Niski | Brak diagnostyki |

## 2. Priorytetowy plan review i ulepszeń

### Priorytet 1 — Krytyczny (wymaga sprzętu)
- **P1.1**: Weryfikacja input premise (#15) — podłącz do 2JZ ECU, sprawdź czy są ramki CAN
  - Kryterium akceptacji: przechwyt ramek CAN z 2JZ ECU na analizatorze, potwierdzenie ID i formatu
- **P1.2**: Weryfikacja coolant ID (#4) — sprawdź na pojeździe czy 0x19E czy 0x329
  - Kryterium akceptacji: zegary pokazują temperaturę po wysłaniu na właściwe ID
- **P1.3**: Rozdzielenie busów (#10) — dodaj drugi MCP2515 lub użyj ESP32 z wbudowanym CAN
  - Kryterium akceptacji: brak kolizji na magistrali Toyota, poprawna transmisja na BMW

### Priorytet 2 — Wysoki (kod, bez sprzętu)
- **P2.1**: Bezpieczeństwo WiFi — konfigurowalne hasło + opcjonalna autentykacja dashboardu
  - Kryterium akceptacji: hasło w config (nie hardcoded), Basic Auth na /api/*
- **P2.2**: Obsługa cofania (signed speed) — 12-bit signed, bit 13 MovingReverse
  - Kryterium akceptacji: test harness z ujemną prędkością przechodzi
- **P2.3**: Walidacja wejścia — guard na speed > 400 km/h, RPM > 10000
  - Kryterium akceptacji: test harness z wartościami granicznymi przechodzi
- **P2.4**: Realne liczniki i błędy w API — rxcount/txcount/errors z rzeczywistych zmiennych
  - Kryterium akceptacji: API zwraca rzeczywiste wartości, nie hardcoded

### Priorytet 3 — Średni (ulepszenia)
- **P3.1**: Watchdog timer — auto-restart przy zawieszeniu
- **P3.2**: OTA update — aktualizacja przez WiFi
- **P3.3**: Logi do LittleFS — trwały zapis z rotacją
- **P3.4**: Realne dane w GUI (oil temp sensor, torque, voltage) — wymaga sprzętu

### Priorytet 4 — Niski (polish)
- **P4.1**: XSS fix w clearLogs (textContent zamiast innerHTML)
- **P4.2**: Dokumentacja aktualizacja (README o WiFi, OLED, settings)

## 3. Kroki naprawy i weryfikacji (przed wykonaniem)

### Krok 1: Bezpieczeństwo WiFi (P2.1)
1. Zmień hardcoded hasło na konfigurowalne (#define WIFI_PASS)
2. Dodaj Basic Auth na endpointy /api/*
3. Test: kompilacja harness + weryfikacja logiki

### Krok 2: Signed speed (P2.2)
1. Zmień raw na int16_t (signed 12-bit)
2. Obsłuż bit 13 (MovingReverse) dla ujemnych prędkości
3. Dodaj test do harnessa

### Krok 3: Walidacja wejścia (P2.3)
1. Dodaj guard: speed > 400 → clamp 400
2. Dodaj guard: rpm > 10000 → clamp 10000
3. Dodaj test do harnessa

### Krok 4: Realne liczniki w API (P2.4)
1. Dodaj zmienne rxCount, txCount, canErrors
2. Inkrementuj w processIncomingFrame i sendBMWData
3. Aktualizuj handleStatus z realnych zmiennych

### Krok 5: XSS fix (P4.1)
1. Zmień innerHTML na textContent w clearLogs
2. Test: kompilacja

### Krok 6: Watchdog (P3.1)
1. Włącz ESP32 watchdog (esp_task_wdt)
2. Feed w loop()
3. Test: kompilacja

### Krok 7: Weryfikacja końcowa
1. Rebuild harness
2. Run all tests
3. Sprawdź git status
EOF
echo "plan written"