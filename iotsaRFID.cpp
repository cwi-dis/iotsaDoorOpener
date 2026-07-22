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
    IotsaSerial.print("added card: ");
    IotsaSerial.println(uid);
  }
}

void IotsaRFIDMod::handleRemoveCard(const String& uid) {
  auto it = normalCards.find(uid);
  if (it != normalCards.end()) {
    normalCards.erase(it);
    IotsaSerial.print("Removed card: ");
    IotsaSerial.println(uid);
  }
}

//
// Implementation of the RFID module
//
void IotsaRFIDMod::setup() {
  SPI.begin();
  mfrc522.PCD_Init();
  configLoad();
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
  reply["lastCard"] = lastCard;
  if (lastCard != "") {
    reply["lastCardPresented"] = (millis()-lastCardReadTime)/1000;
  }
  JsonArray rCards = reply["cards"].as<JsonArray>();
  for (auto value : normalCards) {
    rCards.add(value);
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
  JsonArray newCards;
  if (getFromRequest<JsonArray>(reqObj, "cards", newCards)) {
    any = true;
    normalCards.clear();
    for (auto value: newCards) {
      normalCards.insert(value.as<String>());
    }
  }
  if (any) configSave();

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
  static uint32_t lastPoll;
  uint32_t now = millis();

  if (now > lastPoll && now < lastPoll + RFID_POLL_INTERVAL) 
    return;
  lastPoll = now;
  //IotsaSerial.println("rfid in");
  if (curMode != card_idle && millis() > curModeEndTime) {
    // No card present for 2 seconds: revert to idle
    IotsaSerial.println("mode timeout.");
    curModeEndTime = 0;
    curMode = card_idle;
    if (modeChanged) modeChanged(curMode);
  }
  if (!mfrc522.PICC_IsNewCardPresent()) {
    //IotsaSerial.println("rfid no card");
    return;
  }
  IotsaSerial.println("card present");
  if (!mfrc522.PICC_ReadCardSerial()) {
    //IotsaSerial.println("rfid no serial");
    return;
  }
  IotsaSerial.println("serial read");
  String newCard = strUid(mfrc522.uid);
  handleCard(newCard);
  //IotsaSerial.println("rfid out");
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
    IotsaSerial.print("mode: ");
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
  IotsaSerial.print("Cards loaded: "); IotsaSerial.println(idx-1);
}

void IotsaRFIDMod::configSave() {
  IotsaConfigFileSave cf("/config/rfid.cfg");
  cf.put("addCard", addCard);
  cf.put("removeCard", removeCard);
  int idx=1;
  for (auto it=normalCards.begin(); it != normalCards.end(); it++, idx++) {
    String name = "card"+String(idx);
    cf.put(name, *it);
  }
  IotsaSerial.print("Cards saved: "); IotsaSerial.println(idx-1);
}

