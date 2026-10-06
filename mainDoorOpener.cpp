//
// Opens a door with a solenoid, triggered by an RFID card/tag or a web/REST request.
// See readme.md for hardware and operation details.
//

#include "iotsa.h"
#include "iotsaRFID.h"
#include "iotsaDoor.h"
#include "iotsaUser.h"

IotsaApplication application("Door Opening Server");
IotsaUserMod myAuthenticator(application, "admin");


// Instantiate the Door module, and install it in the framework
IotsaDoorMod doorMod(application);

// Instantiate the RFID module, and install it in the framework
IotsaRFIDMod rfidMod(application);

void openDoor(String& uid) {
  doorMod.openDoor();
}

void onSolenoidDeactivated() {
  rfidMod.scheduleReset();
}

// LED feedback for RFID mode changes, via iotsaStatus's status-pulse channel
// (cwi-dis/iotsa#176/#256) instead of ledMod.set(). IotsaRFIDMod calls
// modeChanged() -- and so showMode() -- at every real mode transition,
// including the return to card_idle (both the loop() timeout and an
// immediate add/remove success), so card_idle's clearStatusPulse() always
// lands exactly when the RFID module's own window really ends. The
// durations passed for card_add/card_remove below are just a safety cap in
// case that callback is ever skipped -- they don't need to match
// IotsaRFIDMod's real 5-second window exactly.
static const uint32_t kRfidPulseCapMs = 6000;

void showMode(cardMode mode) {
  if (mode == card_ok) {
    iotsaStatus.setStatusPulse(0x00ff00, 0, 0, 2000, "card ok");  // 2 seconds green
    IotsaSerial.println("showMode: card_ok");
  } else if (mode == card_bad) {
    iotsaStatus.setStatusPulse(0xff0000, 0, 0, 2000, "card bad");  // 2 seconds red
    IotsaSerial.println("showMode: card_bad");
  } else if (mode == card_add) {
    iotsaStatus.setStatusPulse(0x00ff00, 250, 250, kRfidPulseCapMs, "present card to add");  // green flashing
    IotsaSerial.println("showMode: card_add");
  } else if (mode == card_remove) {
    iotsaStatus.setStatusPulse(0xff0000, 250, 250, kRfidPulseCapMs, "present card to remove");  // red flashing
    IotsaSerial.println("showMode: card_remove");
  } else {
    iotsaStatus.clearStatusPulse(); // Resume normal iotsa status display
    IotsaSerial.println("showMode: iotsa status");
  }
}

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setAuth(&myAuthenticator);  // every module, the standard ones included, uses this
  application.setup();
  application.lateSetup();
  rfidMod.cardPresented = openDoor;
  rfidMod.modeChanged = showMode;
  doorMod.solenoidDeactivated = onSolenoidDeactivated;
  showMode(card_idle);
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
