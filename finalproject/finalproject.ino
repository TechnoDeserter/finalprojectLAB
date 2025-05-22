#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

// Define relay pins (active LOW)
#define RELAY1_PIN 16
#define RELAY2_PIN 17
#define RELAY3_PIN 18
#define RELAY4_PIN 19

// Define switch pins
#define SWITCH1_PIN 23
#define SWITCH2_PIN 25
#define SWITCH3_PIN 26
#define SWITCH4_PIN 32

// Switch settings (active-high)
const int switchPins[] = {SWITCH1_PIN, SWITCH2_PIN, SWITCH3_PIN, SWITCH4_PIN};
const int numSwitches = 4;
const unsigned long DEBOUNCE_DELAY = 50; // Debounce time in ms
unsigned long lastDebounceTime[numSwitches] = {0};
int lastButtonState[numSwitches] = {LOW};
int buttonState[numSwitches] = {LOW};

// Scheduling mode variables
bool schedulingMode = false;
int selectedRelay = 0; // 0 = none, 1-4 = Relay 1-4
int selectedParam = 0; // 0 = onHour, 1 = onMinute, 2 = offHour, 3 = offMinute
int tempOnHour[4], tempOnMinute[4], tempOffHour[4], tempOffMinute[4];

// Initialize I2C LCD (20x4)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Initialize NTP client (UTC+8 for Philippines)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp.pagasa.dost.gov.ph", 28800, 60000);

Preferences preferences;
WebServer server(80);

// Time settings for each relay
int onHour1, onMinute1, offHour1, offMinute1;
int onHour2, onMinute2, offHour2, offMinute2;
int onHour3, onMinute3, offHour3, offMinute3;
int onHour4, onMinute4, offHour4, offMinute4;

// Relay states
bool relay1State = false;
bool relay2State = false;
bool relay3State = false;
bool relay4State = false;

// Serial input buffer
String serialInput = "";
const char* apSSID = "ESP32_Relay_Controller";
const char* apPassword = "12345678";
bool isClientConnected = false;

// LCD display functions
void displayIPAddress(String ip) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");
  lcd.setCursor(0, 1);
  lcd.print("IP Address:");
  lcd.setCursor(0, 2);
  lcd.print(ip);
  lcd.setCursor(0, 3);
  lcd.print("Enter in App");
  Serial.println("LCD updated with IP: " + ip);
}

void displayAPMode() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("AP MODE");
  lcd.setCursor(0, 1);
  lcd.print("SSID: ");
  lcd.print(apSSID);
  lcd.setCursor(0, 2);
  lcd.print("PASS: ");
  lcd.print(apPassword);
  lcd.setCursor(0, 3);
  lcd.print("IP: 192.168.4.1");
  Serial.println("LCD updated with AP mode info: SSID=" + String(apSSID) + ", PASS=" + String(apPassword) + ", IP=192.168.4.1");
}

void sendCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleRoot() {
  sendCorsHeaders();
  DynamicJsonDocument doc(256);
  doc["success"] = true;
  doc["ip"] = WiFi.localIP().toString();
  doc["ssid"] = WiFi.SSID();
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
  isClientConnected = true;
  Serial.println("App connected via /, displaying sensor data on LCD");
}

void handleSetWiFi() {
  sendCorsHeaders();
  DynamicJsonDocument doc(512);
  if (!server.hasArg("plain")) {
    doc["success"] = false;
    doc["error"] = "No data provided";
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    return;
  }

  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error) {
    doc.clear();
    doc["success"] = false;
    doc["error"] = "Invalid JSON";
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    Serial.println("Failed to parse JSON: " + String(error.c_str()));
    return;
  }

  String ssid = doc["ssid"];
  String password = doc["password"];

  if (ssid.isEmpty()) {
    doc.clear();
    doc["success"] = false;
    doc["error"] = "SSID cannot be empty";
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    Serial.println("SSID is empty");
    return;
  }

  Serial.println("Attempting to connect to WiFi SSID: " + ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  int attempts = 0;
  const int maxAttempts = 20;
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    attempts++;
    Serial.print(".");
  }
  Serial.println();

  doc.clear();
  if (WiFi.status() == WL_CONNECTED) {
    String ip = WiFi.localIP().toString();
    preferences.begin("wifi", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
    doc["success"] = true;
    doc["ip"] = ip;
    doc["ssid"] = ssid;
    isClientConnected = false;
    displayIPAddress(ip);
    Serial.println("Connected to WiFi, IP: " + ip);
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
    delay(1000); // Delay to ensure response is sent before switching modes
    WiFi.softAPdisconnect(true); // Explicitly disconnect AP mode
  } else {
    doc["success"] = false;
    doc["error"] = "Failed to connect to WiFi";
    String output;
    serializeJson(doc, output);
    server.send(500, "application/json", output);
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
    displayAPMode();
    Serial.println("Failed to connect to WiFi, reverted to AP mode, IP: 192.168.4.1");
  }
}

