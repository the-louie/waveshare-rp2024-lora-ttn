/*
 * Copyright (c) 2026 the_louie
 *
 * Waveshare RP2040-LoRa LoRaWAN/TTN sensor node (Arduino).
 * Functionality ported from m0-lorawan-ttn; radio via RadioLib SX1262.
 * Pinout: config.h (from pico-lorawan-waveshare sx126x-board.c).
 */

#include "config.h"
#include <RadioLib.h>
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <string.h>
#include "app-device-config.h"

/* Increment for each upload to confirm firmware on device. */
#define FIRMWARE_BUILD 27

/* RUN_MODE: DEV = short intervals + Serial; TEST = same intervals + Serial; PROD = 5 min, no Serial. */
#define RUN_MODE_DEV  1
#define RUN_MODE_TEST 2
#define RUN_MODE_PROD 3
#define RUN_MODE RUN_MODE_DEV

#if RUN_MODE == RUN_MODE_PROD
#define SERIAL_PRINT(...)
#define SERIAL_PRINTLN(...)
#define SERIAL_BEGIN(...)
#else
#define SERIAL_PRINT(...)    Serial.print(__VA_ARGS__)
#define SERIAL_PRINTLN(...)  Serial.println(__VA_ARGS__)
#define SERIAL_BEGIN(...)    Serial.begin(__VA_ARGS__)
#endif

#define RTC_DEFAULT_EPOCH 0
#define CUSTOM_EPOCH      1735689600u  /* 2026-01-01 00:00:00 UTC */
#define SECONDS_PER_TICK  60u

#if RUN_MODE == RUN_MODE_DEV
#define SEND_PERIOD_MEASURES  1u
#define SLEEP_SECONDS        30
#elif RUN_MODE == RUN_MODE_TEST
#define SEND_PERIOD_MEASURES  1u
#define SLEEP_SECONDS        30
#else
#define SEND_PERIOD_MEASURES  1u
#define SLEEP_SECONDS        300
#endif

#define PERSIST_MAGIC 0x4C4D4943u
#define LOG_FLASH_MAGIC 0x4C4F4732u
#define LOG_ENTRIES_MAX 48
#define LOG_SEND_CAP    43

struct __attribute__((packed)) LogEntry {
    uint8_t timeTick[3];
    uint16_t temperature;
};

static inline uint32_t getTick24(const LogEntry* e) {
    return ((uint32_t)(e->timeTick[0]) << 16) | ((uint32_t)(e->timeTick[1]) << 8) | (uint32_t)(e->timeTick[2]);
}
static inline void setTick24(LogEntry* e, uint32_t val) {
    val &= 0xFFFFFFu;
    e->timeTick[0] = (uint8_t)(val >> 16);
    e->timeTick[1] = (uint8_t)(val >> 8);
    e->timeTick[2] = (uint8_t)(val);
}

