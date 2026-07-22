#ifndef _IOTSA_DOOR_H_
#define _IOTSA_DOOR_H_

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
  IotsaDoorMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL)
  : IotsaApiMod(_app, _auth),
    activateSolenoidUntil(0),
    solenoidActivationDuration(2000)
  {}
  void setup() override;
  void serverSetup() override;
  void loop() override;
  String info() override;
  void openDoor();
private:
  bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  void handler();
  uint32_t activateSolenoidUntil;
  uint32_t solenoidActivationDuration;
};
#endif // _IOTSA_DOOR_H_
