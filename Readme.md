# Waveshare RP2040-LoRa on The Things Network (Arduino)

LoRaWAN/TTN sensor node for the [Waveshare RP2040-LoRa](https://www.waveshare.com/wiki/RP2040-LoRa) board (SX1262, single board). Functionality ported from the working [m0-lorawan-ttn](../m0-lorawan-ttn) (Adafruit Feather M0 LoRa) project. Same batched log payload format and TTN decoder.

**Hardware:** [Waveshare RP2040-LoRa](https://www.waveshare.com/wiki/RP2040-LoRa) is a **pre-wired PCB** (RP2040 + SX1262 on one board); SX1262 MISO is fixed to **GP24**—no rewire possible. Pinout is for this integrated board, not the Pico-LoRa-SX1262 HAT (RadioLib #947). **Critical:** No TCXO; sketch sets TCXO voltage to 0 in `radio->begin(..., 0.0f)` per [RadioLib](https://github.com/jgromes/RadioLib) to avoid init errors (-706/-707).

## Build environment

- **Board:** Raspberry Pi Pico (Arduino-Pico core by earlephilhower). In Arduino IDE: Tools → Board → Raspberry Pi Pico.
- **Libraries:** Install via Library Manager (use latest; RadioLib 6.x or 7.x required for LoRaWANNode):
  - **RadioLib** (jgromes) – SX1262 and LoRaWAN.
  - **OneWire** (Paul Stoffregen)
  - **DallasTemperature** (Miles Burton)
  - **EEPROM** (built-in with Arduino-Pico; used for session and log persistence).
- **Core:** Add Boards Manager URL: `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`, then install "Raspberry Pi Pico/RP2040".

**OneWire RP2040 warning:** The OneWire library emits a `#warning` on RP2040 ("Fallback mode. Using API calls for pinMode, digitalRead and digitalWrite."). Behaviour is correct; the library uses the standard Arduino GPIO API on RP2040. To hide the warning, add a `platform.local.txt` next to `platform.txt` in your Arduino-Pico core (e.g. `Arduino15/packages/earlephilhower/hardware/pico/<version>/`) with: `compiler.cpp.extra_flags=-Wno-cpp`.

Pin and SPI configuration: see [config.h](config.h). Pins match the [Waveshare wiki](https://www.waveshare.com/wiki/RP2040-LoRa). **SPI:** Default is **SPI1** with **MISO=GP24** (RADIO_MISO). The RP2040 can map SPI1 MISO to GP8, GP12, **GP24**, or GP28 via `setRX()`—so the sketch uses `SPI1.setRX(24)` to match the board's trace. **No rewire.** NSS=13, MOSI=15, SCK=14. This avoids the SPI0 hang on some Arduino-Pico builds. Set `USE_SPI1_FOR_RADIO` to 0 in [config.h](config.h) to try SPI0 (may hang).

## TTN setup

EU868, OTAA. Put AppEUI, DevEUI, AppKey in [app-device-config.h](app-device-config.h). Use the payload decoder [ttn-decoder-batch.js](ttn-decoder-batch.js) in TTN Console → Application → Payload Formats → Custom.

## Payload format

Same as m0-lorawan-ttn: FPort 1 = batched log `[vbat_hi,vbat_lo][n][tick_3B,temp_2B]×n` (big-endian); FPort 2 = "HELLO WORLD" string.

## Limitations

- No hardware RTC on RP2040; software epoch only (persisted, updated after TX). Ticks may be 0 until first sync; decoder uses `received_at` for tick=0. DeviceTimeReq can be added via RadioLib `sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME)` and `getMacDeviceTimeAns()` to update epoch.
- Deep sleep: sketch uses delay-based intervals; RP2040 deep sleep can be added for lower power.
- VBAT: RP2040-LoRa has no dedicated battery sense; see [config.h](config.h) for VBAT_ADC_PIN (e.g. VSYS divider) or placeholder.
