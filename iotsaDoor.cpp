#include "iotsa.h"
#include "iotsaDoor.h"

// Implementation of the Door module
void IotsaDoorMod::setup() {
	pinMode(PIN_SOLENOID, OUTPUT);
  digitalWrite(PIN_SOLENOID, LOW);
  activateSolenoidUntil = 0;
}

void IotsaDoorMod::lateSetup() {
  api.setup("door", true, true);
  name = "door";
  changesNeedRight("door");
}

String IotsaDoorMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>See <a href=\"/door\">/door</a> for opening the door.</p>";
  return rv;
}

void IotsaDoorMod::getHandler(IotsaApiContext& ctx) {
  ctx.show("open", isOpen());
}

void IotsaDoorMod::putHandler(IotsaApiContext& ctx) {
  bool open = false;
  ctx.arg("open", open, IotsaApiContext::Transient);
  if (ctx.failed()) return;
  if (open) openDoor();
}

#ifdef IOTSA_WITH_WEB
void IotsaDoorMod::formHandler_args(IotsaWebContext& ctx) {
  if (!ctx.argAction("open", IotsaWebContext::Transient)) return;
  openDoor();
  ctx.feedback("Opened.");
}

void IotsaDoorMod::formHandler_fields(IotsaWebContext& ctx) {
  if (!ctx.includesEveryday()) return;
  ctx.showStatus("Door", "open", isOpen() ? "open" : "closed");
  ctx.showButton("open", "Open", IotsaWebContext::Transient);
}
#endif // IOTSA_WITH_WEB

void IotsaDoorMod::openDoor() {
  activateSolenoidUntil = millis() + solenoidActivationDuration;
}

void IotsaDoorMod::loop() {
  //IotsaSerial.println("sol in");
  // Activate or deactivate the solenoid.
  if (activateSolenoidUntil) {
    if (millis() > activateSolenoidUntil) {
      activateSolenoidUntil = 0;
      digitalWrite(PIN_SOLENOID, LOW);
      if (solenoidDeactivated) solenoidDeactivated();
    } else {
      digitalWrite(PIN_SOLENOID, HIGH);
    }
  }
  //IotsaSerial.println("sol out");
}
