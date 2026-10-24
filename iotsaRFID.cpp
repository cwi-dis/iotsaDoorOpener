#include "iotsa.h"
#include "iotsaRFID.h"
#include "iotsaConfigFile.h"

// Helper function: convert RFID UID to string

static String strUid(MFRC522::Uid& uid) {
  String rv;
  for (int i=0; i<uid.size; i++) {
    char c1 = "0123456789ABCDEF"[((uid.uidByte[i] >> 4) & 0xf)];
    char c2 = "0123456789ABCDEF"[(uid.uidByte[i] & 0xf)];
    rv += c1;
    rv += c2;
  }
  return rv;
}

bool IotsaRFIDMod::lookupCard(const String& uid) {
  for (auto& c : cards) {
    if (c.uid == uid) return true;
  }
  return false;
}

void IotsaRFIDMod::handleAddCard(const String& uid) {
  if (lookupCard(uid)) return;
  cards.push_back(IotsaRFIDCard(uid));
  IOTSA_LOG("rfid", "added card %s", uid.c_str());
}

void IotsaRFIDMod::handleRemoveCard(const String& uid) {
  for (auto it = cards.begin(); it != cards.end(); ++it) {
    if (it->uid == uid) {
      cards.erase(it);
      IOTSA_LOG("rfid", "removed card %s", uid.c_str());
      return;
    }
  }
}

// A card in the API: its UID, which is also its key. Only set for a new card.
void IotsaRFIDCard::getHandler(IotsaApiContext& ctx) {
  ctx.show("uid", uid);
}

void IotsaRFIDCard::putHandler(IotsaApiContext& ctx) {
  if (uid == "") ctx.arg("uid", uid, IotsaApiContext::Saved);
}

#ifdef IOTSA_WITH_WEB
//
// A card on the web page: its UID in the table, and in the add form.
//
void IotsaRFIDCard::formHandler_args(IotsaWebContext& ctx) {
  ctx.arg("uid", uid, IotsaWebContext::Saved);
}

void IotsaRFIDCard::formHandler_fields(IotsaWebContext& ctx) {
  ctx.showField("Card ID", "uid", uid, IotsaWebContext::Saved, "hex, as shown for the last card");
}

void IotsaRFIDCard::formHandler_row(IotsaWebContext& ctx) {
  ctx.showCell("Card ID", uid);
}

#endif // IOTSA_WITH_WEB

// The ID as the reader reports it: upper case, no spaces.
bool IotsaRFIDCardList::accept(IotsaRFIDCard& card, IotsaContext& ctx) {
  card.uid.trim();
  card.uid.toUpperCase();
  if (card.uid == "") {
    ctx.error(400, "A new card needs its ID.");
    return false;
  }
  for (auto& c : _v) {
    if (c.uid == card.uid) {
      ctx.error(409, "Card " + card.uid + " is already known.");
      return false;
    }
  }
  return true;
}

//
// Implementation of the RFID module
//
void IotsaRFIDMod::setup() {
  SPI.begin();
  mfrc522.PCD_Init();
  chipVersion = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
  // 0x00 and 0xFF are not valid MFRC522 firmware versions - both are what a floating/dead
  // SPI bus reads back, i.e. no chip actually answering (unplugged, unpowered, miswired).
  chipPresent = (chipVersion != 0x00 && chipVersion != 0xFF);
  if (chipPresent) {
    IOTSA_LOG("rfid", "MFRC522 VersionReg=0x%x", chipVersion);
  } else {
    IOTSA_LOG("rfid", "MFRC522 not detected (VersionReg=0x%x), check reader wiring/power", chipVersion);
  }
  configLoad();
}

void IotsaRFIDMod::scheduleReset() {
  if (resetAfterSolenoidMs == 0) return;
  pendingResetAtMillis = millis() + resetAfterSolenoidMs;
}

