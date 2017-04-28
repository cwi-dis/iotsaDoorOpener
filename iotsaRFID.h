#ifndef _IOTSA_RFID_H_
#define _IOTSA_RFID_H_

#include <ESP.h>
#include "iotsa.h"

//
//  RFID module. Reads RFID cards.
//


//
//  Pins for RFID-RC522 module. Read RFID cards.
//

#define PIN_RFID_RESET  16
#define PIN_RFID_SDA    5
#define PIN_RFID_MOSI   13
#define PIN_RFID_MISO   12
#define PIN_RFID_SCK    14

//
// Interval to poll RFID reader. A read takes about 25ms, so don't do it every loop.
//
#define RFID_POLL_INTERVAL 200

// Current mode of operation.
typedef enum { card_idle, card_ok, card_bad, card_add, card_remove} cardMode;

typedef void (*callbackFunc)(String& uid);
typedef void (*modeCallbackFunc)(cardMode mode);

class IotsaRFIDMod : public IotsaMod {
public:
  IotsaRFIDMod(IotsaApplication &_app, IotsaAuthMod *_auth=NULL) : IotsaMod(_app, _auth) {}
  void setup();
  void serverSetup();
  void loop();
  String info();
  callbackFunc cardPresented;
  callbackFunc unknownCardPresented;
  modeCallbackFunc modeChanged;
private:
  void configSave();
  void configLoad();
  void handler();
  void handleCard(String& uid);
};


#endif // _IOTSA_RFID_H_
