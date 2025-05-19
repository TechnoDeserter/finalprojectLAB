/**************************************************
 * Real-Time Operating System (RTOS) Based Relay Control with ESP32
 * SUBMITTED BY: PANCHO, MICHAEL ANGILO SALUDO PANCHO
 * SUBMITTED TO: PROF. MICHAEL T. SAMONTE
 * Modified: Compatible with 30-pin ESP32, JSON responses, static IP, WiFiManager, Serial reset to AP mode, CORS
 * Switch Adaptation: Active-high buttons (HIGH = pressed) with external 10kΩ pull-down resistors, debounced
 **************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiManager.h>
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
int lastButtonState[numSwitches] = {LOW}; // LOW = not pressed (pull-down)
int buttonState[numSwitches] = {LOW};    // LOW = not pressed (pull-down)

// Initialize I2C LCD (20x4)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Initialize NTP client (UTC+8 for Philippines)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp.pagasa.dost.gov.ph", 28800, 60000);

Preferences preferences;
WiFiManager wifiManager;
WebServer server(80);

// Static IP configuration
IPAddress local_IP(192, 168, 8, 116);
IPAddress gateway(192, 168, 8, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8); // Google DNS

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
  wifiManager.resetSettings();
  preferences.begin("wifi", false);
  preferences.clear();
  preferences.end();
  DynamicJsonDocument doc(256);
  doc["success"] = true;
  doc["message"] = "WiFi settings reset. Device will restart in AP mode.";
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

void resetToAPMode() {
  Serial.println("Resetting to AP mode...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Resetting to AP...");
  wifiManager.resetSettings();
  preferences.begin("wifi", false);
  preferences.clear();
  preferences.end();
  Serial.println("WiFi settings cleared. Restarting...");
  lcd.setCursor(0, 1);
  lcd.print("Restarting...");
  delay(3000);
  ESP.restart();
}

void setup() {
  Serial.begin(9600);
  Wire.begin(); // Uses default I2C pins: SDA=21, SCL=22

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  // Initialize relays (active LOW)
  pinMode(RELAY1_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, HIGH); // Off
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY2_PIN, HIGH); // Off
  pinMode(RELAY3_PIN, OUTPUT);
  digitalWrite(RELAY3_PIN, HIGH); // Off
  pinMode(RELAY4_PIN, OUTPUT);
  digitalWrite(RELAY4_PIN, HIGH); // Off

  // Initialize switches (active-high, external pull-down)
  for (int i = 0; i < numSwitches; i++) {
    pinMode(switchPins[i], INPUT); // Requires 10kΩ pull-down resistor
  }

  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi...");

  // Try to connect with saved credentials
  preferences.begin("wifi", true);
  String savedSsid = preferences.getString("ssid", "");
  String savedPassword = preferences.getString("password", "");
  preferences.end();

  if (!savedSsid.isEmpty()) {
    WiFi.config(local_IP, gateway, subnet, primaryDNS);
    WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    wifiManager.setConfigPortalTimeout(180);
    wifiManager.setConnectTimeout(30);
    if (!wifiManager.autoConnect("ESP32_Relay_Controller")) {
      Serial.println("Failed to connect and hit timeout");
      lcd.clear();
      lcd.print("WiFi Connect Failed");
      lcd.setCursor(0, 1);
      lcd.print("Restarting...");
      delay(3000);
      ESP.restart();
    }
  }

  Serial.println("\nConnected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  lcd.clear();
  lcd.print("WiFi Connected");
  lcd.setCursor(0, 1);
  lcd.print("IP: ");
  lcd.print(WiFi.localIP());
  delay(2000);

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
  server.on("/set", HTTP_POST, handleSet);
  server.on("/reset", handleReset);
  server.on("/toggle", HTTP_POST, handleToggle);
  server.on("/set", HTTP_OPTIONS, handleOptions);
  server.on("/toggle", HTTP_OPTIONS, handleOptions);
  server.begin();
  Serial.println("HTTP server started");
}

void handleSwitch(int index) {
  int reading = digitalRead(switchPins[index]);

  // Check if button state changed
  if (reading != lastButtonState[index]) {
    lastDebounceTime[index] = millis();
  }

  // Check if debounce delay has passed
  if ((millis() - lastDebounceTime[index]) > DEBOUNCE_DELAY) {
    if (reading != buttonState[index]) {
      buttonState[index] = reading;
      // Active-high: HIGH = pressed
      if (buttonState[index] == HIGH) {
        // Toggle corresponding relay
        switch (index) {
          case 0:
            relay1State = !relay1State;
            digitalWrite(RELAY1_PIN, relay1State ? LOW : HIGH);
            Serial.printf("Button 1 pressed, Relay 1 %s\n", relay1State ? "ON" : "OFF");
            break;
          case 1:
            relay2State = !relay2State;
            digitalWrite(RELAY2_PIN, relay2State ? LOW : HIGH);
            Serial.printf("Button 2 pressed, Relay 2 %s\n", relay2State ? "ON" : "OFF");
            break;
          case 2:
            relay3State = !relay3State;
            digitalWrite(RELAY3_PIN, relay3State ? LOW : HIGH);
            Serial.printf("Button 3 pressed, Relay 3 %s\n", relay3State ? "ON" : "OFF");
            break;
          case 3:
            relay4State = !relay4State;
            digitalWrite(RELAY4_PIN, relay4State ? LOW : HIGH);
            Serial.printf("Button 4 pressed, Relay 4 %s\n", relay4State ? "ON" : "OFF");
            break;
        }
      }
    }
  }

  lastButtonState[index] = reading;
}

void loop() {
  // Handle serial input for RESET_AP command
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      serialInput.trim();
      if (serialInput.equalsIgnoreCase("RESET_AP")) {
        resetToAPMode();
      }
      serialInput = "";
    } else {
      serialInput += c;
    }
  }

  // Handle switches
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

  delay(100); // Reduced delay for better responsiveness
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