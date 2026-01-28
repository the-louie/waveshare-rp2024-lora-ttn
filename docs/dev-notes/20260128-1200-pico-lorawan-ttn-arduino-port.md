# Pico LoRaWAN TTN Arduino port

Port of the working m0-lorawan-ttn (Adafruit Feather M0 LoRa) sensor node to a new Arduino project for the Waveshare RP2040-LoRa board (pico-lorawan-ttn). Plan executed from `.cursor/plans/pico_lorawan_ttn_arduino_8fccf1d8.plan.md`.

## Summary

- **Project:** New folder `pico-lorawan-ttn/` with a single Arduino sketch and supporting headers.
- **Hardware:** Waveshare RP2040-LoRa (SX1262, single board). Pin and SPI constants taken from pico-lorawan-waveshare `src/boards/rp2040/sx126x-board.c`: NSS=13, RST=23, DIO1=16, BUSY=18, ANT_SW=17, MOSI=15, MISO=24, SCK=14, SPI0, &lt;18 MHz (10 MHz used).
- **Stack:** RadioLib (SX1262 + LoRaWANNode), Arduino-Pico core. EU868, OTAA. Session and nonces persisted via getBufferSession/setBufferSession and getBufferNonces/setBufferNonces in EEPROM.
- **Behaviour:** Same as M0 where feasible: batched log (LogEntry: 3-byte tick + 2-byte temp), RAM buffer + EEPROM-backed log, merge on send, clear on success. RUN_MODE (DEV/PROD), SERIAL macros, SEND_PERIOD_MEASURES. After join: "HELLO WORLD" on FPort 2; batch on FPort 1. Software epoch (no hardware RTC); persisted rtcEpoch and epochBaseMillis; tick = 1-min since 2026-01-01.
- **Sensors:** DS18B20 on ONE_WIRE_BUS (GP4). VBAT: placeholder or VBAT_ADC_PIN if defined in config.h.
- **Files:** `pico-lorawan-ttn.ino`, `config.h`, `app-device-config.h`, `ttn-decoder-batch.js` (same decoder as M0), `Readme.md`, `.gitignore`. Credentials in app-device-config.h (sketch root for Arduino include).
- **Limitations:** No hardware RTC; optional DeviceTimeReq via RadioLib for network time. Delay-based intervals (no deep sleep in this version). TODO.md / TODO-summarized.md were not present in the repo, so no task was removed.
