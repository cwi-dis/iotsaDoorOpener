#ifndef _IOTSA_DOOR_H_
#define _IOTSA_DOOR_H_

#include "iotsa.h"

//
//  Pin for door module. Opens door with a solenoid.
//

#define PIN_SOLENOID 4

//
//  Door module. Opens door with a solenoid. No REST API: there is no
//  persistent config to expose (opening the door is a momentary trigger,
//  not a config change), so this stays a plain web endpoint only --
//  matches the REST-is-config-only convention used across the framework
//  (e.g. iotsaSmartMeter's IotsaP1Mod).
//

class IotsaDoorMod : public IotsaMod {
public:
  IotsaDoorMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL)
  : IotsaMod(_app, _auth),
    activateSolenoidUntil(0),
    solenoidActivationDuration(2000)
  {}
  void setup() override;
  void serverSetup() override;
  void loop() override;
  String info() override;
  void openDoor();
private:
  void handler();
  uint32_t activateSolenoidUntil;
  uint32_t solenoidActivationDuration;
};
#endif // _IOTSA_DOOR_H_
