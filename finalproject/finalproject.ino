#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <ESP.h>

// Define relay pins (active LOW)
#define RELAY1_PIN 16
#define RELAY2_PIN 17
#define RELAY3_PIN 18
#define RELAY4_PIN 19

// Define switch pins (active-high)
#define SWITCH1_PIN 23
#define SWITCH2_PIN 25
#define SWITCH3_PIN 26
#define SWITCH4_PIN 32

// Arrays for easier management
const int switchPins[] = {SWITCH1_PIN, SWITCH2_PIN, SWITCH3_PIN, SWITCH4_PIN};
const int relayPins[] = {RELAY1_PIN, RELAY2_PIN, RELAY3_PIN, RELAY4_PIN};
const int numSwitches = 4;

// Debounce and timing settings
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long EXIT_DURATION = 3000;
const unsigned long CURSOR_BLINK_INTERVAL = 500;
const unsigned long BLINK_INTERVAL = 1000;
const unsigned long TIMER_UPDATE_INTERVAL = 1000;
const unsigned long LCD_UPDATE_INTERVAL = 1000;
const unsigned long DISPLAY_SWITCH_INTERVAL = 3000;
const unsigned long STARTUP_AP_DISPLAY_DURATION = 2000;
const unsigned long NO_NETWORK_MESSAGE_DURATION = 3000;

// Initialize I2C LCD (20x4)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Initialize NTP client (UTC+8 for Philippines)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp.pagasa.dost.gov.ph", 28800, 60000);

// Preferences and WebServer
Preferences preferences;
WebServer server(80);

// WiFi settings
const char* apSSID = "ESP32_Relay_Controller";
const char* apPassword = "12345678";
bool isClientConnected = false;
String serialInput = "";

// Debounce and state variables for switches
unsigned long lastDebounceTime[numSwitches] = {0, 0, 0, 0};
int lastButtonState[numSwitches] = {LOW, LOW, LOW, LOW};
int buttonState[numSwitches] = {LOW, LOW, LOW, LOW};
bool relayState[numSwitches] = {false, false, false, false};

// Mode selection and exit tracking
unsigned long switch2PressStartTime = 0;
bool isExiting = false;
bool inMenu = true;
int cursorPosition = 0;
int menuOffset = 0; // Tracks the first visible mode
unsigned long lastCursorBlinkTime = 0;
bool cursorState = true;

// Timer mode variables
enum TimerState { TIMER_SETUP, TIMER_RUNNING, TIMER_PAUSED, TIMER_BLINKING };
TimerState timerState = TIMER_SETUP;
int timerField = 0;
int timerValues[3] = {0, 0, 0};
unsigned long timerStartTime = 0;
unsigned long lastTimerUpdate = 0;
unsigned long lastBlinkTime = 0;
bool relaysBlinkingOn = false;

// Scheduling mode variables
bool schedulingMode = false;
int selectedRelay = 0; // 0 = none, 1-4 = Relay 1-4
int selectedParam = 0; // 0 = onHour, 1 = onMinute, 2 = offHour, 3 = offMinute
int tempOnHour[4], tempOnMinute[4], tempOffHour[4], tempOffMinute[4];
int onHour1, onMinute1, offHour1, offMinute1;
int onHour2, onMinute2, offHour2, offMinute2;
int onHour3, onMinute3, offHour3, offMinute3;
int onHour4, onMinute4, offHour4, offMinute4;

// Startup state
enum StartupState { SHOW_AP, SHOW_MENU };
StartupState startupState = SHOW_AP;
unsigned long startupStartTime = 0;

// No network message state
bool showingNoNetworkMessage = false;
unsigned long noNetworkMessageStartTime = 0;

// Mode enumeration
enum Mode { SCHEDULING, TEST_BULB, RESET_ESP, TIMER, SHOW_IP };
Mode currentMode = SCHEDULING;
int numModes = 4; // Default without SHOW_IP

void setup() {
  Serial.begin(115200);
  Wire.begin();

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  // Initialize relays (active LOW)
  for (int i = 0; i < numSwitches; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
    relayState[i] = false;
  }

  // Initialize switches (active-high, external pull-down)
  for (int i = 0; i < numSwitches; i++) {
    pinMode(switchPins[i], INPUT);
  }

  // Try to connect with saved credentials
  preferences.begin("wifi", true);
  String savedSsid = preferences.getString("ssid", "");
  String savedPassword = preferences.getString("password", "");
  preferences.end();

  if (savedSsid.length() > 0) {
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
      numModes = 5; // Include SHOW_IP mode
    } else {
      Serial.println("Failed to connect to saved WiFi, starting AP mode");
      WiFi.disconnect();
      WiFi.mode(WIFI_AP);
      WiFi.softAP(apSSID, apPassword);
      Serial.println("AP mode started, IP: 192.168.4.1");
    }
  } else {
    Serial.println("No WiFi credentials found, starting AP mode");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
    Serial.println("AP mode started, IP: 192.168.4.1");
  }

  // Initialize NTP client
  timeClient.begin();

  // Load saved schedules
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

  // Setup web server
  server.on("/", handleRoot);
  server.on("/setWiFi", HTTP_POST, handleSetWiFi);
  server.on("/setWiFi", HTTP_OPTIONS, handleOptions);
  server.on("/disconnect", HTTP_POST, handleDisconnect);
  server.on("/set", HTTP_POST, handleSet);
  server.on("/setSingle", HTTP_POST, handleSetSingle);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/toggle", HTTP_POST, handleToggle);
  server.on("/getStatus", HTTP_GET, handleGetStatus);
  server.on("/set", HTTP_OPTIONS, handleOptions);
  server.on("/setSingle", HTTP_OPTIONS, handleOptions);
  server.on("/toggle", HTTP_OPTIONS, handleOptions);
  server.on("/getStatus", HTTP_OPTIONS, handleOptions);
  server.begin();
  Serial.println("HTTP server started");

  startupStartTime = millis();
  updateLCD();
}

