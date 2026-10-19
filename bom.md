# Lista części — 2JZ → BMW E92 CAN Translator

## BOM (Bill of Materials)

| # | Komponent | Specyfikacja | Cena orientacyjna (PLN) | Uwagi |
|---|-----------|--------------|-------------------------|-------|
| 1 | ESP32 DevKitC (38-pin) | ESP32-WROOM-32, USB-micro | ~35-45 zł | Kontroler główny |
| 2 | Moduł CAN MCP2515 + TJA1050 | SPI CAN controller + transceiver | ~10-15 zł | Na jednej płytce |
| 3 | Rezystor 120Ω | 1/4W, tolerancja 1% | ~2 zł | Tylko dla nowej magistrali |
| 4 | Przewody breadboard / goldpin | M-M, M-F, ~20cm | ~5-10 zł | Minimum 8 sztuk |
| 5 | Płytka prototypowa / obudowa | 830 punktów | ~5-10 zł | Opcjonalnie |
| 6 | Zasilacz USB 5V lub step-down 12V→5V | 5V / 1A minimum | ~10-15 zł | Step-down w samochodzie |

## Koszt całkowity

| Wariant | Koszt |
|---------|-------|
| Minimalny (bez obudowy, bez step-downa) | ~50-70 zł |
| Pełny (z obudową, zasilaczem, rezystorem) | ~67-97 zł |

## Gdzie kupić (Polska)

- Botland, Kamami, TME — komponenty elektroniczne
- AliExpress — tańsze moduły MCP2515, ESP32
- OLX — używane części, zasilacze

## Wskazówki zakupowe

1. **MCP2515 moduł** — upewnij się, że ma TJA1050 (lub SN65HVD230) — to transceiver CAN
2. **ESP32 DevKitC** — wersja 38-pin (nie 30-pin) dla wygody podłączania
3. **Rezystor 120Ω** — jeśli nie wiesz, czy potrzebny, zmierz opór CANH↔CANL w aucie: ~60Ω = terminacja OK
4. **Step-down 12V→5V** — wybierz z zabezpieczeniem przepięciem i zasilaniem minimum 1A
