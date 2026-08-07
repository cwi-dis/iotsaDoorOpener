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
  auto it = normalCards.find(uid);
  bool ok = it != normalCards.end();
  return ok;
}

void IotsaRFIDMod::handleAddCard(const String& uid) {
  auto it = normalCards.find(uid);
  if (it == normalCards.end()) {
    normalCards.insert(uid);
    IotsaSerial.print("rfid: added card: ");
    IotsaSerial.println(uid);
  }
}

void IotsaRFIDMod::handleRemoveCard(const String& uid) {
  auto it = normalCards.find(uid);
  if (it != normalCards.end()) {
    normalCards.erase(it);
    IotsaSerial.print("rfid: Removed card: ");
    IotsaSerial.println(uid);
  }
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
    IotsaSerial.print("rfid: MFRC522 VersionReg=0x");
    IotsaSerial.println(chipVersion, HEX);
  } else {
    IotsaSerial.print("rfid: MFRC522 not detected (VersionReg=0x");
    IotsaSerial.print(chipVersion, HEX);
    IotsaSerial.println("), check reader wiring/power");
  }
  configLoad();
}

void IotsaRFIDMod::scheduleReset() {
  if (resetAfterSolenoidMs == 0) return;
  pendingResetAtMillis = millis() + resetAfterSolenoidMs;
}

void IotsaRFIDMod::resetChip() {
  IotsaSerial.println("rfid: resetChip requested");
  mfrc522.PCD_Init();
  resetCount++;
  lastResetTime = millis();
  IotsaSerial.print("rfid: resetChip done, VersionReg=0x");
  IotsaSerial.println(mfrc522.PCD_ReadRegister(MFRC522::VersionReg), HEX);
}

void
IotsaRFIDMod::handler() {
  // Handles the page that is specific to the RFID module.
  if (needsAuthentication()) return;
  bool anyChanged = false;
  if (server->hasArg("addCard") && server->arg("addCard") != addCard) {
    anyChanged = true;
    addCard = server->arg("addCard");
    IotsaSerial.print("Set addCard: ");
    IotsaSerial.println(addCard);
  }
  if (server->hasArg("removeCard") && server->arg("removeCard") != removeCard) {
    anyChanged = true;
    removeCard = server->arg("removeCard");
    IotsaSerial.print("Set removeCard: ");
    IotsaSerial.println(removeCard);
  }
  if (server->hasArg("normalAdd") && server->arg("normalAdd") != "") {
    anyChanged = true;
    handleAddCard(server->arg("normalAdd"));
  }
  if (server->hasArg("normalRemove") && server->arg("normalRemove") != "") {
    anyChanged = true;
    handleRemoveCard(server->arg("normalRemove"));
  }
  if (anyChanged) configSave();

  // Stream the response instead of building one big in-core String: with enough
  // known cards (~30-40) that String overflowed available RAM and crashed the
  // device (cwi-dis/iotsaDoorOpener#1). Each sendContent() call only ever holds
  // one card's worth of HTML at a time, so this is bounded regardless of how
  // many cards are known.
#ifdef CONTENT_LENGTH_UNKNOWN
  server->setContentLength(CONTENT_LENGTH_UNKNOWN);
#endif
  server->send(200, "text/html");
  server->sendContent("<html><head><title>RFID Server</title></head><body><h1>RFID Server</h1>");
  if (lastCard != "") {
    String message = "<p>Last card was presented " + String((millis()-lastCardReadTime)/1000) + " seconds ago, Card ID " + lastCard;
    if (!lastCardKnown) message += " (unknown)";
    message += ".</p>";
    server->sendContent(message);
  } else {
    server->sendContent("<p>No card presented since last power-on.</p>");
  }

  String form = "<h2>Adding Cards</h2><form method='get'>Master addition card ID: <input name='addCard' value='";
  form += addCard;
  form += "'><br>Master removal card ID: <input name='removeCard' value='";
  form += removeCard;
  form += "'><br>Manually add normal card: <input name='normalAdd'><br><input type='submit'></form>";
  server->sendContent(form);

  server->sendContent("<h2>Known Cards</h2><ul>");
  for (auto it=normalCards.begin(); it != normalCards.end(); it++) {
    server->sendContent("<li>" + *it + "(<a href='/rfid?normalRemove=" + *it + "'>remove</a>)</li>");
  }
  server->sendContent("</ul></body></html>");
}

