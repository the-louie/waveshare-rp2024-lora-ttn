/*
 * LoRaWAN OTAA credentials. Copy from TTN Console; do not commit real keys to public repo.
 * EU868 / TTN EU.
 *
 * RadioLib format (for beginOTAA):
 *   JoinEUI/DevEUI: uint64_t, MSB (literal) = paste hex from TTN as 0x...ULL.
 *   AppKey/NwkKey: uint8_t[], MSB (big-endian) C-style array; paste from TTN.
 */
#ifndef PICO_LORAWAN_TTN_APP_DEVICE_CONFIG_H
#define PICO_LORAWAN_TTN_APP_DEVICE_CONFIG_H

/* EUIs: MSB (human-readable) literal; copy hex from TTN (e.g. 70B3D57ED007560E -> 0x70B3D57ED007560EULL). */
#define LORAWAN_JOIN_EUI  0x0000000000000000ULL
//#define LORAWAN_DEV_EUI   0x70B3D57ED007560EULL  /* telamon-device-1 */
#define LORAWAN_DEV_EUI   0x70B3D57ED00756C3ULL  /* telamon-device-2 */

/* Keys: MSB (big-endian) C-style array; paste from TTN. NwkKey: pass nullptr in beginOTAA if not used. */
#define LORAWAN_APP_KEY   { 0x72, 0xA5, 0x30, 0xB5, 0xB0, 0xE7, 0xC3, 0x0C, 0x65, 0x2D, 0x66, 0xAD, 0x6D, 0xA9, 0x2C, 0xD5 }

#endif
