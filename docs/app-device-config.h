/*
 * LoRaWAN OTAA credentials. Copy from TTN Console; do not commit real keys to public repo.
 * EU868 / TTN EU. JoinEUI/DevEUI as literal 0x...ULL; AppKey as C-style array MSB.
 * AppKey MUST match the key in TTN for the chosen DevEUI; MIC mismatch = wrong key.
 */
#ifndef PICO_LORAWAN_TTN_APP_DEVICE_CONFIG_H
#define PICO_LORAWAN_TTN_APP_DEVICE_CONFIG_H

#define LORAWAN_JOIN_EUI  0x0000000000000000ULL
#define LORAWAN_DEV_EUI   0x70B3D57ED007560EULL
#define LORAWAN_APP_KEY   { 0x72, 0xA5, 0x30, 0xB5, 0xB0, 0xE7, 0xC3, 0x0C, 0x65, 0x2D, 0x66, 0xAD, 0x6D, 0xA9, 0x2C, 0xD5 }

#endif
