#ifndef _IOTSA_SOLENIOD_H_
#define _IOTSA_SOLENIOD_H_

#include <ESP.h>
#include "iotsa.h"
#include "iotsaApi.h"

//
//  Pin for door module. Opens door with a solenoid.
//

#define PIN_SOLENOID 4

//
//  Door module. Opens door with a solenoid.
//

class IotsaDoorMod : public IotsaApiMod {
public:
  IotsaDoorMod(IotsaApplication &_app, IotsaAuthMod *_auth=NULL) : IotsaApiMod(_app, _auth) {}
	void setup();
	void serverSetup();
	void loop();
  String info();
  void openDoor();
  using IotsaBaseMod::needsAuthentication;
  bool needsAuthentication(const char *object, const char *verb) { if (auth==NULL) IotsaSerial.println("xxxjack IotsaBaseMod::needsAuthentication(2arg): no auth"); return auth ? auth->needsAuthentication(object, verb) : false; };
private:
  bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply);
  void handler();
};
#endif // _IOTSA_SOLENIOD_H_