void loop() {
  // Handle serial input for CLEAR_WIFI
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      serialInput.trim();
      if (serialInput.equalsIgnoreCase("CLEAR_WIFI")) {
        Serial.println("Clearing WiFi credentials and restarting");
        preferences.begin("wifi", false);
        preferences.clear();
        preferences.end();
        preferences.begin("relay_settings", false);
        preferences.clear();
        preferences.end();
        WiFi.disconnect();
        delay(500);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(apSSID, apPassword);
        isClientConnected = false;
        Serial.println("Switched to AP mode, restarting...");
        delay(1000);
        ESP.restart();
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

  // Handle web server
  server.handleClient();

  // Update NTP and relay states if WiFi is connected
  static unsigned long lastNTPUpdate = 0;
  if (WiFi.status() == WL_CONNECTED && millis() - lastNTPUpdate >= 1000) {
    timeClient.update();
    lastNTPUpdate = millis();
    if (currentMode == SCHEDULING && !inMenu && !schedulingMode) {
      int currentHour = timeClient.getHours();
      int currentMinute = timeClient.getMinutes();
      updateRelayState(currentHour, currentMinute);
    }
  }

  // Handle startup transition
  if (startupState == SHOW_AP && millis() - startupStartTime >= STARTUP_AP_DISPLAY_DURATION) {
    startupState = SHOW_MENU;
    inMenu = true;
    updateLCD();
  }

  // Handle no network message timeout
  if (showingNoNetworkMessage && millis() - noNetworkMessageStartTime >= NO_NETWORK_MESSAGE_DURATION) {
    showingNoNetworkMessage = false;
    inMenu = true;
    updateLCD();
  }

  // Handle timer countdown and blinking
  handleTimer();

  // Update blinking cursor
  if ((inMenu || (currentMode == TIMER && timerState == TIMER_SETUP) || 
       (currentMode == SCHEDULING && schedulingMode)) && 
      millis() - lastCursorBlinkTime >= CURSOR_BLINK_INTERVAL) {
    cursorState = !cursorState;
    lastCursorBlinkTime = millis();
    updateLCD();
  }

  // Force LCD update for real-time clock in scheduling mode
  if (currentMode == SCHEDULING && !inMenu && !schedulingMode) {
    updateLCD();
  }
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
        if (startupState == SHOW_AP || showingNoNetworkMessage) {
          return; // Ignore switches during AP display or no network message
        }
        if (inMenu) {
          if (index == 2) { // Switch 3 (Up)
            if (cursorPosition > 0) {
              cursorPosition--;
              // Scroll up if cursor is at the top of the visible window
              if (cursorPosition < menuOffset) {
                menuOffset--;
              }
            } else {
              cursorPosition = numModes - 1; // Wrap to last mode
              menuOffset = max(0, numModes - 4); // Show last 4 modes
            }
            Serial.printf("Switch 3 pressed, moved cursor to Mode %d: %s, offset: %d\n", 
                          cursorPosition + 1, modeToString(cursorPosition), menuOffset);
            updateLCD();
          } else if (index == 3) { // Switch 4 (Down)
            if (cursorPosition < numModes - 1) {
              cursorPosition++;
              // Scroll down if cursor is at the bottom of the visible window
              if (cursorPosition > menuOffset + 3) {
                menuOffset++;
              }
            } else {
              cursorPosition = 0; // Wrap to first mode
              menuOffset = 0; // Reset to top
            }
            Serial.printf("Switch 4 pressed, moved cursor to Mode %d: %s, offset: %d\n", 
                          cursorPosition + 1, modeToString(cursorPosition), menuOffset);
            updateLCD();
          } else if (index == 0) { // Switch 1 (Select)
            currentMode = (Mode)cursorPosition;
            if (currentMode == SCHEDULING && WiFi.status() != WL_CONNECTED) {
              showingNoNetworkMessage = true;
              noNetworkMessageStartTime = millis();
              Serial.println("SCHEDULING selected but no network, showing message");
              updateLCD();
              return;
            }
            inMenu = false;
            for (int i = 0; i < numSwitches; i++) {
              if (currentMode != SCHEDULING) {
                digitalWrite(relayPins[i], HIGH);
                relayState[i] = false;
              }
            }
            if (currentMode == TIMER) {
              timerState = TIMER_SETUP;
              timerField = 0;
              timerValues[0] = 0;
              timerValues[1] = 0;
              timerValues[2] = 0;
            } else if (currentMode == SCHEDULING) {
              schedulingMode = false;
              selectedRelay = 0;
              selectedParam = 0;
            }
            Serial.printf("Switch 1 pressed, selected Mode %d: %s\n", 
                          currentMode + 1, modeToString(currentMode));
            updateLCD();
          }
        } else if (currentMode == TEST_BULB) {
          if (index < numSwitches) {
            relayState[index] = !relayState[index];
            digitalWrite(relayPins[index], relayState[index] ? LOW : HIGH);
            Serial.printf("Button %d pressed, Relay %d toggled to %s\n", 
                          index + 1, index + 1, relayState[index] ? "ON" : "OFF");
            updateLCD();
          }
        } else if (currentMode == RESET_ESP) {
          if (index == 2) {
            Serial.println("Switch 3 pressed in RESET ESP mode. Returning to menu.");
            inMenu = true;
            updateLCD();
          } else if (index == 3) {
            Serial.println("Switch 4 pressed in RESET ESP mode. Clearing settings and restarting...");
            preferences.begin("wifi", false);
            preferences.clear();
            preferences.end();
            preferences.begin("relay_settings", false);
            preferences.clear();
            preferences.end();
            delay(100);
            ESP.restart();
          }
        } else if (currentMode == TIMER) {
          if (timerState == TIMER_SETUP) {
            if (index == 2) {
              if (timerField == 0) {
                timerValues[0] = (timerValues[0] + 1) % 24;
              } else if (timerField == 1) {
                timerValues[1] = (timerValues[1] + 1) % 60;
              } else if (timerField == 2) {
                timerValues[2] = (timerValues[2] + 1) % 60;
              }
              Serial.printf("Switch 3 pressed, %s set to %d\n", 
                            timerField == 0 ? "Hours" : timerField == 1 ? "Minutes" : "Seconds", 
                            timerValues[timerField]);
              updateLCD();
            } else if (index == 3) {
              if (timerField == 0) {
                timerValues[0] = (timerValues[0] == 0) ? 23 : timerValues[0] - 1;
              } else if (timerField == 1) {
                timerValues[1] = (timerValues[1] == 0) ? 59 : timerValues[1] - 1;
              } else if (timerField == 2) {
                timerValues[2] = (timerValues[2] == 0) ? 59 : timerValues[2] - 1;
              }
              Serial.printf("Switch 4 pressed, %s set to %d\n", 
                            timerField == 0 ? "Hours" : timerField == 1 ? "Minutes" : "Seconds", 
                            timerValues[timerField]);
              updateLCD();
            } else if (index == 0) {
              timerField++;
              if (timerField > 2) {
                if (timerValues[0] == 0 && timerValues[1] == 0 && timerValues[2] == 0) {
                  timerField = 0;
                  Serial.println("Invalid timer (00:00:00), please set a duration.");
                } else {
                  timerState = TIMER_RUNNING;
                  timerStartTime = millis();
                  lastTimerUpdate = millis();
                  Serial.printf("Switch 1 pressed, timer started: %02d:%02d:%02d\n", 
                                timerValues[0], timerValues[1], timerValues[2]);
                }
              }
              updateLCD();
            }
          } else if (timerState == TIMER_RUNNING || timerState == TIMER_PAUSED) {
            if (index == 0) {
              timerState = (timerState == TIMER_RUNNING) ? TIMER_PAUSED : TIMER_RUNNING;
              Serial.printf("Switch 1 pressed, timer %s\n", 
                            timerState == TIMER_PAUSED ? "paused" : "resumed");
              updateLCD();
            }
          } else if (timerState == TIMER_BLINKING && index == 0) {
            for (int i = 0; i < numSwitches; i++) {
              digitalWrite(relayPins[i], HIGH);
              relayState[i] = false;
            }
            timerState = TIMER_SETUP;
            timerField = 0;
            timerValues[0] = 0;
            timerValues[1] = 0;
            timerValues[2] = 0;
            Serial.println("Switch 1 pressed, stopped blinking, returned to timer setup.");
            updateLCD();
          }
        } else if (currentMode == SCHEDULING) {
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
                onHour1 = tempOnHour[0]; onMinute1 = tempOnMinute[0];
                offHour1 = tempOffHour[0]; offMinute1 = tempOffMinute[0];
                onHour2 = tempOnHour[1]; onMinute2 = tempOnMinute[1];
                offHour2 = tempOffHour[1]; offMinute2 = tempOffMinute[1];
                onHour3 = tempOnHour[2]; onMinute3 = tempOnMinute[2];
                offHour3 = tempOffHour[2]; offMinute3 = tempOffMinute[2];
                onHour4 = tempOnHour[3]; onMinute4 = tempOnMinute[3];
                offHour4 = tempOffHour[3]; offMinute4 = tempOffMinute[3];
                saveSchedule();
                Serial.println("Exiting scheduling mode, schedule saved");
              } else {
                selectedParam = 0;
                Serial.printf("Selected Relay %d\n", selectedRelay);
              }
            }
            updateLCD();
          } else if (schedulingMode) {
            if (index == 1) {
              selectedParam = (selectedParam + 1) % 4;
              Serial.printf("Selected parameter: %s\n", 
                            selectedParam == 0 ? "onHour" : 
                            selectedParam == 1 ? "onMinute" : 
                            selectedParam == 2 ? "offHour" : "offMinute");
              updateLCD();
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
              updateLCD();
            } else if (index == 3) {
              int relayIndex = selectedRelay - 1;
              if (selectedParam == 0) {
                tempOnHour[relayIndex] = (tempOnHour[relayIndex] - 1 + 24) % 24;
              } else if (selectedParam == 1) {
                tempOnMinute[relayIndex] = (tempOnMinute[relayIndex] - 1 + 60) % 60;
              } else if (selectedParam == 2) {
                tempOffHour[relayIndex] = (tempOffHour[relayIndex] - 1 + 24) % 24;
              } else if (selectedParam == 3) {
                tempOffMinute[relayIndex] = (tempOffMinute[relayIndex] - 1 + 60) % 60;
              }
              Serial.printf("Decremented Relay %d %s to %d\n", 
                            selectedRelay, 
                            selectedParam == 0 ? "onHour" : 
                            selectedParam == 1 ? "onMinute" : 
                            selectedParam == 2 ? "offHour" : "offMinute",
                            selectedParam == 0 ? tempOnHour[relayIndex] :
                            selectedParam == 1 ? tempOnMinute[relayIndex] :
                            selectedParam == 2 ? tempOffHour[relayIndex] : tempOffMinute[relayIndex]);
              updateLCD();
            }
          }
        } else if (currentMode == SHOW_IP) {
          // No actions needed, just display IP
        }
      }
    }
  }

  // Handle Switch 2 long press for exit
  if (index == 1 && !inMenu && !showingNoNetworkMessage && startupState != SHOW_AP) {
    if (buttonState[index] == HIGH) {
      if (!isExiting) {
        switch2PressStartTime = millis();
        isExiting = true;
      }
      unsigned long holdTime = millis() - switch2PressStartTime;
      if (holdTime >= EXIT_DURATION) {
        for (int i = 0; i < numSwitches; i++) {
          if (currentMode != SCHEDULING) {
            digitalWrite(relayPins[i], HIGH);
            relayState[i] = false;
          }
        }
        inMenu = true;
        isExiting = false;
        if (currentMode == TIMER) {
          timerState = TIMER_SETUP;
          timerField = 0;
          timerValues[0] = 0;
          timerValues[1] = 0;
          timerValues[2] = 0;
        } else if (currentMode == SCHEDULING) {
          schedulingMode = false;
          selectedRelay = 0;
          selectedParam = 0;
        }
        Serial.println("Switch 2 held for 3s, returning to mode selection menu.");
        updateLCD();
      }
    } else if (isExiting) {
      isExiting = false;
    }
  }

  lastButtonState[index] = reading;
}

