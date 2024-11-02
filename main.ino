#include <WiFi.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <MFRC522.h>
#include <SPI.h>
#include <vector>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

#define SS_PIN 5
#define RST_PIN 27
MFRC522 rfid(SS_PIN, RST_PIN);

struct InventoryItem {
  String id;
  String name;
  int quantity;
  String lastScanned;
};

std::vector<InventoryItem> inventory;
std::vector<String> recentScans;

void setup() {
  Serial.begin(115200);
  
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  SPI.begin();
  rfid.PCD_Init();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());
  
  inventory.push_back({"001", "Item 1", 10, ""});
  inventory.push_back({"002", "Item 2", 5, ""});
  inventory.push_back({"003", "Item 3", 15, ""});

  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.begin();
}

void loop() {
  ws.cleanupClients();
  
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String id = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      id += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
      id += String(rfid.uid.uidByte[i], HEX);
    }
    
    handleRFIDScan(id);
    
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      sendInventory(client);
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, message);
    
    if (doc["type"] == "addItem") {
      InventoryItem newItem;
      newItem.id = doc["item"]["id"].as<String>();
      newItem.name = doc["item"]["name"].as<String>();
      newItem.quantity = doc["item"]["quantity"].as<int>();
      newItem.lastScanned = doc["item"]["lastScanned"].as<String>();
      inventory.push_back(newItem);
      sendInventoryToAll();
    }
  }
}

void sendInventory(AsyncWebSocketClient *client) {
  DynamicJsonDocument doc(1024);
  doc["type"] = "inventory";
  JsonArray items = doc.createNestedArray("items");
  
  for (const auto& item : inventory) {
    JsonObject itemObj = items.createNestedObject();
    itemObj["id"] = item.id;
    itemObj["name"] = item.name;
    itemObj["quantity"] = item.quantity;
    itemObj["lastScanned"] = item.lastScanned;
  }
  
  String output;
  serializeJson(doc, output);
  client->text(output);
}

void sendInventoryToAll() {
  DynamicJsonDocument doc(1024);
  doc["type"] = "inventory";
  JsonArray items = doc.createNestedArray("items");
  
  for (const auto& item : inventory) {
    JsonObject itemObj = items.createNestedObject();
    itemObj["id"] = item.id;
    itemObj["name"] = item.name;
    itemObj["quantity"] = item.quantity;
    itemObj["lastScanned"] = item.lastScanned;
  }
  
  String output;
  serializeJson(doc, output);
  ws.textAll(output);
}

void handleRFIDScan(String id) {
  bool itemFound = false;
  for (auto& item : inventory) {
    if (item.id == id) {
      item.quantity++;
      item.lastScanned = String(millis());
      itemFound = true;
      
      DynamicJsonDocument doc(256);
      doc["type"] = "scan";
      doc["time"] = item.lastScanned;
      
      String output;
      serializeJson(doc, output);
      ws.textAll(output);
      
      sendInventoryToAll();
      break;
    }
  }
  
  if (!itemFound) {
    Serial.println("Unknown RFID tag scanned: " + id);
  }
  
  updateRecentScans(String(millis()));
}

void updateRecentScans(String time) {
  recentScans.insert(recentScans.begin(), time);
  if (recentScans.size() > 5) {
    recentScans.pop_back();
  }
  
  DynamicJsonDocument doc(512);
  doc["type"] = "recentScans";
  JsonArray scansArray = doc.createNestedArray("scans");
  for (const auto& scan : recentScans) {
    scansArray.add(scan);
  }
  
  String output;
  serializeJson(doc, output);
  ws.textAll(output);
}