#ifndef _IOTSA_DOOR_H_
#define _IOTSA_DOOR_H_

#include "iotsa.h"
#include "iotsaApi.h"

//
//  Pin for door module. Opens door with a solenoid.
//

#define PIN_SOLENOID 4

typedef void (*doorCallbackFunc)();

//
//  Door module. Opens the door with a solenoid, for a couple of seconds: on
//  the page /door (an Open button, or GET /door?open=1 from a script), with
//  PUT /api/door {"open": true}, or from another module (openDoor(), the
//  RFID cards). Opening needs the right "door"; whether it is open doesn't.
//

class IotsaDoorMod : public IotsaModule {
public:
  using IotsaModule::IotsaModule;
  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
  void openDoor();
  bool isOpen() { return activateSolenoidUntil != 0; }
  // Fired once, right after the solenoid switches back off. Lets another module
  // (e.g. IotsaRFIDMod, see cwi-dis/iotsaDoorOpener#7) react to the solenoid without
  // this module knowing that module exists - wiring happens in the main .cpp, same
  // as the existing rfidMod.cardPresented/modeChanged callbacks.
  doorCallbackFunc solenoidDeactivated = nullptr;
protected:
  void getHandler(IotsaApiContext& ctx) override;
  void putHandler(IotsaApiContext& ctx) override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Door"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
private:
  uint32_t activateSolenoidUntil = 0;
  uint32_t solenoidActivationDuration = 2000;
};
#endif // _IOTSA_DOOR_H_
