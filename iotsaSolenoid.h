#ifndef _IOTSA_SOLENIOD_H_
#define _IOTSA_SOLENIOD_H_

#include <ESP.h>
#include "iotsa.h"

//
//  Pin for door module. Opens door with a solenoid.
//

#define PIN_SOLENOID 4

//
//  Door module. Opens door with a solenoid.
//

class IotsaDoorMod : public IotsaMod {
public:
  IotsaDoorMod(IotsaApplication &_app, IotsaAuthMod *_auth=NULL) : IotsaMod(_app, _auth) {}
	void setup();
	void serverSetup();
	void loop();
  String info();
  void openDoor();
private:
  void handler();
};
#endif // _IOTSA_SOLENIOD_H_