void updateLCD() {
  static int lastMode = -1;
  static bool lastInMenu = false;
  static int lastCursorPosition = -1;
  static int lastMenuOffset = -1; // Track menu offset
  static bool lastCursorState = false;
  static bool lastRelayState[numSwitches] = {false, false, false, false};
  static int lastButtonState[numSwitches] = {LOW, LOW, LOW, LOW};
  static TimerState lastTimerState = TIMER_SETUP;
  static int lastTimerField = -1;
  static int lastTimerValues[3] = {-1, -1, -1};
  static unsigned long lastRemainingTime = 0;
  static bool lastSchedulingMode = false;
  static int lastSelectedRelay = -1;
  static int lastSelectedParam = -1;
  static int lastTempValues[4][4] = {{-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}};
  static StartupState lastStartupState = SHOW_AP;
  static bool lastShowingNoNetworkMessage = false;
  static int lastScrollOffset = -1; // Track scroll offset for relays
  static int lastHour = -1, lastMinute = -1, lastSecond = -1;
  static bool lastWiFiConnected = false;

  unsigned long currentTime = millis();
  static unsigned long lastLCDUpdate = 0;
  static int scrollOffset = 0; // Scroll offset for relays (0 to 1)
  static unsigned long lastSwitchTime = 0;

  // Update scroll offset for scheduling mode
  if (currentMode == SCHEDULING && !schedulingMode && !inMenu && 
      currentTime - lastSwitchTime >= DISPLAY_SWITCH_INTERVAL) {
    scrollOffset = (scrollOffset + 1) % 2; // Scroll between 0 and 1 (R1-R3, R2-R4)
    lastSwitchTime = currentTime;
  }

  // Only update LCD if interval has passed or state changed
  if (currentTime - lastLCDUpdate < LCD_UPDATE_INTERVAL && 
      lastMode == currentMode && lastInMenu == inMenu && 
      lastCursorPosition == cursorPosition && lastMenuOffset == menuOffset && 
      lastCursorState == cursorState && lastTimerState == timerState && 
      lastTimerField == timerField && lastSchedulingMode == schedulingMode && 
      lastSelectedRelay == selectedRelay && lastSelectedParam == selectedParam && 
      lastStartupState == startupState && lastShowingNoNetworkMessage == showingNoNetworkMessage && 
      lastScrollOffset == scrollOffset && 
      lastWiFiConnected == (WiFi.status() == WL_CONNECTED)) {
    bool stateChanged = false;
    for (int i = 0; i < numSwitches; i++) {
      stateChanged |= (buttonState[i] != lastButtonState[i] || relayState[i] != lastRelayState[i]);
    }
    if (timerState == TIMER_SETUP) {
      for (int i = 0; i < 3; i++) {
        stateChanged |= (timerValues[i] != lastTimerValues[i]);
      }
    } else if (timerState == TIMER_RUNNING || timerState == TIMER_PAUSED) {
      unsigned long remainingTime = calculateRemainingTime();
      stateChanged |= (remainingTime != lastRemainingTime);
    }
    if (schedulingMode) {
      int relayIndex = selectedRelay - 1;
      if (relayIndex >= 0) {
        stateChanged |= (tempOnHour[relayIndex] != lastTempValues[relayIndex][0] ||
                         tempOnMinute[relayIndex] != lastTempValues[relayIndex][1] ||
                         tempOffHour[relayIndex] != lastTempValues[relayIndex][2] ||
                         tempOffMinute[relayIndex] != lastTempValues[relayIndex][3]);
      }
    }
    if (currentMode == SCHEDULING && !schedulingMode) {
      int currentHour = timeClient.getHours();
      int currentMinute = timeClient.getMinutes();
      int currentSecond = timeClient.getSeconds();
      stateChanged |= (currentHour != lastHour || currentMinute != lastMinute || currentSecond != lastSecond);
    }
    if (!stateChanged) {
      return;
    }
  }
  lastLCDUpdate = currentTime;

  lcd.clear();
  lcd.setCursor(0, 0);

  if (startupState == SHOW_AP) {
    displayAPMode();
  } else if (showingNoNetworkMessage) {
    lcd.print("No Network!");
    lcd.setCursor(0, 1);
    lcd.print("Scheduling");
    lcd.setCursor(0, 2);
    lcd.print("Disabled");
  } else if (inMenu) {
    // Display up to 4 modes starting from menuOffset
    for (int i = 0; i < 4;  i++) {
      int modeIndex = menuOffset + i;
      if (modeIndex < numModes) {
        lcd.setCursor(0, i);
        lcd.print(modeToString(modeIndex));
        if (modeIndex == cursorPosition) {
          lcd.setCursor(18, i);
          lcd.print(cursorState ? "*" : " ");
        }
      }
    }
  } else {
    if (currentMode == SCHEDULING) {
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
        
        lcd.print("Edit: " + relayStr + " " + paramStr);
        lcd.setCursor(0, 1);
        lcd.print("Value: " + valueStr);
        lcd.setCursor(0, 2);
        lcd.print("S1:Next S2:Param");
        lcd.setCursor(0, 3);
        lcd.print("S3:+ S4:-");
      } else {
        int currentHour = timeClient.getHours();
        int currentMinute = timeClient.getMinutes();
        int currentSecond = timeClient.getSeconds();
        String timeStr = "Time: " + formatTime(currentHour, currentMinute) + ":" + 
                         (currentSecond < 10 ? "0" + String(currentSecond) : String(currentSecond)) + 
                         " W:ON";
        String R1Str = "R1ON:" + formatTime(onHour1, onMinute1) + " OFF:" + formatTime(offHour1, offMinute1);
        String R2Str = "R2ON:" + formatTime(onHour2, onMinute2) + " OFF:" + formatTime(offHour2, offMinute2);
        String R3Str = "R3ON:" + formatTime(onHour3, onMinute3) + " OFF:" + formatTime(offHour3, offMinute3);
        String R4Str = "R4ON:" + formatTime(onHour4, onMinute4) + " OFF:" + formatTime(offHour4, offMinute4);
        
        lcd.print(timeStr);
        // Display three relays based on scrollOffset
        String relayStrings[] = {R1Str, R2Str, R3Str, R4Str};
        for (int i = 0; i < 3; i++) {
          int relayIndex = (scrollOffset + i) % 4; // Cycle through 0-3
          lcd.setCursor(0, i + 1);
          lcd.print(relayStrings[relayIndex]);
        }
        Serial.printf("LCD displaying Relays %d-%d\n", scrollOffset + 1, scrollOffset + 3);
        lastHour = currentHour;
        lastMinute = currentMinute;
        lastSecond = currentSecond;
      }
    } else if (currentMode == TEST_BULB) {
      for (int i = 0; i < numSwitches; i++) {
        String switchStatus = buttonState[i] == HIGH ? "ON " : "OFF";
        String relayStatus = relayState[i] ? "ON " : "OFF";
        lcd.setCursor(0, i);
        lcd.printf("S%d:%s R%d:%s", i + 1, switchStatus.c_str(), i + 1, relayStatus.c_str());
      }
    } else if (currentMode == RESET_ESP) {
      lcd.print("Reset ESP32?");
      lcd.setCursor(0, 1);
      lcd.print("SW4: Yes, SW3: No");
    } else if (currentMode == TIMER) {
      if (timerState == TIMER_SETUP) {
        lcd.print("Set Timer:");
        lcd.setCursor(0, 1);
        lcd.printf("%02d:%02d:%02d", timerValues[0], timerValues[1], timerValues[2]);
        int cursorCol = (timerField == 0) ? 0 : (timerField == 1) ? 3 : 6;
        lcd.setCursor(cursorCol, 2);
        lcd.print(cursorState ? "*" : " ");
      } else if (timerState == TIMER_RUNNING || timerState == TIMER_PAUSED) {
        unsigned long remainingTime = calculateRemainingTime();
        int hours = remainingTime / 3600;
        int minutes = (remainingTime % 3600) / 60;
        int seconds = remainingTime % 60;
        lcd.print(timerState == TIMER_RUNNING ? "Timer Running:" : "Timer Paused:");
        lcd.setCursor(0, 1);
        lcd.printf("%02d:%02d:%02d", hours, minutes, seconds);
        lcd.setCursor(0, 2);
        lcd.print("SW1: ");
        lcd.print(timerState == TIMER_RUNNING ? "Pause" : "Resume");
      } else if (timerState == TIMER_BLINKING) {
        lcd.print("Timer Done!");
        lcd.setCursor(0, 1);
        lcd.print("SW1: Stop");
      }
    } else if (currentMode == SHOW_IP) {
      lcd.print("WiFi Connected");
      lcd.setCursor(0, 1);
      lcd.print("IP Address:");
      lcd.setCursor(0, 2);
      lcd.print(WiFi.localIP().toString());
      lcd.setCursor(0, 3);
      lcd.print("Hold SW2 to Exit");
    }
  }

  // Update last states
  lastMode = currentMode;
  lastInMenu = inMenu;
  lastCursorPosition = cursorPosition;
  lastMenuOffset = menuOffset;
  lastCursorState = cursorState;
  lastTimerState = timerState;
  lastTimerField = timerField;
  lastSchedulingMode = schedulingMode;
  lastSelectedRelay = selectedRelay;
  lastSelectedParam = selectedParam;
  lastStartupState = startupState;
  lastShowingNoNetworkMessage = showingNoNetworkMessage;
  lastScrollOffset = scrollOffset;
  lastWiFiConnected = (WiFi.status() == WL_CONNECTED);
  for (int i = 0; i < numSwitches; i++) {
    lastButtonState[i] = buttonState[i];
    lastRelayState[i] = relayState[i];
  }
  for (int i = 0; i < 3; i++) {
    lastTimerValues[i] = timerValues[i];
  }
  for (int i = 0; i < 4; i++) {
    lastTempValues[i][0] = tempOnHour[i];
    lastTempValues[i][1] = tempOnMinute[i];
    lastTempValues[i][2] = tempOffHour[i];
    lastTempValues[i][3] = tempOffMinute[i];
  }
  lastRemainingTime = calculateRemainingTime();
}

