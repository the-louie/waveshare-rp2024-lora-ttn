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

/* Keys: MSB (big-endian) C-style array; MUST match TTN for the DevEUI above. MIC mismatch = wrong key. */
/* telamon-device-1: 72A530B5B0E7C30C652D66AD6DA92CD5 | telamon-device-2: 09715FB1759414EB4768087215920246 */
#define LORAWAN_APP_KEY   { 0x09, 0x71, 0x5F, 0xB1, 0x75, 0x94, 0x14, 0xEB, 0x47, 0x68, 0x08, 0x72, 0x15, 0x92, 0x02, 0x46 }

#endif
