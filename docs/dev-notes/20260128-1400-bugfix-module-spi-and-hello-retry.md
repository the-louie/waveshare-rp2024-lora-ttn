# Bugfix: Module SPI constructor and HELLO WORLD retry

## Summary

- **Module constructor compile error:** RadioLib's `Module` constructor expects `SPIClass& spi`, not a pointer. The sketch was passing `&SPI`, which is `SPIClassRP2040*`. The fix was to pass `SPI` (reference) instead of `&SPI` in the `Module` constructor call in `setup()`, so the call matches the signature and compiles on Arduino-Pico (RP2040).

- **HELLO WORLD retry:** The first uplink after join sends "HELLO WORLD" on FPort 2. The retry loop could run indefinitely if the send never succeeded (e.g. no gateway). A maximum retry count was added so the loop eventually exits and the main loop continues; behaviour is consistent with the rest of the sketch (e.g. batch send retries).

- **Review:** No other logical errors or regressions were found in `setup()` or `loop()`.