void IotsaRFIDMod::resetChip() {
  IOTSA_LOG("rfid", "resetChip requested");
  mfrc522.PCD_Init();
  resetCount++;
  lastResetTime = millis();
  IOTSA_LOG("rfid", "resetChip done, VersionReg=0x%x", mfrc522.PCD_ReadRegister(MFRC522::VersionReg));
}

#ifdef IOTSA_WITH_WEB
void IotsaRFIDMod::formHandler_args(IotsaWebContext& ctx) {
  ctx.arg("addCard", addCard, IotsaWebContext::Saved);
  ctx.arg("removeCard", removeCard, IotsaWebContext::Saved);
  ctx.arg("resetAfterSolenoidMs", resetAfterSolenoidMs, IotsaWebContext::Saved);
  IotsaWebContext::Scope s(ctx, "cards");
  cardList.formHandler_args(ctx);
}

// The page is built in memory and then sent in pieces, so a long list of
// cards is in memory once (cwi-dis/iotsaDoorOpener#1, cwi-dis/iotsa@2448219).
void IotsaRFIDMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.includesEveryday()) {
    if (lastCard == "") {
      ctx.showStatus("Last card", "lastCard", "none since power-on");
    } else {
      String last = lastCard + (lastCardKnown ? "" : " (unknown)");
      last += ", " + String((millis()-lastCardReadTime)/1000) + " seconds ago";
      ctx.showStatus("Last card", "lastCard", last);
    }
    return;
  }
  {
    IotsaWebContext::Form form(ctx, "Save", "Master cards");
    ctx.showField("Master addition card", "addCard", addCard, IotsaWebContext::Saved, "presenting it, then a new card, adds that card");
    ctx.showField("Master removal card", "removeCard", removeCard, IotsaWebContext::Saved, "presenting it, then a known card, removes that card");
    IotsaContext::RightsScope rights(ctx);
    ctx.showsNeedRight(IOTSA_RIGHT_NONE);
    ctx.showField("Reset reader after door", "resetAfterSolenoidMs", resetAfterSolenoidMs, IotsaWebContext::Saved, "ms after the solenoid switches off, 0: never");
  }
  ctx << "<h3>Known cards</h3>\n";
  IotsaWebContext::Scope s(ctx, "cards");
  cardList.formHandler_fields(ctx);
}
#endif // IOTSA_WITH_WEB