void handleDisconnect() {
  sendCorsHeaders();
  preferences.begin("wifi", false);
  preferences.clear();
  preferences.end();
  WiFi.disconnect();
  delay(500);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPassword);
  isClientConnected = false;
  displayAPMode();
  Serial.println("Disconnected and switched to AP mode, IP: 192.168.4.1");
  server.send(200, "application/json", "{\"success\": true, \"message\": \"Disconnected and switched to AP mode\"}");
}

void handleSet() {
  sendCorsHeaders();
  DynamicJsonDocument doc(1024);
  if (!server.hasArg("plain")) {
    doc["success"] = false;
    doc["error"] = "No data provided";
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    return;
  }

  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error) {
    doc.clear();
    doc["success"] = false;
    doc["error"] = "Invalid JSON";
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    return;
  }

  String onTime1 = doc["onTime1"];
  String offTime1 = doc["offTime1"];
  String onTime2 = doc["onTime2"];
  String offTime2 = doc["offTime2"];
  String onTime3 = doc["onTime3"];
  String offTime3 = doc["offTime3"];
  String onTime4 = doc["onTime4"];
  String offTime4 = doc["offTime4"];

  if (!onTime1 || !offTime1 || !onTime2 || !offTime2 || 
      !onTime3 || !offTime3 || !onTime4 || !offTime4 ||
      !onTime1.substring(2, 3).equals(":") || onTime1.length() != 5) {
    doc.clear();
    doc["success"] = false;
    doc["error"] = "Invalid time format";
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    return;
  }

  onHour1 = onTime1.substring(0, 2).toInt();
  onMinute1 = onTime1.substring(3).toInt();
  offHour1 = offTime1.substring(0, 2).toInt();
  offMinute1 = offTime1.substring(3).toInt();
  onHour2 = onTime2.substring(0, 2).toInt();
  onMinute2 = onTime2.substring(3).toInt();
  offHour2 = offTime2.substring(0, 2).toInt();
  offMinute2 = offTime2.substring(3).toInt();
  onHour3 = onTime3.substring(0, 2).toInt();
  onMinute3 = onTime3.substring(3).toInt();
  offHour3 = offTime3.substring(0, 2).toInt();
  offMinute3 = offTime3.substring(3).toInt();
  onHour4 = onTime4.substring(0, 2).toInt();
  onMinute4 = onTime4.substring(3).toInt();
  offHour4 = offTime4.substring(0, 2).toInt();
  offMinute4 = offTime4.substring(3).toInt();

  preferences.begin("relay_settings", false);
  preferences.putUInt("R1_onHour", onHour1);
  preferences.putUInt("R1_onMinute", onMinute1);
  preferences.putUInt("R1_offHour", offHour1);
  preferences.putUInt("R1_offMinute", offMinute1);
  preferences.putUInt("R2_onHour", onHour2);
  preferences.putUInt("R2_onMinute", onMinute2);
  preferences.putUInt("R2_offHour", offHour2);
  preferences.putUInt("R2_offMinute", offMinute2);
  preferences.putUInt("R3_onHour", onHour3);
  preferences.putUInt("R3_onMinute", onMinute3);
  preferences.putUInt("R3_offHour", offHour3);
  preferences.putUInt("R3_offMinute", offMinute3);
  preferences.putUInt("R4_onHour", onHour4);
  preferences.putUInt("R4_onMinute", onMinute4);
  preferences.putUInt("R4_offHour", offHour4);
  preferences.putUInt("R4_offMinute", offMinute4);
  preferences.end();

  doc.clear();
  doc["success"] = true;
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleReset() {
  sendCorsHeaders();
  preferences.begin("wifi", false);
  preferences.clear();
  preferences.end();
  preferences.begin("relay_settings", false);
  preferences.clear();
  preferences.end();
  DynamicJsonDocument doc(256);
  doc["success"] = true;
  doc["message"] = "Settings cleared. Device will restart in AP mode.";
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
  delay(3000);
  ESP.restart();
}

