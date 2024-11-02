#include <WiFi.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <MFRC522.h>
#include <SPI.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// MySQL server settings
IPAddress server_addr(XXX,XXX,XXX,XXX);  // IP of the MySQL server
char user[] = "YOUR_MYSQL_USERNAME";     // MySQL user login username
char password_mysql[] = "YOUR_MYSQL_PASSWORD"; // MySQL user login password

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

#define SS_PIN 5
#define RST_PIN 27
MFRC522 rfid(SS_PIN, RST_PIN);

WiFiClient client;
MySQL_Connection conn((Client *)&client);

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
  
  // Connect to MySQL
  Serial.println("Connecting to MySQL...");
  while (!conn.connect(server_addr, 3306, user, password_mysql)) {
    Serial.println("MySQL connection failed. Retrying...");
    delay(5000);
  }
  Serial.println("Connected to MySQL");

  // Create tables if they don't exist
  createTables();

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

void createTables() {
  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  
  const char *create_inventory = "CREATE TABLE IF NOT EXISTS inventory ("
                                 "id VARCHAR(50) PRIMARY KEY,"
                                 "name VARCHAR(100) NOT NULL,"
                                 "quantity INT NOT NULL,"
                                 "last_scanned BIGINT)";

  const char *create_scans = "CREATE TABLE IF NOT EXISTS recent_scans ("
                             "id INT AUTO_INCREMENT PRIMARY KEY,"
                             "scan_time BIGINT NOT NULL)";

  cur_mem->execute(create_inventory);
  cur_mem->execute(create_scans);

  delete cur_mem;
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
      String id = doc["item"]["id"];
      String name = doc["item"]["name"];
      int quantity = doc["item"]["quantity"];
      String lastScanned = doc["item"]["lastScanned"];
      
      addInventoryItem(id, name, quantity, lastScanned);
      sendInventoryToAll();
    }
  }
}

void sendInventory(AsyncWebSocketClient *client) {
  DynamicJsonDocument doc(1024);
  doc["type"] = "inventory";
  JsonArray items = doc.createNestedArray("items");
  
  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  cur_mem->execute("SELECT * FROM inventory");
  column_names *cols = cur_mem->get_columns();

  row_values *row = NULL;
  do {
    row = cur_mem->get_next_row();
    if (row != NULL) {
      JsonObject itemObj = items.createNestedObject();
      itemObj["id"] = row->values[0];
      itemObj["name"] = row->values[1];
      itemObj["quantity"] = atoi(row->values[2]);
      itemObj["lastScanned"] = row->values[3];
    }
  } while (row != NULL);

  delete cur_mem;
  
  String output;
  serializeJson(doc, output);
  client->text(output);
}

void sendInventoryToAll() {
  DynamicJsonDocument doc(1024);
  doc["type"] = "inventory";
  JsonArray items = doc.createNestedArray("items");
  
  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  cur_mem->execute("SELECT * FROM inventory");
  column_names *cols = cur_mem->get_columns();

  row_values *row = NULL;
  do {
    row = cur_mem->get_next_row();
    if (row != NULL) {
      JsonObject itemObj = items.createNestedObject();
      itemObj["id"] = row->values[0];
      itemObj["name"] = row->values[1];
      itemObj["quantity"] = atoi(row->values[2]);
      itemObj["lastScanned"] = row->values[3];
    }
  } while (row != NULL);

  delete cur_mem;
  
  String output;
  serializeJson(doc, output);
  ws.textAll(output);
}

void handleRFIDScan(String id) {
  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  
  char query[128];
  sprintf(query, "UPDATE inventory SET quantity = quantity + 1, last_scanned = %lld WHERE id = '%s'", millis(), id.c_str());
  cur_mem->execute(query);
  
  if (cur_mem->affected_rows() > 0) {
    DynamicJsonDocument doc(256);
    doc["type"] = "scan";
    doc["time"] = String(millis());
    
    String output;
    serializeJson(doc, output);
    ws.textAll(output);
    
    sendInventoryToAll();
  } else {
    Serial.println("Unknown RFID tag scanned: " + id);
  }
  
  // Add to recent scans
  sprintf(query, "INSERT INTO recent_scans (scan_time) VALUES (%lld)", millis());
  cur_mem->execute(query);
  
  delete cur_mem;
  
  updateRecentScans();
}

void updateRecentScans() {
  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  
  cur_mem->execute("SELECT scan_time FROM recent_scans ORDER BY id DESC LIMIT 5");
  column_names *cols = cur_mem->get_columns();

  DynamicJsonDocument doc(512);
  doc["type"] = "recentScans";
  JsonArray scansArray = doc.createNestedArray("scans");

  row_values *row = NULL;
  do {
    row = cur_mem->get_next_row();
    if (row != NULL) {
      scansArray.add(row->values[0]);
    }
  } while (row != NULL);

  delete cur_mem;
  
  String output;
  serializeJson(doc, output);
  ws.textAll(output);
}

void addInventoryItem(String id, String name, int quantity, String lastScanned) {
  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  
  char query[256];
  sprintf(query, "INSERT INTO inventory (id, name, quantity, last_scanned) VALUES ('%s', '%s', %d, %s) ON DUPLICATE KEY UPDATE name='%s', quantity=%d, last_scanned=%s",
          id.c_str(), name.c_str(), quantity, lastScanned.c_str(), name.c_str(), quantity, lastScanned.c_str());
  
  cur_mem->execute(query);
  
  delete cur_mem;
}