unsigned long calculateRemainingTime() {
  if (timerState != TIMER_RUNNING && timerState != TIMER_PAUSED) {
    return 0;
  }
  unsigned long totalSeconds = timerValues[0] * 3600 + timerValues[1] * 60 + timerValues[2];
  if (timerState == TIMER_PAUSED) {
    return totalSeconds;
  }
  unsigned long elapsed = (millis() - timerStartTime) / 1000;
  return (totalSeconds > elapsed) ? totalSeconds - elapsed : 0;
}

void handleTimer() {
  if (currentMode != TIMER || inMenu) {
    return;
  }

  if (timerState == TIMER_RUNNING) {
    unsigned long currentTime = millis();
    if (currentTime - lastTimerUpdate >= TIMER_UPDATE_INTERVAL) {
      unsigned long remainingTime = calculateRemainingTime();
      if (remainingTime == 0) {
        timerState = TIMER_BLINKING;
        lastBlinkTime = currentTime;
        relaysBlinkingOn = true;
        for (int i = 0; i < numSwitches; i++) {
          digitalWrite(relayPins[i], LOW);
          relayState[i] = true;
        }
        Serial.println("Timer reached zero, starting relay blinking.");
        updateLCD();
      }
      lastTimerUpdate = currentTime;
      updateLCD();
    }
  } else if (timerState == TIMER_BLINKING) {
    unsigned long currentTime = millis();
    if (currentTime - lastBlinkTime >= BLINK_INTERVAL) {
      relaysBlinkingOn = !relaysBlinkingOn;
      for (int i = 0; i < numSwitches; i++) {
        digitalWrite(relayPins[i], relaysBlinkingOn ? LOW : HIGH);
        relayState[i] = relaysBlinkingOn;
      }
      lastBlinkTime = currentTime;
      Serial.printf("Relays %s\n", relaysBlinkingOn ? "ON" : "OFF");
      updateLCD();
    }
  }
}

