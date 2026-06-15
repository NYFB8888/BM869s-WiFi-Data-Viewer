/**
 * BasicTest.ino
 * =============
 * Minimal example — connect BU-86X and print readings to Serial.
 *
 * Board settings (Tools menu):
 *   Board:       ESP32S3 Dev Module (or your board)
 *   USB Mode:    USB-OTG (TinyUSB)
 *   Upload Mode: USB-OTG (TinyUSB)
 *
 * Wiring:
 *   BOTTOM USB port (OTG/native) --> BU-86X --> DMM (ON, measuring)
 *   TOP USB port (CH343/UART)    --> PC (Serial Monitor, 115200 baud)
 */

#include <BU86X.h>

BU86X dmm;

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println("\nBU-86X Basic Test");
    Serial.println("Plug BU-86X into OTG port, DMM ON.\n");
    dmm.begin();
}

void loop() {
    dmm.loop();             // drives the 500ms auto-trigger

    BU86X_Reading r;
    if (dmm.read(r)) {
        dmm.printReading(r);
    }

    if (!dmm.isConnected()) {
        Serial.println("Waiting for BU-86X...");
        delay(2000);
    }
}
