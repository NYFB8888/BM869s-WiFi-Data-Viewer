/**
 * BU86X_SimpleTest.ino  v17 — Wireshark fully decoded
 * =====================================================
 * Wireshark raw packet decode:
 *
 * TRIGGER (host->device, EP 0x01, 3 bytes on wire):
 *   00 86 66
 *   ^^ = HID report ID (0x00)
 *      ^^ ^^ = command bytes
 *
 * RESPONSE (device->host, EP 0x81, 3 x 8 bytes):
 *   Chunk 1: 00 01 11 BE BF 7E DA F8
 *   Chunk 2: 01 00 BE BF BE BE 04 00
 *   Chunk 3: 86 86 86 86 00 00 00 00
 *             ^^ = model ID 0x86 at byte[0] of chunk 3 = byte[16] of full payload
 *
 * Serial: 115200 baud via TOP port. BU-86X: BOTTOM port, DMM ON.
 */

#include <EspUsbHost.h>

#define BU86X_VID       0x0820
#define BU86X_PID       0x0001
#define BU86X_MODEL_ID  0x86
#define BU86X_PAYLOAD   24

// 3 bytes on wire: report ID (0x00) + command (0x86 0x66)
static const uint8_t RTD[3] = { 0x00, 0x86, 0x66 };

EspUsbHost usb;
bool     connected = false;
uint8_t  devAddr   = 0;
uint8_t  rxBuf[32] = {0};
uint8_t  rxLen     = 0;
uint32_t pktCount  = 0;
uint32_t lastTxMs  = 0;
bool     gotData   = false;

void handleChunk(const uint8_t *data, size_t len, const char *src) {
    gotData = true;
    Serial.printf("[%s] len=%u  ", src, len);
    for (size_t i = 0; i < len; i++) {
        if (data[i] < 0x10) Serial.print('0');
        Serial.print(data[i], HEX);
        Serial.print(' ');
    }
    Serial.println();

    uint8_t toCopy = min((int)(BU86X_PAYLOAD - rxLen), (int)len);
    memcpy(rxBuf + rxLen, data, toCopy);
    rxLen += toCopy;

    if (rxLen >= BU86X_PAYLOAD) {
        pktCount++;
        Serial.printf("\n[PKT#%lu] ", pktCount);
        for (int i = 0; i < BU86X_PAYLOAD; i++) {
            if (rxBuf[i] < 0x10) Serial.print('0');
            Serial.print(rxBuf[i], HEX);
            Serial.print(' ');
        }
        Serial.println();
        if (rxBuf[16] == BU86X_MODEL_ID)
            Serial.println("*** byte[16]==0x86  LINK WORKS! ***\n");
        else
            Serial.printf("byte[16]=0x%02X (expected 0x86)\n\n", rxBuf[16]);
        rxLen = 0;
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n== BU-86X Test v17 ==");
    Serial.println("Trigger: {0x00, 0x86, 0x66} (3 bytes incl report ID)");

    usb.onDeviceConnected([](const EspUsbHostDeviceInfo &dev) {
        Serial.printf("[USB] Connected VID=0x%04X PID=0x%04X \"%s\"\n",
                      dev.vid, dev.pid, dev.product);
        if (dev.vid != BU86X_VID || dev.pid != BU86X_PID) return;
        connected = true;
        devAddr   = dev.address;
        rxLen     = 0;
        gotData   = false;
        Serial.println("[USB] BU-86X ready!");
    });

    usb.onDeviceDisconnected([](const EspUsbHostDeviceInfo &dev) {
        if (dev.vid == BU86X_VID && dev.pid == BU86X_PID) {
            Serial.println("[USB] Disconnected.");
            connected = false;
        }
    });

    usb.onHIDInput([](const EspUsbHostHIDInput &i) {
        if (i.vid != BU86X_VID || i.pid != BU86X_PID) return;
        handleChunk(i.data, i.length, "HID");
    });

    usb.onVendorInput([](const EspUsbHostVendorInput &i) {
        if (i.vid != BU86X_VID || i.pid != BU86X_PID) return;
        handleChunk(i.rawData, i.rawLength, "VND");
    });

    if (!usb.begin()) {
        Serial.printf("[ERR] begin() failed: %s\n", usb.lastErrorName());
        while (1) delay(1000);
    }
    Serial.println("[USB] Host started. Plug in BU-86X, DMM ON.");
}

void loop() {
    if (!connected) {
        Serial.println("[---] Waiting...");
        delay(3000);
        return;
    }

    if (millis() - lastTxMs >= 1000) {
        lastTxMs = millis();
        bool ok = usb.sendVendorOutput(RTD, sizeof(RTD), devAddr);
        Serial.printf("[TX] {00,86,66}: %s%s\n",
                      ok ? "OK" : "FAIL",
                      gotData ? "" : " (no RX yet)");
    }

    delay(50);
}


