# Plan: GUI dla użytkownika (ESP32 Web Dashboard)

## Cel
Użytkownik (mechanik/kierowca) ma łatwy dostęp do stanu tłumacza bez Serial Monitora:
- podgląd RPM / prędkości / temperatury w czasie rzeczywistym
- status magistrali CAN (RX/TX, błędy, czas od ostatniej ramki)
- logi zdarzeń (CAN frames, błędy) zapisywane do analizy
- konfiguracja (ID ramek, skala, tryb debug)

## Architektura
```
┌─────────────────────────────────────────────┐
│  ESP32 (WiFi AP/STA)                        │
│  ┌────────────┐  ┌──────────────────────┐   │
│  │ CAN handler│→ │ WebServer (port 80)  │   │
│  │ (MCP2515)  │  │  GET /  (dashboard)  │   │
│  └────────────┘  │  GET /api/status     │   │
│  ┌────────────┐  │  GET /api/logs       │   │
│  │ Log buffer │  │  GET /api/config     │   │
│  │ (ring, RAM)│  │  POST /api/config    │   │
│  └────────────┘  └──────────────────────┘   │
│         │                                    │
│         ▼                                    │
│  SD/Flash (LittleFS) — trwałe logi         │
└─────────────────────────────────────────────┘
         ▲ WiFi
         │
   ┌─────┴─────┐
   │ Przeglądarka │  (telefon/PC)
   └───────────┘
```

## Komponenty GUI (układ ekranu)

```
┌──────────────────────────────────────┐
│  [Header] 2JZ→E92 Translator  [WiFi]  │
├──────────┬──────────┬────────────────┤
│  RPM     │  Speed   │  Coolant       │
│  3500    │  120     │  87 °C         │
│  [bar]   │  [bar]   │  [bar]         │
├──────────┴──────────┴────────────────┤
│  CAN Status                          │
│  RX: 42 frames/s  TX: 20 frames/s    │
│  Last RX: 0.05s ago  Errors: 0       │
│  Bus: 500 kbps  Crystal: 16 MHz      │
├──────────────────────────────────────┤
│  Log (ostatnie 50 zdarzeń)           │
│  [12:34:56] RX 0x0B4 RPM=3500       │
│  [12:34:56] TX 0x316 raw=22400      │
│  [12:34:57] RX 0x0C8 speed=120      │
│  ...                                 │
├──────────────────────────────────────┤
│  [Config] [Download Logs] [Clear]    │
└──────────────────────────────────────┘
```

## Zapis logów — analiza czasu
- Ring buffer w RAM: 50 wpisów × ~80 bajtów = 4 KB (natychmiastowy zapis, <1 ms)
- Flush do LittleFS: co 5 s lub przy zapełnieniu (batch, ~50 ms dla 4 KB)
- Pobieranie przez HTTP: /api/logs?format=csv → ~200 ms dla 1000 wpisów
- Retencja: 7 dni (rotacja plików dziennych)

## Etapy implementacji
1. [x] Plan (ten dokument)
2. Dodaj WiFi AP mode (ESP32 jako access point)
3. WebServer z endpointami /api/status, /api/logs
4. Dashboard HTML (embedded w flash, ~8 KB)
5. Log buffer + flush do LittleFS
6. Test na sprzęcie (wymaga ESP32)

## Zależności
- WiFi (wbudowana ESP32)
- WebServer (wbudowana)
- LittleFS (wbudowana)
- ArduinoJson (opcjonalnie, dla API)

## Ograniczenia
- Bez sprzętu: kod kompiluje się, ale nie można przetestować WiFi/CAN jednocześnie
- Pamięć ESP32: dashboard ~8 KB flash, log buffer 4 KB RAM
EOF
echo "Plan created"