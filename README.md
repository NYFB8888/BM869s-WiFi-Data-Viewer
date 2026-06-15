# BU86X Arduino Library

Reads live measurements from Brymen BM52x / BM82x / BM86x multimeters
via the BU-86X infrared USB adapter on ESP32-S3. Includes a WebSocket
gateway example for a large-digit browser display.

## Hardware

```
DMM  <--IR-->  BU-86X  <--USB-->  ESP32-S3 (OTG/bottom port)
                                        |
                                   WiFi network
                                        |
                                   Browser (big digits)
```

**Important:** Use the BOTTOM USB port (native OTG) for the BU-86X.
Use the TOP USB port (CH343/UART) for Serial Monitor and flashing.

## Dependencies

1. **EspUsbHost** by tanakamasayuki — install from GitHub master (NOT Library Manager):
   `https://github.com/tanakamasayuki/EspUsbHost/archive/refs/heads/master.zip`
   Arduino IDE → Sketch → Include Library → Add .ZIP Library

2. **ESPAsyncWebServer** — for the WebSocket example only:
   `https://github.com/ESP-NOW/ESPAsyncWebServer`

3. **AsyncTCP** — required by ESPAsyncWebServer:
   `https://github.com/ESP-NOW/AsyncTCP`

## EspUsbHost patch (required)

`sendVendorOutput()` in EspUsbHost has a bug that causes EP0 STALLs.
See `extras/PATCH_INSTRUCTIONS.md` for the exact edit (one function, ~15 lines).

## Board settings

```
Board:          ESP32S3 Dev Module
USB Mode:       USB-OTG (TinyUSB)
Upload Mode:    USB-OTG (TinyUSB)
```

## Quick start

```cpp
#include <BU86X.h>
BU86X dmm;

void setup() {
    Serial.begin(115200);
    dmm.begin();
}

void loop() {
    dmm.loop();           // drives 500ms auto-trigger
    BU86X_Reading r;
    if (dmm.read(r)) {
        Serial.println(r.mainDisplay);   // e.g. "12.34 mV DC"
        Serial.println(r.subDisplay);    // e.g. "60.11 Hz"
        Serial.println(r.mainValue);     // float
    }
}
```

## Protocol (Wireshark verified)

| | Value |
|---|---|
| USB VID/PID | 0x0820 / 0x0001 |
| Product name | "Superior DMM" |
| Trigger | `{0x00, 0x86, 0x66}` to interrupt OUT EP 0x01 |
| Response | 3 × 8 bytes = 24 bytes from interrupt IN EP 0x81 |
| Model ID | `payload[16] == 0x86` |

## BU86X_Reading fields

| Field | Type | Description |
|---|---|---|
| `mainDisplay` | char[32] | Ready string e.g. `"-12.34 mV DC"` |
| `subDisplay` | char[32] | Secondary display e.g. `"60.11 Hz"` |
| `mainValue` | float | Numeric value (NAN if OL) |
| `subValue` | float | Secondary numeric value |
| `mainUnit` | char[16] | Unit string e.g. `"mV DC"` |
| `subUnit` | char[16] | Secondary unit |
| `mainAC/DC/OL/Neg` | bool | Main display flags |
| `flag_auto/max/min/avg/hold/rel` | bool | Mode flags |
| `raw[24]` | uint8_t | Raw payload bytes |
| `valid` | bool | True when `raw[16] == 0x86` |

## Examples

- **BasicTest** — Serial output of live readings
- **WebSocketServer** — WiFi gateway with large-digit browser display