// Everything needs the right "rfid" (see lateSetup()), except the reader's
// health: resetAfterSolenoidMs and the telemetry, for debugging a reader at
// a distance (cwi-dis/iotsaDoorOpener#4, #7).
void IotsaRFIDMod::getHandler(IotsaApiContext& ctx) {
  ctx.show("addCard", addCard);
  ctx.show("removeCard", removeCard);
  ctx.show("lastCard", lastCard);
  if (lastCard != "") {
    ctx.show("lastCardPresented", (millis()-lastCardReadTime)/1000);
    ctx.show("lastCardType", lastCardType);
  }
  cardList.getHandler(ctx);

  ctx.showsNeedRight(IOTSA_RIGHT_NONE);
  // 0 (default) disables auto-recovery: resetChip() only runs on an explicit resetChip
  // PUT (cwi-dis/iotsaDoorOpener#7). Nonzero opts in to firing it automatically that many
  // ms after the door solenoid deactivates (wired in the main .cpp via
  // IotsaDoorMod::solenoidDeactivated -> scheduleReset()).
  ctx.show("resetAfterSolenoidMs", resetAfterSolenoidMs);

  // General RFID telemetry, for remote debugging of read failures (cwi-dis/iotsaDoorOpener#4)
  JsonObject telemetry = ctx.reply()["telemetry"].to<JsonObject>();
  // Live register read, not the cached setup()-time value: a chip that was present at
  // boot can still go unresponsive later, and we want telemetry to reflect that.
  uint8_t liveChipVersion = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
  bool liveChipPresent = (liveChipVersion != 0x00 && liveChipVersion != 0xFF);
  telemetry["chipVersion"] = liveChipVersion;
  telemetry["chipPresent"] = liveChipPresent;
  telemetry["chipPresentAtBoot"] = chipPresent;
  if (liveChipPresent) {
    // Only meaningful when a real chip is answering: on a disconnected/dead bus every
    // register reads back 0xFF, which masks to a plausible-looking but bogus 0x70.
    telemetry["antennaGain"] = mfrc522.PCD_GetAntennaGain();
    // TxControlReg bits 0/1 (Tx1RFEn/Tx2RFEn) - whether the antenna drivers are actually
    // enabled. Distinct from antennaGain above, which is RFCfgReg (receiver gain, not
    // transmit driver enable). A reset (e.g. a glitch on the MFRC522's NRSTPD line) leaves
    // these disabled until PCD_AntennaOn()/PCD_Init() re-enables them - directly tests the
    // "reader alive at the register level but antenna off" hypothesis (cwi-dis/iotsaDoorOpener#7).
    telemetry["antennaOn"] = (mfrc522.PCD_ReadRegister(MFRC522::TxControlReg) & 0x03) == 0x03;
  }
  if (everAttemptedRead) {
    telemetry["lastAttemptAgo"] = (millis()-lastAttemptTime)/1000;
    telemetry["lastAttemptOk"] = lastAttemptOk;
    telemetry["lastAttemptStatus"] = String(MFRC522::GetStatusCodeName(lastAttemptStatus));
    if (!lastAttemptOk) {
      telemetry["lastAttemptErrorReg"] = lastAttemptErrorReg;
      telemetry["lastAttemptCollReg"] = lastAttemptCollReg;
    }
  }
  if (resetCount > 0) {
    telemetry["resetCount"] = resetCount;
    telemetry["lastResetAgo"] = (millis()-lastResetTime)/1000;
  }
}

// cards is a collection (cwi-dis/iotsa#309): an array replaces the whole list
// (a backup restored), {"<uid>": {}} adds a card, null removes it. resetChip
// is an action.
void IotsaRFIDMod::putHandler(IotsaApiContext& ctx) {
  ctx.arg("addCard", addCard, IotsaApiContext::Saved);
  ctx.arg("removeCard", removeCard, IotsaApiContext::Saved);
  ctx.arg("resetAfterSolenoidMs", resetAfterSolenoidMs, IotsaApiContext::Saved);
  cardList.putHandler(ctx);
  bool reset = false;
  ctx.arg("resetChip", reset, IotsaApiContext::Transient);
  if (ctx.failed()) return;
  // Re-initializes the MFRC522 without a full device reboot (cwi-dis/iotsaDoorOpener#7).
  if (reset) resetChip();
}

void IotsaRFIDMod::lateSetup() {
  // The /rfid page is registered by api.setup() below (webPage defaults to true).
  name = "rfid";
  api.setup("rfid", true, true);
  // Card UIDs are as good as keys: reading and changing anything here needs
  // the right "rfid", unless a handler says otherwise (the reader's health).
  readsNeedRight("rfid");
  changesNeedRight("rfid");
}

String IotsaRFIDMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>See <a href=\"/rfid\">/rfid</a> for rfid management. Api available at <a href=\"/api/rfid\">/api/rfid</a>.</p>";
  return rv;
}