bool IotsaRFIDMod::getHandler(const char *path, JsonObject& reply) {
  reply["addCard"] = addCard;
  reply["removeCard"] = removeCard;
  // 0 (default) disables auto-recovery: resetChip() only runs on an explicit resetChip
  // PUT (cwi-dis/iotsaDoorOpener#7). Nonzero opts in to firing it automatically that many
  // ms after the door solenoid deactivates (wired in the main .cpp via
  // IotsaDoorMod::solenoidDeactivated -> scheduleReset()).
  reply["resetAfterSolenoidMs"] = resetAfterSolenoidMs;
  reply["lastCard"] = lastCard;
  if (lastCard != "") {
    reply["lastCardPresented"] = (millis()-lastCardReadTime)/1000;
    reply["lastCardType"] = lastCardType;
  }
  JsonArray rCards = reply["cards"].to<JsonArray>();
  for (auto value : normalCards) {
    rCards.add(value);
  }

  // General RFID telemetry, for remote debugging of read failures (cwi-dis/iotsaDoorOpener#4)
  JsonObject telemetry = reply["telemetry"].to<JsonObject>();
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
  return true;
}

bool IotsaRFIDMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  if (!request.is<JsonObject>()) return false;
  JsonObject reqObj = request.as<JsonObject>();
  bool any = false;
  if (getFromRequest<const char *>(reqObj, "addCard", addCard)) {
    any = true;
  }
  if (getFromRequest<const char *>(reqObj, "removeCard", removeCard)) {
    any = true;
  }
  if (getFromRequest<uint32_t>(reqObj, "resetAfterSolenoidMs", resetAfterSolenoidMs)) {
    any = true;
  }
  JsonArray newCards;
  if (getFromRequest<JsonArray>(reqObj, "cards", newCards)) {
    any = true;
    normalCards.clear();
    for (auto value: newCards) {
      normalCards.insert(value.as<String>());
    }
  }
  if (any) configSave();

  // Momentary action, not persisted config (matches IotsaConfigMod's "reboot" field):
  // re-initializes the MFRC522 without a full device reboot (cwi-dis/iotsaDoorOpener#7).
  if (reqObj["resetChip"]) {
    resetChip();
    any = true;
  }

  return any;
}

void IotsaRFIDMod::serverSetup() {
  // Setup the web server hooks for this module.
  server->on("/rfid", std::bind(&IotsaRFIDMod::handler, this));
  api.setup("/api/rfid", true, true);
  name = "rfid";
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
    IotsaSerial.println("rfid: mode timeout.");
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
    IotsaSerial.print("rfid: card present but read failed: ");
    IotsaSerial.println(MFRC522::GetStatusCodeName(selectStatus));
    return;
  }
  String newCard = strUid(mfrc522.uid);
  lastCardType = String(MFRC522::PICC_GetTypeName(MFRC522::PICC_GetType(mfrc522.uid.sak)));
  IotsaSerial.print("rfid: card read ok, uid=");
  IotsaSerial.print(newCard);
  IotsaSerial.print(" type=");
  IotsaSerial.println(lastCardType);
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
    IotsaSerial.print("rfid: mode: ");
    IotsaSerial.println(int(newMode));
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
  // xxxjack should use object interface
  normalCards.clear();
  while(1) {
    String newCard;
    String name = "card"+String(idx);
    cf.get(name, newCard, "");
    if (newCard == "") break;
    normalCards.insert(newCard);
    idx++;
  }
  IotsaSerial.print("rfid: Cards loaded: "); IotsaSerial.println(idx-1);
}

void IotsaRFIDMod::configSave() {
  IotsaConfigFileSave cf("/config/rfid.cfg");
  cf.put("addCard", addCard);
  cf.put("removeCard", removeCard);
  cf.put("resetAfterSolenoidMs", resetAfterSolenoidMs);
  int idx=1;
  for (auto it=normalCards.begin(); it != normalCards.end(); it++, idx++) {
    String name = "card"+String(idx);
    cf.put(name, *it);
  }
  IotsaSerial.print("rfid: Cards saved: "); IotsaSerial.println(idx-1);
}

