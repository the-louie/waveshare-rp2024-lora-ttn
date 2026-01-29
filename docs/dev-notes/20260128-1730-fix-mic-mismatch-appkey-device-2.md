# Fix MIC mismatch (AppKey for telamon-device-2)

## Problem

TTN Join Server returned `ns.up.join.cluster.fail` with `mic_mismatch` for telamon-device-2 (DevEUI 70B3D57ED00756C3, JoinEUI 0000000000000000). The device was reaching TTN but the Join Request MIC verification failed.

## Cause

MIC mismatch means the AppKey used by the device to compute the Join Request MIC does not match the AppKey registered in TTN for that (JoinEUI, DevEUI) pair. The sketch was configured for telamon-device-2 (LORAWAN_DEV_EUI = 70B3D57ED00756C3) but LORAWAN_APP_KEY was still set to the key for telamon-device-1 (72A530B5...). TTN has a different AppKey for device-2 (09715FB1759414EB4768087215920246).

## Changes

- **app-device-config.h**: Set LORAWAN_APP_KEY to the key registered in TTN for telamon-device-2 (09715FB1759414EB4768087215920246 as MSB-first byte array). Added a comment that LORAWAN_APP_KEY must match TTN for the chosen DevEUI and that MIC mismatch indicates the wrong key; included a short reference for both device keys so switching devices is clear.
- **docs/app-device-config.h**: Documented that AppKey must match the key in TTN for the chosen DevEUI and that MIC mismatch means wrong key.

## Result

The device uses the correct AppKey for telamon-device-2 when joining. After re-flashing, join should succeed provided TTN device registration and gateway connectivity are unchanged.