void updateRelayState(int hour, int minute) {
  if (currentMode == SCHEDULING && !inMenu && !schedulingMode && WiFi.status() == WL_CONNECTED) {
    if (hour == onHour1 && minute == onMinute1) {
      relayState[0] = true;
      digitalWrite(RELAY1_PIN, LOW);
    } else if (hour == offHour1 && minute == offMinute1) {
      relayState[0] = false;
      digitalWrite(RELAY1_PIN, HIGH);
    }
    if (hour == onHour2 && minute == onMinute2) {
      relayState[1] = true;
      digitalWrite(RELAY2_PIN, LOW);
    } else if (hour == offHour2 && minute == offMinute2) {
      relayState[1] = false;
      digitalWrite(RELAY2_PIN, HIGH);
    }
    if (hour == onHour3 && minute == onMinute3) {
      relayState[2] = true;
      digitalWrite(RELAY3_PIN, LOW);
    } else if (hour == offHour3 && minute == offMinute3) {
      relayState[2] = false;
      digitalWrite(RELAY3_PIN, HIGH);
    }
    if (hour == onHour4 && minute == onMinute4) {
      relayState[3] = true;
      digitalWrite(RELAY4_PIN, LOW);
    } else if (hour == offHour4 && minute == offMinute4) {
      relayState[3] = false;
      digitalWrite(RELAY4_PIN, HIGH);
    }
  }
}