void handleToggle() {
  sendCorsHeaders();
  DynamicJsonDocument doc(256);
  if (!server.hasArg("plain")) {
    doc["success"] = false;
    doc["error"] = "No data provided";
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    return;
  }

  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error) {
    doc.clear();
    doc["success"] = false;
    doc["error"] = "Invalid JSON";
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    return;
  }

  String relayNum = doc["relay"];
  String state = doc["state"];
  
  int relayIndex = relayNum.toInt() - 1;
  bool newState = (state == "1");
  
  switch(relayIndex) {
    case 0:
      relay1State = newState;
      digitalWrite(RELAY1_PIN, newState ? LOW : HIGH);
      break;
    case 1:
      relay2State = newState;
      digitalWrite(RELAY2_PIN, newState ? LOW : HIGH);
      break;
    case 2:
      relay3State = newState;
      digitalWrite(RELAY3_PIN, newState ? LOW : HIGH);
      break;
    case 3:
      relay4State = newState;
      digitalWrite(RELAY4_PIN, newState ? LOW : HIGH);
      break;
    default:
      doc.clear();
      doc["success"] = false;
      doc["error"] = "Invalid relay number";
      String output;
      serializeJson(doc, output);
      server.send(400, "application/json", output);
      return;
  }
  
  doc.clear();
  doc["success"] = true;
  doc["relay"] = relayNum;
  doc["state"] = state;
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleOptions() {
  sendCorsHeaders();
  server.send(200, "text/plain", "");
}

void saveSchedule() {
  preferences.begin("relay_settings", false);
  preferences.putUInt("R1_onHour", tempOnHour[0]);
  preferences.putUInt("R1_onMinute", tempOnMinute[0]);
  preferences.putUInt("R1_offHour", tempOffHour[0]);
  preferences.putUInt("R1_offMinute", tempOffMinute[0]);
  preferences.putUInt("R2_onHour", tempOnHour[1]);
  preferences.putUInt("R2_onMinute", tempOnMinute[1]);
  preferences.putUInt("R2_offHour", tempOffHour[1]);
  preferences.putUInt("R2_offMinute", tempOffMinute[1]);
  preferences.putUInt("R3_onHour", tempOnHour[2]);
  preferences.putUInt("R3_onMinute", tempOnMinute[2]);
  preferences.putUInt("R3_offHour", tempOffHour[2]);
  preferences.putUInt("R3_offMinute", tempOffMinute[2]);
  preferences.putUInt("R4_onHour", tempOnHour[3]);
  preferences.putUInt("R4_onMinute", tempOnMinute[3]);
  preferences.putUInt("R4_offHour", tempOffHour[3]);
  preferences.putUInt("R4_offMinute", tempOffMinute[3]);
  preferences.end();

  onHour1 = tempOnHour[0];
  onMinute1 = tempOnMinute[0];
  offHour1 = tempOffHour[0];
  offMinute1 = tempOffMinute[0];
  onHour2 = tempOnHour[1];
  onMinute2 = tempOnMinute[1];
  offHour2 = tempOffHour[1];
  offMinute2 = tempOffMinute[1];
  onHour3 = tempOnHour[2];
  onMinute3 = tempOnMinute[2];
  offHour3 = tempOffHour[2];
  offMinute3 = tempOffMinute[2];
  onHour4 = tempOnHour[3];
  onMinute4 = tempOnMinute[3];
  offHour4 = tempOffHour[3];
  offMinute4 = tempOffMinute[3];
}

