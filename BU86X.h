/**
 * BU86X.h
 * =======
 * Arduino library for Brymen BU-86X USB HID infrared adapter.
 * Sits on top of EspUsbHost. Confirmed working on ESP32-S3.
 *
 * HARDWARE
 * --------
 *   ESP32-S3 USB OTG port (BOTTOM/native port) --> BU-86X --> DMM (ON, measuring)
 *   Serial Monitor via TOP port (CH343/UART)
 *
 * CONFIRMED PROTOCOL (Wireshark verified)
 * ----------------------------------------
 *   Trigger : {0x00, 0x86, 0x66} to interrupt OUT EP 0x01
 *   Response: 3 x 8 bytes = 24 bytes from interrupt IN EP 0x81
 *   Model ID: payload[16] == 0x86
 *
 * QUICK START
 * -----------
 *   #include <BU86X.h>
 *   BU86X dmm;
 *
 *   void setup() {
 *     Serial.begin(115200);
 *     dmm.begin();
 *   }
 *
 *   void loop() {
 *     dmm.loop();              // drives auto-trigger
 *     BU86X_Reading r;
 *     if (dmm.read(r)) {
 *       Serial.println(r.mainDisplay);  // e.g. "-12.34 mV DC"
 *       Serial.println(r.subDisplay);   // e.g. "60.11 Hz"
 *     }
 *   }
 *
 * DEPENDENCY
 * ----------
 *   EspUsbHost by tanakamasayuki — install from GitHub master ZIP:
 *   https://github.com/tanakamasayuki/EspUsbHost/archive/refs/heads/master.zip
 *
 * EspUsbHost PATCH REQUIRED
 * --------------------------
 *   See extras/PATCH_INSTRUCTIONS.md — one function edit in EspUsbHost.cpp.
 */

#pragma once
#include <Arduino.h>
#include <EspUsbHost.h>

#define BU86X_VID       0x0820
#define BU86X_PID       0x0001
#define BU86X_MODEL_ID  0x86
#define BU86X_PAYLOAD   24
#define BU86X_CHUNK     8

// ── Reading struct ────────────────────────────────────────────────
struct BU86X_Reading {
    char  mainDisplay[32];  // e.g. "-12.34 mV DC"  -- show this on screen
    char  subDisplay[32];   // e.g. "60.11 Hz"
    char  mainUnit[16];     // e.g. "mV DC"
    char  subUnit[16];      // e.g. "Hz"
    float mainValue;        // numeric, NAN if OL/blank
    float subValue;
    bool  mainAC, mainDC, mainOL, mainNeg;
    bool  subAC,  subDC,  subOL;
    bool  flag_auto, flag_max, flag_min, flag_avg, flag_hold, flag_rel;
    uint8_t raw[BU86X_PAYLOAD];
    bool  valid;            // true when raw[16] == 0x86
};

// ── Library class ─────────────────────────────────────────────────
class BU86X {
public:
    void    begin();
    bool    isConnected()           { return _connected; }
    bool    available()             { return _dataReady; }
    bool    read(BU86X_Reading &out);
    bool    trigger();
    void    setPollInterval(uint32_t ms) { _pollMs = ms; }
    void    loop();
    void    setDebug(bool on)       { _debug = on; }
    void    printReading(const BU86X_Reading &r);

    // internal — called by static EspUsbHost callbacks
    void _onChunk(const uint8_t *data, size_t len);
    void _onConnect(uint16_t vid, uint16_t pid, uint8_t addr);
    void _onDisconnect(uint16_t vid, uint16_t pid);

private:
    bool     _connected = false;
    bool     _dataReady = false;
    bool     _debug     = false;
    uint8_t  _devAddr   = 0;
    uint32_t _pollMs    = 500;
    uint32_t _lastTx    = 0;
    uint8_t  _rxBuf[BU86X_PAYLOAD] = {0};
    uint8_t  _rxLen     = 0;
    BU86X_Reading _latest = {};

    void   _decode(BU86X_Reading &r);
    static char  _seg7(uint8_t b);
    static float _toFloat(const char *digits, int dpPos, bool neg);
};

extern BU86X* _bu86x_instance;
