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
IPAddress server_addr(192,168,1,100);  // Replace with your MySQL server IP
char user[] = "rfid_user";              // MySQL user login username
char password_mysql[] = "rfid_password"; // MySQL user login password
char schema[] = "rfid_inventory";       // MySQL database name

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

#define SS_PIN 5
#define RST_PIN 27
MFRC522 rfid(SS_PIN, RST_PIN);

WiFiClient client;
MySQL_Connection conn((Client *)&client);

// New function to check connection status
bool checkConnection() {
  if (conn.connected()) {
    return true;
  }
  Serial.println("MySQL connection lost. Reconnecting...");
  return conn.connect(server_addr, 3306, user, password_mysql);
}

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

  // New endpoint for fetching recent scans
  server.on("/recent-scans", HTTP_GET, [](AsyncWebServerRequest *request){
    if (checkConnection()) {
      sendRecentScans(request);
    } else {
      request->send(500, "text/plain", "Database connection error");
    }
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
    
    if (checkConnection()) {
      handleRFIDScan(id);
    } else {
      Serial.println("Failed to process RFID scan due to database connection error");
    }
    
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}

// ... (keep the existing functions)

// New function to send recent scans
void sendRecentScans(AsyncWebServerRequest *request) {
  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  
  cur_mem->execute("SELECT scan_time FROM recent_scans ORDER BY id DESC LIMIT 10");
  column_names *cols = cur_mem->get_columns();

  DynamicJsonDocument doc(1024);
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
  request->send(200, "application/json", output);
}

// New function to get item details
void getItemDetails(String id, AsyncWebSocketClient *client) {
  if (!checkConnection()) {
    return;
  }

  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  
  char query[128];
  sprintf(query, "SELECT * FROM inventory WHERE id = '%s'", id.c_str());
  cur_mem->execute(query);

  column_names *cols = cur_mem->get_columns();
  row_values *row = cur_mem->get_next_row();

  if (row != NULL) {
    DynamicJsonDocument doc(256);
    doc["type"] = "itemDetails";
    doc["id"] = row->values[0];
    doc["name"] = row->values[1];
    doc["quantity"] = atoi(row->values[2]);
    doc["lastScanned"] = row->values[3];

    String output;
    serializeJson(doc, output);
    client->text(output);
  }

  delete cur_mem;
}

// Modify handleWebSocketMessage to include the new getItemDetails function
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
    } else if (doc["type"] == "getItemDetails") {
      String id = doc["id"];
      AsyncWebSocketClient *client = (AsyncWebSocketClient*)arg;
      getItemDetails(id, client);
    }
  }
}