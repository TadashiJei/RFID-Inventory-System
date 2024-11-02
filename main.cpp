#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <vector>
#include <TimeLib.h>

// Pin Definitions
#define SS_PIN 5
#define RST_PIN 27
#define RELAY_PIN 22
#define VOLTAGE_SENSOR_PIN A0

// RFID
MFRC522 rfid(SS_PIN, RST_PIN);

// Network and Firebase Configuration
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define FIREBASE_HOST "YOUR_FIREBASE_HOST"
#define FIREBASE_AUTH "YOUR_FIREBASE_AUTH"

// Firebase
FirebaseData firebaseData;
FirebaseJson json;

// Authorized UIDs (you can add more)
std::vector<std::vector<byte>> authorizedUIDs = {
  {0xD3, 0xF8, 0x02, 0x1E},
  {0xA1, 0xB2, 0xC3, 0xD4}
};

// Variables
int scanCount = 0;
unsigned long lastScanTime = 0;
const unsigned long COOLDOWN_PERIOD = 5000; // 5 seconds cooldown
const int MAX_DAILY_SCANS = 100;
int dailyScans = 0;
time_t lastResetTime = 0;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(VOLTAGE_SENSOR_PIN, INPUT);

  setupWiFi();
  setupFirebase();

  Serial.println("Inventory Management System Ready");
  Serial.println("Tap an RFID/NFC tag on the RFID-RC522 reader");
}

void loop() {
  if (isNewDay()) {
    resetDailyScans();
  }

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    handleRFIDScan();
  }
}

void setupWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nConnected to Wi-Fi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void setupFirebase() {
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  Firebase.setReadTimeout(firebaseData, 1000 * 60);
  Firebase.setwriteSizeLimit(firebaseData, "tiny");
  Serial.println("Connected to Firebase");
}

void handleRFIDScan() {
  if (isAuthorizedTag()) {
    if (canScan()) {
      processAuthorizedTag();
    } else {
      Serial.println("Scan limit reached or cooldown period active");
    }
  } else {
    reportUnauthorizedTag();
  }
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

bool isAuthorizedTag() {
  for (const auto& uid : authorizedUIDs) {
    if (rfid.uid.uidByte[0] == uid[0] &&
        rfid.uid.uidByte[1] == uid[1] &&
        rfid.uid.uidByte[2] == uid[2] &&
        rfid.uid.uidByte[3] == uid[3]) {
      return true;
    }
  }
  return false;
}

bool canScan() {
  unsigned long currentTime = millis();
  if (currentTime - lastScanTime < COOLDOWN_PERIOD) {
    return false;
  }
  if (dailyScans >= MAX_DAILY_SCANS) {
    return false;
  }
  return true;
}

void processAuthorizedTag() {
  Serial.println("Authorized Tag");
  scanCount++;
  dailyScans++;
  lastScanTime = millis();

  int voltageReading = analogRead(VOLTAGE_SENSOR_PIN);
  
  updateFirebase(scanCount, voltageReading);
  activateRelay();
}

void updateFirebase(int count, int voltage) {
  json.clear();
  json.set("/scanCount", count);
  json.set("/voltage", voltage);
  json.set("/lastScan", Firebase.getCurrentTime());

  if (Firebase.updateNode(firebaseData, "/InventoryData", json)) {
    Serial.println("Firebase update successful");
  } else {
    Serial.println("Firebase update failed");
    Serial.println("Reason: " + firebaseData.errorReason());
  }
}

void activateRelay() {
  digitalWrite(RELAY_PIN, HIGH);
  delay(2000);
  digitalWrite(RELAY_PIN, LOW);
}

void reportUnauthorizedTag() {
  Serial.print("Unauthorized Tag with UID:");
  for (int i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();

  // Log unauthorized attempt to Firebase
  json.clear();
  json.set("/unauthorizedScan", Firebase.getCurrentTime());
  Firebase.pushJSON(firebaseData, "/UnauthorizedScans", json);
}

bool isNewDay() {
  time_t now = time(nullptr);
  if (now - lastResetTime >= 86400) { // 86400 seconds in a day
    return true;
  }
  return false;
}

void resetDailyScans() {
  dailyScans = 0;
  lastResetTime = time(nullptr);
  Serial.println("Daily scan count reset");
}