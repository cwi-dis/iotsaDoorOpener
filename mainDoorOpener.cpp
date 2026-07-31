//
// Opens a door with a solenoid, triggered by an RFID card/tag or a web/REST request.
// See readme.md for hardware and operation details.
//

#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaRFID.h"
#include "iotsaOta.h"
#include "iotsaLed.h"
#include "iotsaDoor.h"
#include "iotsaUser.h"

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.
#define NEO_PIN 15  // Pin where neopixel led is attached

IotsaApplication application("Door Opening Server");
IotsaUserMod myAuthenticator(application, "admin");

IotsaWifiMod wifiMod(application, &myAuthenticator);
IotsaOtaMod otaMod(application, &myAuthenticator);
IotsaLedMod ledMod(application, NEO_PIN);

// Instantiate the Door module, and install it in the framework
IotsaDoorMod doorMod(application, &myAuthenticator);

// Instantiate the RFID module, and install it in the framework
IotsaRFIDMod rfidMod(application, &myAuthenticator);

void openDoor(String& uid) {
  doorMod.openDoor();
}

void showMode(cardMode mode) {
  if (mode == card_ok) {
    ledMod.set(0x00ff00, 2000, 0, 1);  // 2 seconds green
    IotsaSerial.println("showMode: card_ok");
  } else if (mode == card_bad) {
    ledMod.set(0xff0000, 2000, 0, 1);  // 2 seconds red
    IotsaSerial.println("showMode: card_bad");
  } else if (mode == card_add) {
    ledMod.set(0x00ff00, 250, 250, 8);  // 2 seconds green flashing
    IotsaSerial.println("showMode: card_add");
  } else if (mode == card_remove) {
    ledMod.set(0xff0000, 250, 250, 8);  // 2 seconds red flashing
    IotsaSerial.println("showMode: card_remove");
  } else {
    ledMod.showStatus(); // Short flashes to show module status/mode
    IotsaSerial.println("showMode: iotsa status");
  }
}

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
#ifndef ESP32
  ESP.wdtEnable(WDTO_120MS);
#endif
  rfidMod.cardPresented = openDoor;
  rfidMod.modeChanged = showMode;
  showMode(card_idle);
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