void IotsaRFIDMod::loop() {
  if (pendingResetAtMillis && millis() > pendingResetAtMillis) {
    pendingResetAtMillis = 0;
    resetChip();
  }

  if (!chipPresent) return; // No point polling a chip that wasn't there at setup()

  static uint32_t lastPoll;
  uint32_t now = millis();

  if (now > lastPoll && now < lastPoll + RFID_POLL_INTERVAL)
    return;
  lastPoll = now;
  //IotsaSerial.println("rfid in");
  if (curMode != card_idle && millis() > curModeEndTime) {
    // No card present for 2 seconds: revert to idle
    IOTSA_LOG_DEBUG("rfid", "mode timeout");
    curModeEndTime = 0;
    curMode = card_idle;
    if (modeChanged) modeChanged(curMode);
  }
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  // PICC_ReadCardSerial() is a thin wrapper around PICC_Select() that discards the
  // StatusCode. Call PICC_Select() directly so a failed read (previously silent,
  // cwi-dis/iotsaDoorOpener#4) can be logged and exposed over /api/rfid.
  MFRC522::StatusCode selectStatus = mfrc522.PICC_Select(&mfrc522.uid);
  everAttemptedRead = true;
  lastAttemptTime = millis();
  lastAttemptStatus = selectStatus;
  lastAttemptOk = (selectStatus == MFRC522::STATUS_OK);
  if (!lastAttemptOk) {
    lastAttemptErrorReg = mfrc522.PCD_ReadRegister(MFRC522::ErrorReg);
    lastAttemptCollReg = mfrc522.PCD_ReadRegister(MFRC522::CollReg);
    IOTSA_LOG("rfid", "card present but read failed: %s", MFRC522::GetStatusCodeName(selectStatus));
    return;
  }
  String newCard = strUid(mfrc522.uid);
  lastCardType = String(MFRC522::PICC_GetTypeName(MFRC522::PICC_GetType(mfrc522.uid.sak)));
  IOTSA_LOG("rfid", "card %s, type %s", newCard.c_str(), lastCardType.c_str());
  handleCard(newCard);
}

void IotsaRFIDMod::handleCard(String& newCard) {
  lastCard = newCard;
  lastCardReadTime = millis();
  lastCardKnown = lookupCard(newCard);
  // First check if we should add or remove this card
  if (curMode == card_add && !lastCardKnown) {
    handleAddCard(newCard);
    configSave();
    lastCardKnown = true;
    curMode = card_idle;
    if (modeChanged) modeChanged(curMode);
    return;
  }
  if (curMode == card_remove && lastCardKnown) {
    handleRemoveCard(newCard);
    configSave();
    lastCardKnown = false;
    curMode = card_idle;
    if (modeChanged) modeChanged(curMode);
    return;
  }
  cardMode newMode = card_bad;
  if (newCard == addCard) newMode = card_add;
  else if (newCard == removeCard) newMode = card_remove;
  else if (lastCardKnown) newMode = card_ok;
  if (newMode != curMode) {
    IOTSA_LOG_DEBUG("rfid", "mode %d", int(newMode));
    curMode = newMode;
    curModeEndTime = millis() + 2000;
    if (newMode == card_add || newMode == card_remove) {
      curModeEndTime = millis() + 5000;
    }
    if (modeChanged) modeChanged(curMode);
    if (curMode == card_ok && cardPresented) cardPresented(newCard);
    if (curMode == card_bad && unknownCardPresented) unknownCardPresented(newCard);
  }
}

void IotsaRFIDMod::configLoad() {
  IotsaConfigFileLoad cf("/config/rfid.cfg");
  cf.get("addCard", addCard, "");
  cf.get("removeCard", removeCard, "");
  cf.get("resetAfterSolenoidMs", resetAfterSolenoidMs, (uint32_t)0);
  int idx=1;
  cards.clear();
  while(1) {
    String newCard;
    String name = "card"+String(idx);
    cf.get(name, newCard, "");
    if (newCard == "") break;
    cards.push_back(IotsaRFIDCard(newCard));
    idx++;
  }
  IOTSA_LOG_DEBUG("rfid", "%d cards loaded", idx-1);
}

void IotsaRFIDMod::configSave() {
  IotsaConfigFileSave cf("/config/rfid.cfg");
  cf.put("addCard", addCard);
  cf.put("removeCard", removeCard);
  cf.put("resetAfterSolenoidMs", resetAfterSolenoidMs);
  int idx=1;
  for (auto it=cards.begin(); it != cards.end(); it++, idx++) {
    String name = "card"+String(idx);
    cf.put(name, it->uid);
  }
  IOTSA_LOG("rfid", "%d cards saved", idx-1);
}