typedef struct {
    uint32_t magic;
    uint32_t wakeCounter;
    uint32_t rtcEpoch;
    uint32_t lastTimeSyncEpoch;
    uint8_t measuresInPeriod;
    uint8_t sessionBuf[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
    uint8_t noncesBuf[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
} PersistentData_t;

typedef struct {
    uint32_t magic;
    uint8_t count;
    uint8_t reserved[3];
    LogEntry entries[LOG_ENTRIES_MAX];
} PersistentLog_t;

static PersistentData_t persistentData;
static PersistentLog_t persistentLog;
static LogEntry dataBuffer[LOG_ENTRIES_MAX];
static uint8_t ramCount;
static uint32_t softwareEpoch;  /* rtcEpoch + (millis() - epochBaseMillis) / 1000 */
static uint32_t epochBaseMillis;

static Module* mod = nullptr;
static SX1262* radio = nullptr;
static LoRaWANNode* node = nullptr;
static OneWire oneWire(ONE_WIRE_BUS);
static DallasTemperature sensors(&oneWire);
static bool haveStoredSession;
static bool joined;
static bool pendingSend;
static bool pendingHello;

static uint16_t encodeTemperatureHighRes(float tempC);
static uint16_t buildBatchPayload(uint8_t* buf, uint16_t vbatCentivolt, uint8_t n, const LogEntry* entries, uint32_t currentTicks);
static void mergeBackupAndPrepareSend(void);
static void do_wake(void);
static void do_send(void);
static void persistData(void);
static void loadData(void);
static void persistLog(void);
static void loadLog(void);

static uint16_t encodeTemperatureHighRes(float tempC) {
    if (tempC < -50.0f || tempC != tempC) return 0xFFFFu;
    if (tempC < 0.0f) return 0xFFFEu;
    if (tempC > 30.0f) return 0xFFFDu;
    int v = (int)(tempC * 100.0f);
    if (v > 3000) v = 3000;
    if (v < 0) v = 0;
    return (uint16_t)v;
}

static uint16_t buildBatchPayload(uint8_t* buf, uint16_t vbatCentivolt, uint8_t n, const LogEntry* entries, uint32_t currentTicks) {
    uint16_t i = 0;
    buf[i++] = (uint8_t)(vbatCentivolt >> 8);
    buf[i++] = (uint8_t)(vbatCentivolt & 0xFF);
    buf[i++] = n;
    for (uint8_t k = 0; k < n && (i + 5) <= 256u; k++) {
        uint32_t tick = getTick24(&entries[k]);
        if (tick == 0 && currentTicks <= 0xFFFFFFu) {
            uint32_t offset = (uint32_t)(n - 1 - k);
            tick = (currentTicks >= offset) ? (currentTicks - offset) : 0u;
            tick &= 0xFFFFFFu;
        }
        buf[i++] = (uint8_t)(tick >> 16);
        buf[i++] = (uint8_t)(tick >> 8);
        buf[i++] = (uint8_t)(tick);
        buf[i++] = (uint8_t)(entries[k].temperature >> 8);
        buf[i++] = (uint8_t)(entries[k].temperature & 0xFF);
    }
    return i;
}

static void mergeBackupAndPrepareSend(void) {
    loadLog();
    uint8_t flashCount = (persistentLog.magic == LOG_FLASH_MAGIC && persistentLog.count <= LOG_ENTRIES_MAX)
        ? persistentLog.count : 0;
    uint8_t total = flashCount + ramCount;
    if (total > LOG_ENTRIES_MAX) total = LOG_ENTRIES_MAX;
    LogEntry merged[LOG_ENTRIES_MAX];
    uint8_t i = 0;
    for (uint8_t k = 0; k < flashCount && i < total; k++) merged[i++] = persistentLog.entries[k];
    for (uint8_t k = 0; k < ramCount && i < total; k++) merged[i++] = dataBuffer[k];
    persistentLog.magic = LOG_FLASH_MAGIC;
    persistentLog.count = i;
    for (uint8_t k = 0; k < i; k++) persistentLog.entries[k] = merged[k];
    persistLog();
    ramCount = 0;
}

static void persistData(void) {
    persistentData.magic = PERSIST_MAGIC;
    if (node && node->isActivated()) {
        memcpy(persistentData.sessionBuf, node->getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
        memcpy(persistentData.noncesBuf, node->getBufferNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
    }
    EEPROM.put(0, persistentData);
    EEPROM.commit();
}

static void loadData(void) {
    EEPROM.get(0, persistentData);
}

static void persistLog(void) {
    EEPROM.put(sizeof(PersistentData_t), persistentLog);
    EEPROM.commit();
}

static void loadLog(void) {
    EEPROM.get(sizeof(PersistentData_t), persistentLog);
}

static void do_send(void) {
    if (!node || !node->isActivated()) return;
    uint16_t vbatCentivolt = 330;  /* Placeholder: 3.30 V when no VBAT sense */
#if defined(VBAT_ADC_PIN)
    /* RP2040 ADC is 12-bit (0–4095) on Arduino-Pico. */
    vbatCentivolt = (uint16_t)(analogRead(VBAT_ADC_PIN) * (3.3f / 4096.0f) * 100.0f);
#endif
    uint8_t n = (persistentLog.count <= LOG_SEND_CAP) ? persistentLog.count : (uint8_t)LOG_SEND_CAP;
    uint32_t tForTicks = (softwareEpoch < CUSTOM_EPOCH) ? CUSTOM_EPOCH : softwareEpoch;
    uint32_t currentTicks = (softwareEpoch >= CUSTOM_EPOCH) ? ((tForTicks - CUSTOM_EPOCH) / SECONDS_PER_TICK) : 0x1000000u;
    uint8_t paybuf[2 + 1 + 5 * LOG_SEND_CAP];
    uint16_t payLen = buildBatchPayload(paybuf, vbatCentivolt, n, persistentLog.entries, currentTicks);
    SERIAL_PRINT(F("Sending batch n="));
    SERIAL_PRINT(n);
    SERIAL_PRINT(F(" VBat="));
    SERIAL_PRINTLN((float)vbatCentivolt / 100.0f);
    int16_t res = node->sendReceive(paybuf, (size_t)payLen, 1, false);
    if (res == RADIOLIB_ERR_NONE || res > 0) {
        persistentLog.magic = 0;
        persistentLog.count = 0;
        persistLog();
        persistentData.rtcEpoch = softwareEpoch;
        epochBaseMillis = millis();
        persistData();
        SERIAL_PRINTLN(F("TX complete"));
    } else {
        SERIAL_PRINT(F("TX err "));
        SERIAL_PRINTLN(res);
    }
    pendingSend = false;
}

static void do_wake(void) {
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);
    uint32_t epoch = softwareEpoch;
    uint32_t t = (epoch < CUSTOM_EPOCH) ? CUSTOM_EPOCH : epoch;
    uint32_t ticks = (t - CUSTOM_EPOCH) / SECONDS_PER_TICK;
    if (ticks > 0xFFFFFFu) ticks = 0xFFFFFFu;
    LogEntry e;
    setTick24(&e, (uint32_t)ticks);
    e.temperature = encodeTemperatureHighRes(tempC);
    if (ramCount < LOG_ENTRIES_MAX) {
        dataBuffer[ramCount++] = e;
    } else {
        memmove(dataBuffer, &dataBuffer[1], (LOG_ENTRIES_MAX - 1) * sizeof(LogEntry));
        dataBuffer[LOG_ENTRIES_MAX - 1] = e;
    }
    uint8_t period = (persistentData.measuresInPeriod + 1) % SEND_PERIOD_MEASURES;
    persistentData.measuresInPeriod = period;
    persistentData.wakeCounter++;
    /* Persist only on join/TX/HELLO to limit EEPROM wear; wakeCounter/measuresInPeriod may reset on power loss. */
    SERIAL_PRINT(F("Wake #"));
    SERIAL_PRINT(persistentData.wakeCounter);
    SERIAL_PRINT(F(" ts="));
    SERIAL_PRINT(epoch);
    SERIAL_PRINT(F(" Temp="));
    SERIAL_PRINTLN(tempC);
    if (period == 0) {
        mergeBackupAndPrepareSend();
        pendingSend = true;
    }
}

void setup() {
    SERIAL_BEGIN(9600);
    delay(2000);
    SERIAL_PRINT(FIRMWARE_BUILD);
    SERIAL_PRINTLN(F(" pico-lorawan-ttn starting"));

    SERIAL_PRINTLN(F("EEPROM begin..."));
    EEPROM.begin(1024);  /* Min needed: sizeof(PersistentData_t)+sizeof(PersistentLog_t); 1024 plenty */
    loadData();
    loadLog();
    SERIAL_PRINTLN(F("EEPROM loaded"));
#if RUN_MODE != RUN_MODE_PROD
    delay(100);  /* Let USB serial buffer drain so next print does not block */
#endif
    SERIAL_PRINTLN(F("post-load"));
    haveStoredSession = (persistentData.magic == PERSIST_MAGIC);
    if (!haveStoredSession) {
        persistentData.wakeCounter = 0;
        persistentData.measuresInPeriod = 0;
    } else {
        persistentData.measuresInPeriod = persistentData.measuresInPeriod % SEND_PERIOD_MEASURES;
    }
    softwareEpoch = (persistentData.rtcEpoch != 0 && persistentData.rtcEpoch != 0xFFFFFFFFu)
        ? persistentData.rtcEpoch : RTC_DEFAULT_EPOCH;
    epochBaseMillis = millis();
    SERIAL_PRINTLN(F("pre-SPI"));
#if RUN_MODE != RUN_MODE_PROD
    delay(50);
#endif
    SERIAL_PRINTLN(F("SPI begin..."));
#if RUN_MODE != RUN_MODE_PROD
    delay(100);
#endif

#if defined(USE_SPI1_FOR_RADIO) && (USE_SPI1_FOR_RADIO == 1)
    /* SPI1 with MISO on RADIO_MISO (GP24): RP2040 allows SPI1 MISO on GP8/12/24/28 via setRX().
     * Waveshare board has MISO on GP24; no rewire. Avoids SPI0 set* hang.
     * Do all set* before begin() so pin assignments take effect before hardware registers are locked. */
    SPI1.setRX(RADIO_MISO);
    SPI1.setTX(RADIO_MOSI);
    SPI1.setSCK(RADIO_SCK);
    SPI1.begin();
#if RUN_MODE != RUN_MODE_PROD
    SERIAL_PRINT(F("SPI1 begun (RX=")); SERIAL_PRINT(RADIO_MISO); SERIAL_PRINT(F(" TX=")); SERIAL_PRINT(RADIO_MOSI); SERIAL_PRINT(F(" SCK=")); SERIAL_PRINT(RADIO_SCK); SERIAL_PRINTLN(F(")"));
#endif
#if RUN_MODE != RUN_MODE_PROD
    delay(100);
    SERIAL_PRINT(F("Pins (config.h): NSS=")); SERIAL_PRINT(RADIO_NSS); SERIAL_PRINT(F(" RST=")); SERIAL_PRINT(RADIO_RST); SERIAL_PRINT(F(" DIO1=")); SERIAL_PRINT(RADIO_DIO1); SERIAL_PRINT(F(" BUSY=")); SERIAL_PRINT(RADIO_BUSY); SERIAL_PRINT(F(" ANT_SW=")); SERIAL_PRINT(RADIO_ANT_SW); SERIAL_PRINT(F(" MOSI=")); SERIAL_PRINT(RADIO_MOSI); SERIAL_PRINT(F(" MISO=")); SERIAL_PRINT(RADIO_MISO); SERIAL_PRINT(F(" SCK=")); SERIAL_PRINT(RADIO_SCK); SERIAL_PRINTLN(F(" (SPI1)"));
#endif
    /* Manual RST pulse before Module: ensure SX1262 out of reset (err=-2 = no SPI response). */
    pinMode(RADIO_RST, OUTPUT);
    digitalWrite(RADIO_RST, LOW);
    delay(10);
    digitalWrite(RADIO_RST, HIGH);
    delay(10);
    mod = new Module(RADIO_NSS, RADIO_DIO1, RADIO_RST, RADIO_BUSY, SPI1, SPISettings(RADIO_SPI_FREQ_HZ, MSBFIRST, SPI_MODE0));
#else
    /* SPI0 path: on some Arduino-Pico builds the first set* blocks. Try begin() first. */
    SERIAL_PRINTLN(F("SPI begin() first..."));
    SPI.begin();
#if RUN_MODE != RUN_MODE_PROD
    delay(50);
#endif
    SERIAL_PRINT(F("SPI setTX(")); SERIAL_PRINT(RADIO_MOSI); SERIAL_PRINTLN(F(")..."));
    SPI.setTX(RADIO_MOSI);
    SERIAL_PRINT(F("SPI setSCK(")); SERIAL_PRINT(RADIO_SCK); SERIAL_PRINTLN(F(")..."));
    SPI.setSCK(RADIO_SCK);
    SERIAL_PRINT(F("SPI setRX(")); SERIAL_PRINT(RADIO_MISO); SERIAL_PRINTLN(F(")..."));
    SPI.setRX(RADIO_MISO);
    SERIAL_PRINTLN(F("SPI begun"));
#if RUN_MODE != RUN_MODE_PROD
    delay(100);
    SERIAL_PRINT(F("Pins (config.h): NSS=")); SERIAL_PRINT(RADIO_NSS); SERIAL_PRINT(F(" RST=")); SERIAL_PRINT(RADIO_RST); SERIAL_PRINT(F(" DIO1=")); SERIAL_PRINT(RADIO_DIO1); SERIAL_PRINT(F(" BUSY=")); SERIAL_PRINT(RADIO_BUSY); SERIAL_PRINT(F(" ANT_SW=")); SERIAL_PRINT(RADIO_ANT_SW); SERIAL_PRINT(F(" MOSI=")); SERIAL_PRINT(RADIO_MOSI); SERIAL_PRINT(F(" MISO=")); SERIAL_PRINT(RADIO_MISO); SERIAL_PRINT(F(" SCK=")); SERIAL_PRINT(RADIO_SCK); SERIAL_PRINTLN(F(" (SPI0)"));
#endif
    pinMode(RADIO_RST, OUTPUT);
    digitalWrite(RADIO_RST, LOW);
    delay(10);
    digitalWrite(RADIO_RST, HIGH);
    delay(10);
    mod = new Module(RADIO_NSS, RADIO_DIO1, RADIO_RST, RADIO_BUSY, SPI, SPISettings(RADIO_SPI_FREQ_HZ, MSBFIRST, SPI_MODE0));
#endif
    /* Waveshare ANT_SW (GP17): low=TX, high=RX/idle. setRfSwitchPins would drive TX high; use table. */
    static const uint32_t rfSwitchPins[] = { RADIO_ANT_SW, RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC };
    static const Module::RfSwitchMode_t rfSwitchTable[] = {
        { Module::MODE_IDLE, { HIGH, 0, 0, 0, 0 } },
        { Module::MODE_RX,   { HIGH, 0, 0, 0, 0 } },
        { Module::MODE_TX,   { LOW,  0, 0, 0, 0 } },
        END_OF_MODE_TABLE,
    };
    mod->setRfSwitchTable(rfSwitchPins, rfSwitchTable);
    radio = new SX1262(mod);
    SERIAL_PRINTLN(F("SX1262 init..."));
#if RUN_MODE != RUN_MODE_PROD
    /* Debug: SX1262 BUSY (GP18) usually transitions or is high during power-up/reset. If MCU can't
     * talk over SPI (e.g. wrong MISO), BUSY often stays stagnant at 0. After correct MISO (GP24),
     * BUSY should toggle during begin(). */
    pinMode(RADIO_BUSY, INPUT);
    SERIAL_PRINT(F("BUSY pin (GP")); SERIAL_PRINT(RADIO_BUSY); SERIAL_PRINT(F(") before begin: "));
    SERIAL_PRINTLN(digitalRead(RADIO_BUSY));
    delay(50);
#endif
    /* EU868: 868.1/868.3/868.5 MHz. LoRaWAN sync word 0x34 (gateway expects it); PRIVATE=0x12 would not decode. */
    int16_t beginRes = radio->begin(868.1f, 125.0f, 9, 7, RADIOLIB_LORAWAN_LORA_SYNC_WORD, 10, 8, 0.0f);
    if (beginRes != RADIOLIB_ERR_NONE) {
        SERIAL_PRINT(F("SX1262 init failed err="));
        SERIAL_PRINTLN(beginRes);
#if RUN_MODE != RUN_MODE_PROD
        if (beginRes == -2) {
#if defined(USE_SPI1_FOR_RADIO) && (USE_SPI1_FOR_RADIO == 1)
            SERIAL_PRINT(F("-2=CHIP_NOT_FOUND. SPI1. Check NSS(")); SERIAL_PRINT(RADIO_NSS); SERIAL_PRINT(F(") MISO(")); SERIAL_PRINT(RADIO_MISO); SERIAL_PRINT(F(") MOSI(")); SERIAL_PRINT(RADIO_MOSI); SERIAL_PRINT(F(") SCK(")); SERIAL_PRINT(RADIO_SCK); SERIAL_PRINT(F(") RST(")); SERIAL_PRINT(RADIO_RST); SERIAL_PRINTLN(F("). config.h pins."));
#else
            SERIAL_PRINT(F("-2=CHIP_NOT_FOUND. Check NSS(")); SERIAL_PRINT(RADIO_NSS); SERIAL_PRINT(F(") MISO(")); SERIAL_PRINT(RADIO_MISO); SERIAL_PRINT(F(") MOSI(")); SERIAL_PRINT(RADIO_MOSI); SERIAL_PRINT(F(") SCK(")); SERIAL_PRINT(RADIO_SCK); SERIAL_PRINT(F(") RST(")); SERIAL_PRINT(RADIO_RST); SERIAL_PRINTLN(F("). config.h pins."));
#endif
        } else {
            SERIAL_PRINT(F("RADIOLIB_ERR_NONE="));
            SERIAL_PRINTLN((int16_t)RADIOLIB_ERR_NONE);
        }
        SERIAL_PRINT(F("BUSY (GP")); SERIAL_PRINT(RADIO_BUSY); SERIAL_PRINT(F(") after fail: "));
        SERIAL_PRINTLN(digitalRead(RADIO_BUSY));
#endif
        return;
    }
    node = new LoRaWANNode(radio, &EU868, 0);
    /* EUIs: literal uint64_t (MSB/human-readable); keys: byte array MSB (big-endian). Copy from TTN. */
    uint64_t joinEui = LORAWAN_JOIN_EUI;
    uint64_t devEui64 = LORAWAN_DEV_EUI;
    static const uint8_t appKey[] = LORAWAN_APP_KEY;
    if (devEui64 == 0) {
        SERIAL_PRINTLN(F("ERROR: DevEUI is still zero. Check app-device-config.h (LORAWAN_DEV_EUI)."));
        return;
    }
    SERIAL_PRINTLN(F("Starting OTAA..."));
    int16_t beginOtaaRes = node->beginOTAA(joinEui, devEui64, nullptr, appKey);
    if (beginOtaaRes != RADIOLIB_ERR_NONE) {
        SERIAL_PRINT(F("OTAA Begin Failed: "));
        SERIAL_PRINTLN(beginOtaaRes);
        return;
    }
    if (haveStoredSession) {
#if RUN_MODE != RUN_MODE_PROD
        SERIAL_PRINTLN(F("Restore session: setBufferSession()..."));
#endif
        int16_t sessRes = node->setBufferSession(persistentData.sessionBuf);
#if RUN_MODE != RUN_MODE_PROD
        SERIAL_PRINT(F("setBufferSession() => "));
        SERIAL_PRINTLN(sessRes);
        SERIAL_PRINTLN(F("setBufferNonces()..."));
#endif
        int16_t nonceRes = node->setBufferNonces(persistentData.noncesBuf);
#if RUN_MODE != RUN_MODE_PROD
        SERIAL_PRINT(F("setBufferNonces() => "));
        SERIAL_PRINTLN(nonceRes);
#endif
        if (sessRes == RADIOLIB_ERR_NONE && nonceRes == RADIOLIB_ERR_NONE) {
            SERIAL_PRINTLN(F("Session restored"));
            joined = true;
        } else {
#if RUN_MODE != RUN_MODE_PROD
            SERIAL_PRINTLN(F("Session restore failed (discard or invalid)"));
#endif
        }
    }
    if (!joined) {
        SERIAL_PRINTLN(F("Attempting Join..."));
        int16_t res = node->activateOTAA();
        if (res != RADIOLIB_ERR_NONE) {
            SERIAL_PRINT(F("Join failed Error: "));
            SERIAL_PRINTLN(res);
            return;
        }
        joined = true;
        SERIAL_PRINTLN(F("Joined"));
        persistData();
        pendingHello = true;
    }
    sensors.begin();
    ramCount = 0;
    do_wake();
}

void loop() {
    if (!node || !joined) {
        delay(5000);
        return;
    }
    if (pendingHello) {
        static uint16_t helloRetries = 0;
        const char* hello = "HELLO WORLD";
        int16_t res = node->sendReceive((const uint8_t*)hello, strlen(hello), 2, false);
        if (res == RADIOLIB_ERR_NONE || res > 0) {
            SERIAL_PRINTLN(F("HELLO WORLD sent"));
            pendingHello = false;
            helloRetries = 0;
            persistData();
        } else if (++helloRetries >= 60) {
            SERIAL_PRINTLN(F("HELLO WORLD give up after 60 tries"));
            pendingHello = false;
            helloRetries = 0;
        }
        delay(100);
        return;
    }
    if (pendingSend) {
        do_send();
        delay(100);
        return;
    }
    delay((unsigned long)SLEEP_SECONDS * 1000UL);
    softwareEpoch = persistentData.rtcEpoch + (millis() - epochBaseMillis) / 1000;
    do_wake();
}
