# Code review: pico-lorawan-ttn vs RadioLib and Waveshare references

**Date:** 2026-01-28

## Scope

Review of `pico-lorawan-ttn.ino` and `config.h` against:

- RadioLib SX1262 and LoRaWANNode API (official docs, begin/tcxo, RF switch)
- Waveshare RP2040-LoRa reference: `pico-lorawan-waveshare` `src/boards/rp2040/sx126x-board.c` (pinout, TCXO, ANT_SW behavior)

## Findings and changes

### 1. TCXO configuration (Waveshare has no TCXO)

- **Reference:** RadioLib SX1262 `begin()` accepts `float tcxoVoltage=1.6`. Docs state: for XTAL/no-TCXO use `tcxoVoltage = 0` (avoids -706/-707 errors). Waveshare sx126x-board does not use TCXO; `SX126xGetBoardTcxoWakeupTime()` returns 0.
- **Change:** Pass `0.0f` as the tcxoVoltage argument in `radio->begin(...)` instead of calling `radio->setTCXO(0.0f)` after `begin()`. Explicit `begin(434.0f, 125.0f, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8, 0.0f)` so TCXO is set at init.

### 2. ANT_SW polarity (Waveshare: low=TX, high=RX/idle)

- **Reference:** Waveshare sx126x-board.c: MODE_TX → GpioWrite 0 (low); else (RX, idle) → GpioWrite 1 (high). RadioLib `setRfSwitchPins(rxEn, txEn)` drives idle=LOW, TX=txEn HIGH, RX=rxEn HIGH. Using one pin for both would drive TX high, which is opposite to Waveshare.
- **Change:** Replace `setRfSwitchPins(RADIO_ANT_SW, RADIO_ANT_SW)` with `setRfSwitchTable()`: one pin (RADIO_ANT_SW), MODE_IDLE and MODE_RX = HIGH, MODE_TX = LOW, matching the Waveshare board HAL.

### 3. LoRaWANNode and session persistence

- **Checked:** beginOTAA, setBufferSession/setBufferNonces, getBufferSession/getBufferNonces, activateOTAA usage matches RadioLib 6.x/7.x session persistence (no code change).

### 4. Pinout and SPI

- **Checked:** NSS=13, RST=23, DIO1=16, BUSY=18, ANT_SW=17, MOSI=15, MISO=24, SCK=14 and SPI1 workaround (MISO on GP12) already match config.h comments and Waveshare wiki; no change.

## Files touched

- `pico-lorawan-ttn/pico-lorawan-ttn.ino`: TCXO via `begin(..., 0.0f)`; ANT_SW via `setRfSwitchTable` with correct polarity.
