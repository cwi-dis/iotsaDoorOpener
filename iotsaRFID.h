#ifndef _IOTSA_RFID_H_
#define _IOTSA_RFID_H_

#include "iotsa.h"
#include "iotsaApi.h"

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

class IotsaRFIDMod : public IotsaApiMod {
public:
  IotsaRFIDMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL) : IotsaApiMod(_app, _auth) {}
  void setup();
  void serverSetup();
  void loop();
  String info();
  callbackFunc cardPresented;
  callbackFunc unknownCardPresented;
  modeCallbackFunc modeChanged;
protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
private:
  void configSave();
  void configLoad();
  void handler();
  void handleCard(String& uid);
};


#endif // _IOTSA_RFID_H_
