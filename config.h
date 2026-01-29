/*
 * Copyright (c) 2026 the_louie
 *
 * Hardware and LoRaWAN config for Waveshare RP2040-LoRa (SX1262).
 * Pinout from pico-lorawan-waveshare src/boards/rp2040/sx126x-board.c
 * and https://www.waveshare.com/wiki/RP2040-LoRa
 * SPI must be < 18 MHz (wiki); 10 MHz used here.
 *
 * This pinout is for the Waveshare RP2040-LoRa integrated board (internal routing),
 * not generic Raspberry Pi Pico pinouts. Arduino-Pico / pins.h defaults may differ;
 * all radio pins are defined here (config.h) for this board.
 */

#ifndef WAVESHARE_RP2040_LORA_TTN_CONFIG_H
#define WAVESHARE_RP2040_LORA_TTN_CONFIG_H

/* SX1262 pins (RP2040-LoRa single board). Wiki: DIO1=GP16, RST=GP23, MISO=GP24,
 * MOSI=GP15, CLK=GP14, CS=GP13, BUSY=GP18, ANT_SW=GP17. Not Pico-LoRa-SX1262 HAT. */
#define RADIO_NSS    13   /* SX1262_CS   -> GP13 */
#define RADIO_RST    23   /* SX1262_RST  -> GP23 */
#define RADIO_DIO1   16   /* SX1262_DIO1 -> GP16 */
#define RADIO_BUSY   18   /* SX1262_BUSY -> GP18 */
#define RADIO_ANT_SW 17   /* SX1262_ANT_SW -> GP17 */
#define RADIO_MOSI   15   /* SX1262_MOSI -> GP15 */
#define RADIO_MISO   24   /* SX1262_MISO -> GP24 (Waveshare PCB). RP2040 SPI1 MISO can be GP8/12/24/28 via setRX(). */
#define RADIO_SCK    14   /* SX1262_CLK  -> GP14 */

/* Use SPI1 (1) with MISO=GP24 (RADIO_MISO): setRX(24) maps SPI1 MISO to board trace. No rewire. Avoids SPI0 hang.
 * Set to 0 to use SPI0 (MISO=24); may hang on some Arduino-Pico builds. */
#define USE_SPI1_FOR_RADIO 1

/* SPI: use SPI0 or SPI1 per USE_SPI1_FOR_RADIO. Max 18 MHz per wiki. */
#define RADIO_SPI_FREQ_HZ  (10 * 1000 * 1000)

/* DS18B20 one-wire data pin; must not conflict with radio pins */
#define ONE_WIRE_BUS 4

/* VBAT: RP2040-LoRa has no dedicated battery sense. Use ADC for VSYS if wired, else placeholder. */
#define VBAT_ADC_PIN 29

#endif
