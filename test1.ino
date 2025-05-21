#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP.h> // Include for ESP.restart()

// Define switch pins
#define SWITCH1_PIN 23
#define SWITCH4_PIN 32

// Define relay pins (active LOW, unused)
#define RELAY1_PIN 16
#define RELAY2_PIN 17
#define RELAY3_PIN 18
#define RELAY4_PIN 19

// Define other switch pins (unused)
#define SWITCH2_PIN 25
#define SWITCH3_PIN 26

// Debounce and timing settings
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long LONG_PRESS_DURATION = 1500; // 1.5 seconds

// Debounce and state variables for switch 1
unsigned long lastDebounceTime1 = 0;
int lastButtonState1 = LOW; // LOW = not pressed (pull-down)
int buttonState1 = LOW;     // LOW = not pressed (pull-down)
unsigned long buttonPressStartTime = 0; // Track when button is pressed
bool isButtonHeld = false;

// Debounce and state variables for switch 4
unsigned long lastDebounceTime4 = 0;
int lastButtonState4 = LOW; // LOW = not pressed (pull-down)
int buttonState4 = LOW;     // LOW = not pressed (pull-down)

// Mode tracking
int currentMode = 0; // 0 = Mode 1, 1 = Mode 2, 2 = Mode 3
bool isBlinking = false; // Track blinking state for current mode

// Blinking settings
unsigned long lastBlinkTime = 0;
const unsigned long BLINK_INTERVAL = 500; // Blink every 500ms
bool blinkState = true; // true = text on, false = text off

// Initialize LCD (0x27 is a common I2C address, adjust if different)
LiquidCrystal_I2C lcd(0x27, 20, 4);

void setup() {
  Serial.begin(115200);
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // Configure switch pins as INPUT
  pinMode(SWITCH1_PIN, INPUT); // Requires external 10kΩ pull-down resistor
  pinMode(SWITCH4_PIN, INPUT); // Requires external 10kΩ pull-down resistor
  
  Serial.println("ESP32 started. Switch 1 is active-high, cycling through modes. Switch 4 restarts ESP in Mode 3 blinking state.");
  updateLCD();
}

void handleSwitch1() {
  int reading = digitalRead(SWITCH1_PIN);

  // Check if button state changed
  if (reading != lastButtonState1) {
    lastDebounceTime1 = millis();
  }

  // Check if debounce delay has passed
  if ((millis() - lastDebounceTime1) > DEBOUNCE_DELAY) {
    if (reading != buttonState1) {
      buttonState1 = reading;
      
      // Button pressed (active-high: HIGH = pressed)
      if (buttonState1 == HIGH) {
        buttonPressStartTime = millis();
        isButtonHeld = true;
      }
      // Button released (active-high: LOW = released)
      else if (isButtonHeld) {
        unsigned long holdTime = millis() - buttonPressStartTime;
        
        // Short press: cycle modes if released before 1.5s
        if (holdTime < LONG_PRESS_DURATION && !isBlinking) {
          currentMode = (currentMode + 1) % 3;
          Serial.printf("Switch 1 short press, Mode %d: %s\n", 
                        currentMode + 1, 
                        currentMode == 0 ? "SCHEDULING MODE" : 
                        currentMode == 1 ? "TEST BULB Switch MODE" : "RESET ESP");
          isBlinking = false; // Ensure blinking stops when mode changes
          updateLCD(); // Update LCD immediately on mode change
        }
        isButtonHeld = false;
      }
    }
  }

  // Handle long press
  if (isButtonHeld && buttonState1 == HIGH) {
    unsigned long holdTime = millis() - buttonPressStartTime;
    
    if (!isBlinking && holdTime >= LONG_PRESS_DURATION) {
      // Enter blinking state for current mode
      isBlinking = true;
      Serial.printf("Mode %d text now blinking: %s\n", 
                    currentMode + 1, 
                    currentMode == 0 ? "SCHEDULING MODE" : 
                    currentMode == 1 ? "TEST BULB Switch MODE" : "RESET ESP");
      updateLCD(); // Update LCD immediately on entering blinking state
    }
    else if (isBlinking && holdTime >= LONG_PRESS_DURATION * 2) {
      // Exit blinking state, stay in same mode
      isBlinking = false;
      Serial.printf("Mode %d text stopped blinking: %s\n", 
                    currentMode + 1, 
                    currentMode == 0 ? "SCHEDULING MODE" : 
                    currentMode == 1 ? "TEST BULB Switch MODE" : "RESET ESP");
      isButtonHeld = false; // Reset to allow new long press detection
      buttonPressStartTime = millis(); // Reset timer
      updateLCD(); // Update LCD immediately on exiting blinking state
    }
  }

  lastButtonState1 = reading;
}

void handleSwitch4() {
  int reading = digitalRead(SWITCH4_PIN);

  // Check if button state changed
  if (reading != lastButtonState4) {
    lastDebounceTime4 = millis();
  }

  // Check if debounce delay has passed
  if ((millis() - lastDebounceTime4) > DEBOUNCE_DELAY) {
    if (reading != buttonState4) {
      buttonState4 = reading;
      
      // Button pressed (active-high: HIGH = pressed)
      if (buttonState4 == HIGH && isBlinking && currentMode == 2) {
        Serial.println("Switch 4 pressed in Mode 3 blinking state. Restarting ESP...");
        delay(100); // Brief delay to ensure serial message is sent
        ESP.restart(); // Restart the ESP32
      }
    }
  }

  lastButtonState4 = reading;
}

void updateLCD() {
  static int lastMode = -1; // Track last mode
  static bool lastBlinking = false; // Track last blinking state
  static bool lastBlinkState = !blinkState; // Track last blink state

  // Only update LCD if mode, blinking state, or blink state changed
  if (lastMode != currentMode || lastBlinking != isBlinking || lastBlinkState != blinkState) {
    lcd.clear();
    lcd.setCursor(0, 0);
    
    if (isBlinking && !blinkState) {
      // Leave LCD blank during "off" blink state
    } else {
      // Display text for current mode
      switch (currentMode) {
        case 0:
          lcd.print("SCHEDULING MODE");
          break;
        case 1:
          lcd.print("TEST BULB Switch MODE");
          break;
        case 2:
          lcd.print("RESET ESP");
          break;
      }
    }
    lastMode = currentMode;
    lastBlinking = isBlinking;
    lastBlinkState = blinkState;
  }
}

void loop() {
  handleSwitch1();
  handleSwitch4(); // Check Switch 4 state
  
  // Update blink state in blinking mode
  if (isBlinking && (millis() - lastBlinkTime >= BLINK_INTERVAL)) {
    blinkState = !blinkState;
    lastBlinkTime = millis();
    updateLCD(); // Update LCD when blink state changes
  }
}