String formatTime(int hour, int minute) {
  String h = (hour < 10) ? "0" + String(hour) : String(hour);
  String m = (minute < 10) ? "0" + String(minute) : String(minute);
  return h + ":" + m;
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
  Serial.println("LCD updated with AP mode info");
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
  Serial.println("App connected via /");
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
    Serial.println("Failed to parse JSON: No data provided");
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

  String ssid = doc["ssid"] | "";
  String password = doc["password"] | "";

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
    numModes = 5; // Enable SHOW_IP mode
    Serial.println("Connected to WiFi, IP: " + ip);
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
    WiFi.softAPdisconnect(true);
  } else {
    doc["success"] = false;
    doc["error"] = "Failed to connect to WiFi";
    String output;
    serializeJson(doc, output);
    server.send(500, "application/json", output);
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
    numModes = 4; // Disable SHOW_IP mode
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
  numModes = 4;
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
    Serial.println("handleSet: No data provided");
    return;
  }

  String rawJson = server.arg("plain");
  Serial.println("handleSet: Received JSON: " + rawJson);
  DeserializationError error = deserializeJson(doc, rawJson);
  if (error) {
    doc.clear();
    doc["success"] = false;
    doc["error"] = "Invalid JSON: " + String(error.c_str());
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    Serial.println("handleSet: JSON parse error: " + String(error.c_str()));
    return;
  }

  String onTime1 = doc["onTime1"] | "";
  String offTime1 = doc["offTime1"] | "";
  String onTime2 = doc["onTime2"] | "";
  String offTime2 = doc["offTime2"] | "";
  String onTime3 = doc["onTime3"] | "";
  String offTime3 = doc["offTime3"] | "";
  String onTime4 = doc["onTime4"] | "";
  String offTime4 = doc["offTime4"] | "";

  bool valid = true;
  String errorMsg = "";
  if (!validateTimeFormat(onTime1)) { valid = false; errorMsg += "Invalid onTime1: " + onTime1 + "; "; }
  if (!validateTimeFormat(offTime1)) { valid = false; errorMsg += "Invalid offTime1: " + offTime1 + "; "; }
  if (!validateTimeFormat(onTime2)) { valid = false; errorMsg += "Invalid onTime2: " + onTime2 + "; "; }
  if (!validateTimeFormat(offTime2)) { valid = false; errorMsg += "Invalid offTime2: " + offTime2 + "; "; }
  if (!validateTimeFormat(onTime3)) { valid = false; errorMsg += "Invalid onTime3: " + onTime3 + "; "; }
  if (!validateTimeFormat(offTime3)) { valid = false; errorMsg += "Invalid offTime3: " + offTime3 + "; "; }
  if (!validateTimeFormat(onTime4)) { valid = false; errorMsg += "Invalid onTime4: " + onTime4 + "; "; }
  if (!validateTimeFormat(offTime4)) { valid = false; errorMsg += "Invalid offTime4: " + offTime4 + "; "; }

  if (!valid) {
    doc.clear();
    doc["success"] = false;
    doc["error"] = "Invalid time format: " + errorMsg;
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    Serial.println("handleSet: Validation failed: " + errorMsg);
    return;
  }

  int tempOnHour1 = onTime1.substring(0, 2).toInt();
  int tempOnMinute1 = onTime1.substring(3, 5).toInt();
  int tempOffHour1 = offTime1.substring(0, 2).toInt();
  int tempOffMinute1 = offTime1.substring(3, 5).toInt();
  int tempOnHour2 = onTime2.substring(0, 2).toInt();
  int tempOnMinute2 = onTime2.substring(3, 5).toInt();
  int tempOffHour2 = offTime2.substring(0, 2).toInt();
  int tempOffMinute2 = offTime2.substring(3, 5).toInt();
  int tempOnHour3 = onTime3.substring(0, 2).toInt();
  int tempOnMinute3 = onTime3.substring(3, 5).toInt();
  int tempOffHour3 = offTime3.substring(0, 2).toInt();
  int tempOffMinute3 = offTime3.substring(3, 5).toInt();
  int tempOnHour4 = onTime4.substring(0, 2).toInt();
  int tempOnMinute4 = onTime4.substring(3, 5).toInt();
  int tempOffHour4 = offTime4.substring(0, 2).toInt();
  int tempOffMinute4 = offTime4.substring(3, 5).toInt();

  if (!validateTimeRange(tempOnHour1, tempOnMinute1) ||
      !validateTimeRange(tempOffHour1, tempOffMinute1) ||
      !validateTimeRange(tempOnHour2, tempOnMinute2) ||
      !validateTimeRange(tempOffHour2, tempOffMinute2) ||
      !validateTimeRange(tempOnHour3, tempOnMinute3) ||
      !validateTimeRange(tempOffHour3, tempOffMinute3) ||
      !validateTimeRange(tempOnHour4, tempOnMinute4) ||
      !validateTimeRange(tempOffHour4, tempOffMinute4)) {
    doc.clear();
    doc["success"] = false;
    doc["error"] = "Time values out of range";
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    Serial.println("handleSet: Time values out of range");
    return;
  }

  onHour1 = tempOnHour1; onMinute1 = tempOnMinute1;
  offHour1 = tempOffHour1; offMinute1 = tempOffMinute1;
  onHour2 = tempOnHour2; onMinute2 = tempOnMinute2;
  offHour2 = tempOffHour2; offMinute2 = tempOffMinute2;
  onHour3 = tempOnHour3; onMinute3 = tempOnMinute3;
  offHour3 = tempOffHour3; offMinute3 = tempOffMinute3;
  onHour4 = tempOnHour4; onMinute4 = tempOnMinute4;
  offHour4 = tempOffHour4; offMinute4 = tempOffMinute4;

  saveSchedule();

  doc.clear();
  doc["success"] = true;
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
  Serial.println("handleSet: Schedule saved successfully");
}

