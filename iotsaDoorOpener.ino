//
// Boilerplate for configurable web server (probably RESTful) running on ESP8266.
//
// This server includes the wifi configuration module, and optionally the
// Over-The-Air update module (to allow uploading new code into the esp12 (or other
// board) from the Arduino IDE.
//
//

#include <ESP.h>
#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaRFID.h"
#include "iotsaFilesBackup.h"
#include "iotsaOta.h"
#include "iotsaLed.h"
#include "iotsaSolenoid.h"
#include "iotsaUser.h"

// CHANGE: Add application includes and declarations here

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.
#define NEO_PIN 15  // Pin where neopixel led is attached

ESP8266WebServer server(80);
IotsaApplication application(server, "Door Opening Server");
IotsaUserMod myAuthenticator(application, "admin");

IotsaWifiMod wifiMod(application, &myAuthenticator);
IotsaFilesBackupMod filesBackupMod(application, &myAuthenticator);
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
    IotsaSerial.println("2 seconds green");
  } else if (mode == card_bad) {
    ledMod.set(0xff0000, 2000, 0, 1);  // 2 seconds red
    IotsaSerial.println("2 seconds red");
  } else if (mode == card_add) {
    ledMod.set(0x00ff00, 250, 250, 8);  // 2 seconds green flashing
    IotsaSerial.println("2 seconds green flashing");
  } else if (mode == card_remove) {
    ledMod.set(0xff0000, 250, 250, 8);  // 2 seconds red flashing
    IotsaSerial.println("2 seconds red flashing");
  } else {
    ledMod.showStatus(); // Short flashes to show module status/mode
    IotsaSerial.println("showing status");
  }
}

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
  ESP.wdtEnable(WDTO_120MS);
  rfidMod.cardPresented = openDoor;
  rfidMod.modeChanged = showMode;
  showMode(card_idle);
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}