void handleSwitch(int index) {
  int reading = digitalRead(switchPins[index]);

  if (reading != lastButtonState[index]) {
    lastDebounceTime[index] = millis();
  }

  if ((millis() - lastDebounceTime[index]) > DEBOUNCE_DELAY) {
    if (reading != buttonState[index]) {
      buttonState[index] = reading;
      if (buttonState[index] == HIGH) {
        if (index == 0) {
          if (!schedulingMode) {
            schedulingMode = true;
            selectedRelay = 1;
            selectedParam = 0;
            tempOnHour[0] = onHour1; tempOnMinute[0] = onMinute1;
            tempOffHour[0] = offHour1; tempOffMinute[0] = offMinute1;
            tempOnHour[1] = onHour2; tempOnMinute[1] = onMinute2;
            tempOffHour[1] = offHour2; tempOffMinute[1] = offMinute2;
            tempOnHour[2] = onHour3; tempOnMinute[2] = onMinute3;
            tempOffHour[2] = offHour3; tempOffMinute[2] = offMinute3;
            tempOnHour[3] = onHour4; tempOnMinute[3] = onMinute4;
            tempOffHour[3] = offHour4; tempOffMinute[3] = offMinute4;
            Serial.println("Entered scheduling mode, Relay 1 selected");
          } else {
            selectedRelay++;
            if (selectedRelay > 4) {
              schedulingMode = false;
              selectedRelay = 0;
              saveSchedule();
              Serial.println("Exiting scheduling mode, schedule saved");
            } else {
              selectedParam = 0;
              Serial.printf("Selected Relay %d\n", selectedRelay);
            }
          }
        } else if (schedulingMode) {
          if (index == 1) {
            selectedParam = (selectedParam + 1) % 4;
            Serial.printf("Selected parameter: %s\n", 
              selectedParam == 0 ? "onHour" : 
              selectedParam == 1 ? "onMinute" : 
              selectedParam == 2 ? "offHour" : "offMinute");
          } else if (index == 2) {
            int relayIndex = selectedRelay - 1;
            if (selectedParam == 0) {
              tempOnHour[relayIndex] = (tempOnHour[relayIndex] + 1) % 24;
            } else if (selectedParam == 1) {
              tempOnMinute[relayIndex] = (tempOnMinute[relayIndex] + 1) % 60;
            } else if (selectedParam == 2) {
              tempOffHour[relayIndex] = (tempOffHour[relayIndex] + 1) % 24;
            } else if (selectedParam == 3) {
              tempOffMinute[relayIndex] = (tempOffMinute[relayIndex] + 1) % 60;
            }
            Serial.printf("Incremented Relay %d %s to %d\n", 
              selectedRelay, 
              selectedParam == 0 ? "onHour" : 
              selectedParam == 1 ? "onMinute" : 
              selectedParam == 2 ? "offHour" : "offMinute",
              selectedParam == 0 ? tempOnHour[relayIndex] :
              selectedParam == 1 ? tempOnMinute[relayIndex] :
              selectedParam == 2 ? tempOffHour[relayIndex] : tempOffMinute[relayIndex]);
          } else if (index == 3) {
            int relayIndex = selectedRelay - 1;
            if (selectedParam == 0) {
              tempOnHour[relayIndex] = (tempOnHour[relayIndex] - 1) < 0 ? 23 : tempOnHour[relayIndex] - 1;
            } else if (selectedParam == 1) {
              tempOnMinute[relayIndex] = (tempOnMinute[relayIndex] - 1) < 0 ? 59 : tempOnMinute[relayIndex] - 1;
            } else if (selectedParam == 2) {
              tempOffHour[relayIndex] = (tempOffHour[relayIndex] - 1) < 0 ? 23 : tempOffHour[relayIndex] - 1;
            } else if (selectedParam == 3) {
              tempOffMinute[relayIndex] = (tempOffMinute[relayIndex] - 1) < 0 ? 59 : tempOffMinute[relayIndex] - 1;
            }
            Serial.printf("Decremented Relay %d %s to %d\n", 
              selectedRelay, 
              selectedParam == 0 ? "onHour" : 
              selectedParam == 1 ? "onMinute" : 
              selectedParam == 2 ? "offHour" : "offMinute",
              selectedParam == 0 ? tempOnHour[relayIndex] :
              selectedParam == 1 ? tempOnMinute[relayIndex] :
              selectedParam == 2 ? tempOffHour[relayIndex] : tempOffMinute[relayIndex]);
          }
        }
      }
    }
  }

  lastButtonState[index] = reading;
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  // Initialize relays (active LOW)
  pinMode(RELAY1_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, HIGH);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY2_PIN, HIGH);
  pinMode(RELAY3_PIN, OUTPUT);
  digitalWrite(RELAY3_PIN, HIGH);
  pinMode(RELAY4_PIN, OUTPUT);
  digitalWrite(RELAY4_PIN, HIGH);

  // Initialize switches (active-high, external pull-down)
  for (int i = 0; i < numSwitches; i++) {
    pinMode(switchPins[i], INPUT);
  }

  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi...");

  // Try to connect with saved credentials
  preferences.begin("wifi", true);
  String savedSsid = preferences.getString("ssid", "");
  String savedPassword = preferences.getString("password", "");
  preferences.end();

  if (savedSsid.length() > 0 && savedPassword.length() > 0) {
    Serial.println("Attempting to connect to saved WiFi SSID: " + savedSsid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
    int attempts = 0;
    const int maxAttempts = 20;
    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      String ip = WiFi.localIP().toString();
      Serial.println("WiFi connected, IP: " + ip);
      displayIPAddress(ip);
    } else {
      Serial.println("Failed to connect to saved WiFi, starting AP mode");
      WiFi.disconnect();
      WiFi.mode(WIFI_AP);
      WiFi.softAP(apSSID, apPassword);
      Serial.println("AP mode started, IP: 192.168.4.1");
      displayAPMode();
    }
  } else {
    Serial.println("No WiFi credentials found, starting AP mode");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
    Serial.println("AP mode started, IP: 192.168.4.1");
    displayAPMode();
  }

  timeClient.begin();

  preferences.begin("relay_settings", true);
  onHour1 = preferences.getUInt("R1_onHour", 8);
  onMinute1 = preferences.getUInt("R1_onMinute", 0);
  offHour1 = preferences.getUInt("R1_offHour", 18);
  offMinute1 = preferences.getUInt("R1_offMinute", 0);
  onHour2 = preferences.getUInt("R2_onHour", 9);
  onMinute2 = preferences.getUInt("R2_onMinute", 0);
  offHour2 = preferences.getUInt("R2_offHour", 19);
  offMinute2 = preferences.getUInt("R2_offMinute", 0);
  onHour3 = preferences.getUInt("R3_onHour", 10);
  onMinute3 = preferences.getUInt("R3_onMinute", 0);
  offHour3 = preferences.getUInt("R3_offHour", 20);
  offMinute3 = preferences.getUInt("R3_offMinute", 0);
  onHour4 = preferences.getUInt("R4_onHour", 11);
  onMinute4 = preferences.getUInt("R4_onMinute", 0);
  offHour4 = preferences.getUInt("R4_offHour", 21);
  offMinute4 = preferences.getUInt("R4_offMinute", 0);
  preferences.end();

  server.on("/", handleRoot);
  server.on("/setWiFi", HTTP_POST, handleSetWiFi);
  server.on("/setWiFi", HTTP_OPTIONS, handleOptions);
  server.on("/disconnect", HTTP_POST, handleDisconnect);
  server.on("/set", HTTP_POST, handleSet);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/toggle", HTTP_POST, handleToggle);
  server.on("/set", HTTP_OPTIONS, handleOptions);
  server.on("/toggle", HTTP_OPTIONS, handleOptions);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      serialInput.trim();
      if (serialInput.equalsIgnoreCase("CLEAR_WIFI")) {
        Serial.println("Clearing WiFi credentials");
        preferences.begin("wifi", false);
        preferences.clear();
        preferences.end();
        WiFi.disconnect();
        delay(500);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(apSSID, apPassword);
        isClientConnected = false;
        Serial.println("Switched to AP mode, IP: 192.168.4.1");
        displayAPMode();
      }
      serialInput = "";
    } else {
      serialInput += c;
    }
  }

  for (int i = 0; i < numSwitches; i++) {
    handleSwitch(i);
  }

  server.handleClient();
  timeClient.update();

  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  int currentSecond = timeClient.getSeconds();

  updateRelayState(currentHour, currentMinute);
  displayData(currentHour, currentMinute, currentSecond);

  delay(100);
}

