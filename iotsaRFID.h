#ifndef _IOTSA_RFID_H_
#define _IOTSA_RFID_H_

#include "iotsa.h"
#include "iotsaApi.h"
#include "MFRC522.h"
#include <set>

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
  IotsaRFIDMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL)
  : IotsaApiMod(_app, _auth),
    mfrc522(PIN_RFID_SDA, PIN_RFID_RESET),
    lastCardReadTime(0),
    lastCardKnown(false),
    curMode(card_idle),
    curModeEndTime(0),
    chipVersion(0),
    chipPresent(false),
    everAttemptedRead(false),
    lastAttemptTime(0),
    lastAttemptOk(false),
    lastAttemptStatus(MFRC522::STATUS_OK),
    lastAttemptErrorReg(0),
    lastAttemptCollReg(0),
    resetCount(0),
    lastResetTime(0)
  {}
  void setup() override;
  void serverSetup() override;
  void loop() override;
  String info() override;
  // Re-initializes the MFRC522 chip (soft reset + reconfigure + antenna back on),
  // without touching the setup()-time chipPresent/chipVersion snapshot. Callable
  // manually over /api/rfid, or by another module (e.g. after the door solenoid
  // disengages) to recover from the reader going "deaf" (cwi-dis/iotsaDoorOpener#7).
  void resetChip();
  callbackFunc cardPresented;
  callbackFunc unknownCardPresented;
  modeCallbackFunc modeChanged;
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
private:
  void configSave() override;
  void configLoad() override;
  void handler();
  void handleCard(String& uid);
  bool lookupCard(const String& uid);
  void handleAddCard(const String& uid);
  void handleRemoveCard(const String& uid);

  MFRC522 mfrc522;
  String addCard;
  String removeCard;
  String lastCard;
  String lastCardType;
  uint32_t lastCardReadTime;
  bool lastCardKnown;
  std::set<String> normalCards;
  cardMode curMode;
  uint32_t curModeEndTime;

  // Chip/read telemetry, exposed over /api/rfid for remote debugging (cwi-dis/iotsaDoorOpener#4)
  uint8_t chipVersion;
  bool chipPresent; // false if VersionReg read as 0x00 or 0xFF at setup(): no chip answering on SPI
  bool everAttemptedRead;
  uint32_t lastAttemptTime;
  bool lastAttemptOk;
  MFRC522::StatusCode lastAttemptStatus;
  uint8_t lastAttemptErrorReg;
  uint8_t lastAttemptCollReg;
  uint32_t resetCount;
  uint32_t lastResetTime;
};


#endif // _IOTSA_RFID_H_
