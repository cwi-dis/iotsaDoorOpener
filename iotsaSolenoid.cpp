#include <ESP.h>
#include "iotsa.h"
#include "iotsaSolenoid.h"

uint32_t activateSolenoidUntil;
uint32_t solenoidActivationDuration = 2000;

// Implementation of the Door module
void IotsaDoorMod::setup() {
	pinMode(PIN_SOLENOID, OUTPUT);
  digitalWrite(PIN_SOLENOID, LOW);
  activateSolenoidUntil = 0;
}

void
IotsaDoorMod::handler() {
  // Handles the page that is specific to the Door module, activates the solenoid
  // for a second.
  if (needsAuthentication()) {
    return;
  }
  for (uint8_t i=0; i<server.args(); i++){
    if( server.argName(i) == "open") {
      if (server.arg(i).toInt() > 0) {
        activateSolenoidUntil = millis() + solenoidActivationDuration;
      } else {
        activateSolenoidUntil = 0;
      }
    }
  }
  String message = "<html><head><title>Door Server</title></head><body><h1>Door Server</h1>";
  message += "<form method='get'>Sesame: <input name='open' value='1' type='hidden'><input type='submit' value='Open'></form></body></html>";
  server.send(200, "text/html", message);
}

bool IotsaDoorMod::postHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool open = request["open"].as<bool>();
  if (open) {
    activateSolenoidUntil = millis() + solenoidActivationDuration;
    return true;
  }
  return false;
}


void IotsaDoorMod::serverSetup() {
  // Setup the web server hooks for this module.
  server.on("/door", std::bind(&IotsaDoorMod::handler, this));
  api.setup("/api/door", false, false, true);
  name = "door";
}

String IotsaDoorMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>See <a href=\"/door\">/door</a> for opening the door. REST API available at <a href=\"/api/door\">/api/door</a>.</p>";
  return rv;
}

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
    } else {
      digitalWrite(PIN_SOLENOID, HIGH);
    }
  }
  //IotsaSerial.println("sol out");
}
