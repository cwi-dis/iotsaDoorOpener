#ifndef _IOTSA_SOLENIOD_H_
#define _IOTSA_SOLENIOD_H_

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
  IotsaDoorMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL) : IotsaApiMod(_app, _auth) {}
  void setup() override;
  void serverSetup() override;
  void loop() override;
  String info() override;
  void openDoor();
private:
  bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  void handler();
};
#endif // _IOTSA_SOLENIOD_H_