void handleSetSingle() {
  sendCorsHeaders();
  DynamicJsonDocument doc(256);
  if (!server.hasArg("plain")) {
    doc["success"] = false;
    doc["error"] = "No data provided";
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    Serial.println("handleSetSingle: No data provided");
    return;
  }

  String rawJson = server.arg("plain");
  DeserializationError error = deserializeJson(doc, rawJson);
  if (error) {
    doc.clear();
    doc["success"] = false;
    doc["error"] = "Invalid JSON: " + String(error.c_str());
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    Serial.println("handleSetSingle: JSON parse error: " + String(error.c_str()));
    return;
  }

  String relayStr = doc["relay"] | "";
  String type = doc["type"] | "";
  String time = doc["time"] | "";

  int relayIndex = relayStr.toInt() - 1;
  if (relayIndex < 0 || relayIndex > 3) {
    doc.clear();
    doc["success"] = false;
    doc["error"] = "Invalid relay number";
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    Serial.println("handleSetSingle: Invalid relay number: " + relayStr);
    return;
  }

  if (type != "onTime" && type != "offTime") {
    doc.clear();
    doc["success"] = false;
    doc["error"] = "Invalid type";
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    Serial.println("handleSetSingle: Invalid type: " + type);
    return;
  }

  if (!validateTimeFormat(time)) {
    doc.clear();
    doc["success"] = false;
    doc["error"] = "Invalid time format: " + time;
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    Serial.println("handleSetSingle: Invalid time format: " + time);
    return;
  }

  int hour = time.substring(0, 2).toInt();
  int minute = time.substring(3, 5).toInt();
  if (!validateTimeRange(hour, minute)) {
    doc.clear();
    doc["success"] = false;
    doc["error"] = "Time values out of range";
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    Serial.println("handleSetSingle: Time values out of range: " + time);
    return;
  }

  switch (relayIndex) {
    case 0:
      if (type == "onTime") { onHour1 = hour; onMinute1 = minute; }
      else { offHour1 = hour; offMinute1 = minute; }
      break;
    case 1:
      if (type == "onTime") { onHour2 = hour; onMinute2 = minute; }
      else { offHour2 = hour; offMinute2 = minute; }
      break;
    case 2:
      if (type == "onTime") { onHour3 = hour; onMinute3 = minute; }
      else { offHour3 = hour; offMinute3 = minute; }
      break;
    case 3:
      if (type == "onTime") { onHour4 = hour; onMinute4 = minute; }
      else { offHour4 = hour; offMinute4 = minute; }
      break;
  }

  saveSchedule();

  doc.clear();
  doc["success"] = true;
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
  Serial.println("handleSetSingle: Schedule updated for Relay " + String(relayIndex + 1) + " " + type + ": " + time);
}