void updateRelayState(int hour, int minute) {
  if (hour == onHour1 && minute == onMinute1) {
    relay1State = true;
    digitalWrite(RELAY1_PIN, LOW);
  } else if (hour == offHour1 && minute == offMinute1) {
    relay1State = false;
    digitalWrite(RELAY1_PIN, HIGH);
  }
  if (hour == onHour2 && minute == onMinute2) {
    relay2State = true;
    digitalWrite(RELAY2_PIN, LOW);
  } else if (hour == offHour2 && minute == offMinute2) {
    relay2State = false;
    digitalWrite(RELAY2_PIN, HIGH);
  }
  if (hour == onHour3 && minute == onMinute3) {
    relay3State = true;
    digitalWrite(RELAY3_PIN, LOW);
  } else if (hour == offHour3 && minute == offMinute3) {
    relay3State = false;
    digitalWrite(RELAY3_PIN, HIGH);
  }
  if (hour == onHour4 && minute == onMinute4) {
    relay4State = true;
    digitalWrite(RELAY4_PIN, LOW);
  } else if (hour == offHour4 && minute == offMinute4) {
    relay4State = false;
    digitalWrite(RELAY4_PIN, HIGH);
  }
}

String formatTime(int hour, int minute) {
  String h = (hour < 10) ? "0" + String(hour) : String(hour);
  String m = (minute < 10) ? "0" + String(minute) : String(minute);
  return h + ":" + m;
}

