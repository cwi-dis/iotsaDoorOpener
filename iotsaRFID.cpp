#include "iotsa.h"
#include "iotsaRFID.h"
#include "iotsaConfigFile.h"
#include "MFRC522.h"
#include <set>

String addCard;
String removeCard;
String lastCard;
uint32_t lastCardReadTime;
bool lastCardKnown;
std::set<String> normalCards;


cardMode curMode;
uint32_t curModeEndTime;

// RFID reader interface object

MFRC522 mfrc522(PIN_RFID_SDA, PIN_RFID_RESET);

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

// Helper function: compare RFID UID to string

static bool cmpUid(MFRC522::Uid& uid, String& wanted) {
  if (uid.size*2 != wanted.length()) return false;
  
  for (int i=0; i<uid.size; i++) {
    char c = '0' + ((uid.uidByte[i] >> 4) & 0xf);
    if (c != wanted[2*i]) return false;
    c = '0' + (uid.uidByte[i] & 0xf);
    if (c != wanted[2*i+1]) return false;
  }
  return true;
}

bool lookupCard(String& uid) {
  auto it = normalCards.find(uid);
  bool ok = it != normalCards.end();
  return ok;
}

void handleAddCard(String& uid) {
  auto it = normalCards.find(uid);
  if (it == normalCards.end()) {
    normalCards.insert(uid);
    IotsaSerial.print("added card: ");
    IotsaSerial.println(uid);
  }
}

void handleRemoveCard(String& uid) {
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
  for (uint8_t i=0; i<server->args(); i++){
    if( server->argName(i) == "addCard") {
        if( server->arg(i) != addCard) {
          anyChanged = true;
          addCard = server->arg(i);
          IotsaSerial.print("Set addCard: ");
          IotsaSerial.println(addCard);
        }
    }
    if( server->argName(i) == "removeCard") {
        if( server->arg(i) != removeCard) {
          anyChanged = true;
          removeCard = server->arg(i);
          IotsaSerial.print("Set removeCard: ");
          IotsaSerial.println(removeCard);
        }
    }
    if( server->argName(i) == "normalAdd") {
        String val = server->arg(i);
        if( val != "") {
          anyChanged = true;
          handleAddCard(val);
        }
    }
    if( server->argName(i) == "normalRemove") {
        String val = server->arg(i);
        if( val != "") {
          anyChanged = true;
          handleRemoveCard(val);
        }
    }
    if (anyChanged) configSave();
  }
  String message = "<html><head><title>RFID Server</title></head><body><h1>RFID Server</h1>";
  if (lastCard != "") {
    message += "<p>Last card was presented " + String((millis()-lastCardReadTime)/1000) + " seconds ago, Card ID " + lastCard;
    if (!lastCardKnown) message += " (unknown)";
    message += ".</p>";
  } else {
    message += "<p>No card presented since last power-on.</p>";
  }

  message += "<h2>Adding Cards</h2><form method='get'>Master addition card ID: <input name='addCard' value='";
  message += addCard;
  message += "'><br>Master removal card ID: <input name='removeCard' value='";
  message += removeCard;
  message += "'><br>Manually add normal card: <input name='normalAdd'><br><input type='submit'></form>";

  message += "<h2>Known Cards</h2><ul>";
  for (auto it=normalCards.begin(); it != normalCards.end(); it++) {
    message += "<li>" + *it + "(<a href='/rfid?normalRemove=" + *it + "'>remove</a>)</li>";
  }
  message += "</ul>";
  
  message += "</body></html>";
  server->send(200, "text/html", message);
}

bool IotsaRFIDMod::getHandler(const char *path, JsonObject& reply) {
  reply["addCard"] = addCard;
  reply["removeCard"] = removeCard;
  reply["lastCard"] = lastCard;
  if (lastCard != "") {
    reply["lastCardPresented"] = (millis()-lastCardReadTime)/1000;
  }
  JsonArray& rCards =reply.createNestedArray("cards");
  for (auto value : normalCards) {
    rCards.add(value);
  }
  return true;
}

bool IotsaRFIDMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  if (!request.is<JsonObject>()) return false;
  JsonObject& reqObj = request.as<JsonObject>();
  bool any = false;
  if (reqObj.containsKey("addCard")) {
    any = true;
    addCard = reqObj.get<String>("addCard");
  }
  if (reqObj.containsKey("removeCard")) {
    any = true;
    removeCard = reqObj.get<String>("removeCard");
  }
  if (reqObj.containsKey("cards")) {
    any = true;
    normalCards.clear();
    JsonArray& newCards = reqObj.get<JsonArray>("cards");
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