bool validateTimeFormat(String time) {
  if (time.length() != 5 || time[2] != ':') return false;
  for (int i = 0; i < 5; i++) {
    if (i == 2) continue;
    if (!isDigit(time[i])) return false;
  }
  return true;
}

bool validateTimeRange(int hour, int minute) {
  return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
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
    Serial.println("handleToggle: No data provided");
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
    Serial.println("handleToggle: JSON parse error: " + String(error.c_str()));
    return;
  }

  String relayNum = doc["relay"] | "";
  String state = doc["state"] | "";
  
  int relayIndex = relayNum.toInt() - 1;
  bool newState = (state == "1");
  
  if (relayIndex >= 0 && relayIndex < numSwitches) {
    relayState[relayIndex] = newState;
    digitalWrite(relayPins[relayIndex], newState ? LOW : HIGH);
  } else {
    doc.clear();
    doc["success"] = false;
    doc["error"] = "Invalid relay number";
    String output;
    serializeJson(doc, output);
    server.send(400, "application/json", output);
    Serial.println("handleToggle: Invalid relay number: " + relayNum);
    return;
  }
  
  doc.clear();
  doc["success"] = true;
  doc["relay"] = relayNum;
  doc["state"] = state;
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
  Serial.println("handleToggle: Relay " + relayNum + " set to state " + state);
}

void handleGetStatus() {
  sendCorsHeaders();
  DynamicJsonDocument doc(1024);
  doc["success"] = true;
  JsonArray relays = doc.createNestedArray("relays");
  
  JsonObject relay1 = relays.createNestedObject();
  relay1["relay"] = 1;
  relay1["state"] = relayState[0] ? 1 : 0;
  relay1["onTime"] = formatTime(onHour1, onMinute1);
  relay1["offTime"] = formatTime(offHour1, offMinute1);
  
  JsonObject relay2 = relays.createNestedObject();
  relay2["relay"] = 2;
  relay2["state"] = relayState[1] ? 1 : 0;
  relay2["onTime"] = formatTime(onHour2, onMinute2);
  relay2["offTime"] = formatTime(offHour2, offMinute2);
  
  JsonObject relay3 = relays.createNestedObject();
  relay3["relay"] = 3;
  relay3["state"] = relayState[2] ? 1 : 0;
  relay3["onTime"] = formatTime(onHour3, onMinute3);
  relay3["offTime"] = formatTime(offHour3, offMinute3);
  
  JsonObject relay4 = relays.createNestedObject();
  relay4["relay"] = 4;
  relay4["state"] = relayState[3] ? 1 : 0;
  relay4["onTime"] = formatTime(onHour4, onMinute4);
  relay4["offTime"] = formatTime(offHour4, offMinute4);
  
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
  Serial.println("handleGetStatus: Sent status");
}

void handleOptions() {
  sendCorsHeaders();
  server.send(200, "text/plain", "");
}

void saveSchedule() {
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
  Serial.println("saveSchedule: Schedule saved to Preferences");
}

String modeToString(int mode) {
  switch (mode) {
    case SCHEDULING: return "SCHEDULING";
    case TEST_BULB: return "TEST BULB";
    case RESET_ESP: return "RESET ESP";
    case TIMER: return "TIMER MODE";
    case SHOW_IP: return "SHOW IP";
    default: return "UNKNOWN";
  }
}