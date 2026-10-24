#ifndef _IOTSA_RFID_H_
#define _IOTSA_RFID_H_

#include "iotsa.h"
#include "iotsaApi.h"
#include "MFRC522.h"
#include <vector>

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

//
// A card that opens the door: its UID, as the reader reports it (hex,
// upper case). An object so the cards are a collection, with Add and
// Remove, on the page and in the API: {"uid": "..."} (cwi-dis/iotsa#309).
//
class IotsaRFIDCard : public IotsaApiModObject {
public:
  IotsaRFIDCard() {}
  IotsaRFIDCard(const String& _uid) : uid(_uid) {}
  String uid;
  // Not a setting of its own: the module saves all cards, as card1..cardN.
  bool configLoad(IotsaConfigFileLoad& cf, const String& name) override { return false; }
  void configSave(IotsaConfigFileSave& cf, const String& name) override {}
  void getHandler(IotsaApiContext& ctx) override;
  void putHandler(IotsaApiContext& ctx) override;
#ifdef IOTSA_WITH_WEB
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
  void formHandler_row(IotsaWebContext& ctx) override;
#endif
};

//
// The known cards, "cards": a table with Remove per card and an Add form on
// the page, the collection's shape in the API, keyed by UID. A new card is
// refused when it is already known.
//
class IotsaRFIDCardList : public IotsaVector<IotsaRFIDCard> {
public:
  IotsaRFIDCardList(std::vector<IotsaRFIDCard>& cards)
  : IotsaVector<IotsaRFIDCard>("cards", cards, Add | Remove, Table, IotsaContext::Saved)
  {}
  String key(int i) override { return _v[i].uid; }
  const char *keyField() override { return "uid"; }
protected:
  bool accept(IotsaRFIDCard& card, IotsaContext& ctx) override;
};

class IotsaRFIDMod : public IotsaModule {
public:
  IotsaRFIDMod(IotsaApplication &_app)
  : IotsaModule(_app),
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
    lastResetTime(0),
    resetAfterSolenoidMs(0),
    pendingResetAtMillis(0)
  {}
  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
  // Re-initializes the MFRC522 chip (soft reset + reconfigure + antenna back on),
  // without touching the setup()-time chipPresent/chipVersion snapshot. Callable
  // manually over /api/rfid, or by another module (e.g. after the door solenoid
  // disengages) to recover from the reader going "deaf" (cwi-dis/iotsaDoorOpener#7).
  void resetChip();
  // Requests a resetChip() call resetAfterSolenoidMs milliseconds from now (handled
  // non-blockingly in loop(), same style as curModeEndTime below). No-op if
  // resetAfterSolenoidMs is 0 (the default - opt-in via config). Callable from another
  // module (e.g. IotsaDoorMod::solenoidDeactivated, wired up in the main .cpp) without
  // that module needing to know anything about RFID/resetChip() itself.
  void scheduleReset();
  callbackFunc cardPresented;
  callbackFunc unknownCardPresented;
  modeCallbackFunc modeChanged;
protected:
  void getHandler(IotsaApiContext& ctx) override;
  void putHandler(IotsaApiContext& ctx) override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "RFID cards"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
private:
  void configSave() override;
  void configLoad() override;
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
  std::vector<IotsaRFIDCard> cards;
  IotsaRFIDCardList cardList{cards};
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

  // Config-gated auto-recovery after a solenoid activation (cwi-dis/iotsaDoorOpener#7).
  uint32_t resetAfterSolenoidMs; // 0 = disabled, persisted config
  uint32_t pendingResetAtMillis; // 0 = none pending
};


#endif // _IOTSA_RFID_H_
