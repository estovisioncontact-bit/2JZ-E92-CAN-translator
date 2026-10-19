# CAN ID dla BMW E9x

## Magistrala powertrain (500 kbps)

Tabela CAN ID używanych w magistrali powertrain BMW E9x (E8x) na podstawie projektu opendbc.

### ID wysyłane przez tłumacz (transmitter: tłumacz → magistrala)

| CAN ID | Nazwa | Opis | Bajty | Skalowanie | Jednostka |
|--------|-------|------|-------|------------|-----------|
| 0x316 | DME1 | RPM, moment obrotowy, status silnika | 0-7 | RPM: (B2*256+B3)/6.4 | RPM, Nm, % |
| 0x1A0 | Speed | Prędkość pojazdu | 0-7 | (B0*256+B1)/100 | km/h |
| 0x1A0 | Speed | Prędkość pojazdu (alt) | 0-7 | (B0*256+B1)/100 | km/h |
| 0x19E | DME2 | Temperatura płynu, oleju | 0-7 | Patrz poniżej | °C |
| 0x1D0 | EMS | Status silnika, hamulec | 0-7 | Mapa bitowa | — |

### Szczegółowy opis ramek

#### 0x316 — DME1 (Digital Motor Electronics 1)

Ramka zawierająca dane o obrotach silnika, momencie obrotowym i statusie.

| Bajt | Opis | Skalowanie | Jednostka |
|------|------|------------|-----------|
| 0 | Status silnika | Mapa bitowa | — |
| 1 | Status podrzędny | Mapa bitowa | — |
| 2 | RPM (low byte) | (B2*256+B3)/6.4 | RPM |
| 3 | RPM (high byte) | (B2*256+B3)/6.4 | RPM |
| 4 | Moment obrotowy | Mapa bitowa | Nm |
| 5 | Moment obrotowy | Mapa bitowa | Nm |
| 6 | Status DME | Mapa bitowa | — |
| 7 | Status DME | Mapa bitowa | — |

**Konwersja RPM:**
```cpp
// ECU 2JZ → BMW E9x
uint16_t temp_RPM = currentStatus.RPM * 64UL / 10U;  // RPM * 6.4
outMsg.buf[2] = lowByte(temp_RPM);   // low byte
outMsg.buf[3] = highByte(temp_RPM);  // high byte
```

#### 0x1A0 — Speed

Ramka zawierająca prędkość pojazdu.

| Bajt | Opis | Skalowanie | Jednostka |
|------|------|------------|-----------|
| 0 | Prędkość (low byte) | (B0*256+B1)/100 | km/h |
| 1 | Prędkość (high byte) | (B0*256+B1)/100 | km/h |
| 2-7 | Zarezerwowane / inne | — | — |

#### 0x19E — DME2

Ramka zawierająca temperatury.

| Bajt | Opis | Skalowanie | Jednostka |
|------|------|------------|-----------|
| 0-1 | Temperatura płynu chłodzącego | ((B0*256+B1)/4-48)*3 | °C |
| 2-3 | Temperatura oleju | ((B2*256+B3)/4-48)*3 | °C |
| 4-7 | Inne | — | — |

**Konwersja temperatury:**
```cpp
// °C → format BMW
uint8_t temp_coolant = ((celsius + 48) * 4) / 3;
```

### Mapa bitowa statusu silnika (0x316, bajt 0)

| Bit | Opis |
|-----|------|
| 0 | Engine running |
| 1 | Engine cranking |
| 2 | Engine warm |
| 3 | Check engine |
| 4 | Cruise control |
| 5 | Idle active |
| 6 | Kickdown |
| 7 | Reserved |

### Inne CAN ID w magistrali BMW E9x (informacyjnie)

| CAN ID | Nazwa | Opis |
|--------|-------|------|
| 0x130 | DSC | Dynamic Stability Control |
| 0x131 | ABS | Anti-lock Braking System |
| 0x1A0 | Kombi | Zegary (cluster) |
| 0x1D0 | IHKA | Klimatyzacja |
| 0x212 | LSZ | Światła |
| 0x213 | MRS | Poduszki powietrzne |
| 0x2A0 | SZL | Kolumna kierownicza |
| 0x321 | KOMBI | Kombi (inne dane) |
| 0x330 | ZGM | Central Gateway |
| 0x332 | CAS | Comfort Access System |
| 0x394 | JBBF | Junction Box |
| 0x3A0 | ZBE | Center console |

## CAN ID 2JZ (Toyota) — do nasłuchu

| CAN ID | Nazwa | Opis |
|--------|-------|------|
| 0x0B4 | Engine | RPM, prędkość kół (Toyota spec) |
| 0x0C8 | Vehicle | Prędkość pojazdu (Toyota spec) |
| 0x1C0 | Engine 2 | Temperatura, ciśnienie (Toyota spec) |

**Uwaga:** CAN ID Toyota mogą się różnić w zależności od rocznika i wersji ECU. Zalecana jest analiza ramek CAN z konkretnego pojazdu (sniffer).

## Przepływ danych

```
2JZ ECU (Toyota)          Tłumacz               BMW E92 (zegary/klimatyzacja)
─────────────────         ──────                ──────────────────────────
CAN ID 0x0B4 (RPM)   →    Przetwarzanie    →    CAN ID 0x316 (RPM)
CAN ID 0x0C8 (Speed) →    i konwersja      →    CAN ID 0x1A0 (Speed)
CAN ID 0x1C0 (Temp)  →    CAN              →    CAN ID 0x19E (Temp)
```

## Narzędzia do analizy CAN

- **SavvyCAN** — darmowy, multi-platformowy analizator CAN
- **CANalyzer** — profesjonalny (Vector)
- **Busmaster** — open source
- **Python + python-can** — skrypty własne
- **Arduino + MCP2515** — prosty sniffer (ten projekt!)