void displayData(int hour, int minute, int second) {
  static unsigned long lastLCDUpdate = 0;
  const unsigned long lcdUpdateInterval = 5000;
  unsigned long currentTime = millis();

  if (currentTime - lastLCDUpdate < lcdUpdateInterval) {
    return;
  }

  if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED && !isClientConnected) {
    displayIPAddress(WiFi.localIP().toString());
    Serial.println("LCD displaying IP, waiting for app connection");
  } else if (WiFi.getMode() == WIFI_AP) {
    displayAPMode();
  } else {
    if (schedulingMode) {
      String relayStr = "Relay " + String(selectedRelay);
      String paramStr = selectedParam == 0 ? "ON_HR" : 
                        selectedParam == 1 ? "ON_MIN" : 
                        selectedParam == 2 ? "OFF_HR" : "OFF_MIN";
      int relayIndex = selectedRelay - 1;
      int value = selectedParam == 0 ? tempOnHour[relayIndex] :
                  selectedParam == 1 ? tempOnMinute[relayIndex] :
                  selectedParam == 2 ? tempOffHour[relayIndex] : tempOffMinute[relayIndex];
      String valueStr = selectedParam % 2 == 0 ? String(value) : (value < 10 ? "0" + String(value) : String(value));
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Edit: " + relayStr + " " + paramStr);
      lcd.setCursor(0, 1);
      lcd.print("Value: " + valueStr);
      lcd.setCursor(0, 2);
      lcd.print("S1:Next S2:Param");
      lcd.setCursor(0, 3);
      lcd.print("S3:+ S4:-");
    } else {
      String timeStr = "Time: " + formatTime(hour, minute) + ":" + 
                      (second < 10 ? "0" + String(second) : String(second)) + 
                      " W:" + (WiFi.status() == WL_CONNECTED ? "ON" : "OFF");
      String R1Str = "R1ON:" + formatTime(onHour1, onMinute1) + " OFF:" + formatTime(offHour1, offMinute1);
      String R2Str = "R2ON:" + formatTime(onHour2, onMinute2) + " OFF:" + formatTime(offHour2, offMinute2);
      String R3Str = "R3ON:" + formatTime(onHour3, onMinute3) + " OFF:" + formatTime(offHour3, offMinute3);
      String R4Str = "R4ON:" + formatTime(onHour4, onMinute4) + " OFF:" + formatTime(offHour4, offMinute4);
      String ipStr = "IP: " + WiFi.localIP().toString();

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(timeStr.substring(0, 20));

      if (second % 10 < 5) {
        lcd.setCursor(0, 1);
        lcd.print(R1Str.substring(0, 20));
        lcd.setCursor(0, 2);
        lcd.print(R2Str.substring(0, 20));
      } else {
        lcd.setCursor(0, 1);
        lcd.print(R3Str.substring(0, 20));
        lcd.setCursor(0, 2);
        lcd.print(R4Str.substring(0, 20));
      }

      lcd.setCursor(0, 3);
      lcd.print(ipStr.substring(0, 20));
    }
  }
  lastLCDUpdate = currentTime;
}