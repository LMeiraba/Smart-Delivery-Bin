#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include "config.h"
#include <ctype.h>

// --- PIN MAPPING ---
// OLED I2C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 21
#define OLED_SCL 22
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Keypad
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {19, 18, 5, 17};
byte colPins[COLS] = {16, 4, 27, 26}; // Avoided strapping pins 2 and 15
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Hardware (Safe Pins)
#define RELAY_PIN 23
#define TRIG_PIN 25
#define ECHO_PIN 34
#define SWITCH_PIN 33

// --- CONFIGURATION ---
// API_URL is now securely defined in config.h
Preferences preferences;
char boxId[40] = "BIN-001";
char ownerPhone[20] = "";
long emptyBaselineDistance = 0;

// Log & Limit Switch Timers
unsigned long lidOpenedTime = 0;
bool isLidOpen = false;
bool leftOpenAlertSent = false;

// State Machine
enum State { STATE_IDLE, STATE_VERIFYING, STATE_AWAIT_DEPOSIT };
State currentState = STATE_IDLE;
String currentInput = "";
bool shouldSaveConfig = false;
unsigned long lastInteractionTime = 0;
bool isScreenOff = false;

// Callback for WiFiManager
void saveConfigCallback() {
  shouldSaveConfig = true;
}

void setup() {
  Serial.begin(115200);
  
  // Init Pins
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Locked by default
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // Init OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed. Running in Serial-only mode."));
    // Removed infinite loop so we can test without OLED
  } else {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println(F("Booting up..."));
    display.display();
  }

  // Init NVS (Preferences)
  preferences.begin("smartbin", false);
  String savedBoxId = preferences.getString("boxId", "BIN-001");
  savedBoxId.toCharArray(boxId, 40);
  String savedPhone = preferences.getString("ownerPhone", "");
  savedPhone.toCharArray(ownerPhone, 20);
  emptyBaselineDistance = preferences.getLong("baseline", 50); // Default 50cm calibration

  // Dual Mode / Captive Portal
  WiFiManager wm;
  
  // Create custom input fields
  WiFiManagerParameter custom_box_id("boxid", "Smart Bin ID (e.g. BIN-001)", boxId, 40);
  WiFiManagerParameter custom_owner_phone("ownerphone", "Owner WhatsApp (e.g. 919876543210)", ownerPhone, 20);
  wm.addParameter(&custom_box_id);
  wm.addParameter(&custom_owner_phone);
  wm.setSaveConfigCallback(saveConfigCallback);

  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("Connecting to WiFi..."));
  display.println(F("If no connection,"));
  display.println(F("connect to AP:"));
  display.println(F("SmartBin-Setup"));
  display.display();

  // This will try to connect. If it fails, it spins up an AP named "SmartBin-Setup"
  if (!wm.autoConnect("SmartBin-Setup", "Admin1234")) {
    Serial.println(F("Failed to connect and hit timeout"));
    delay(3000);
    ESP.restart();
  }

  // Connected to Home Wi-Fi!
  if (shouldSaveConfig) {
    strcpy(boxId, custom_box_id.getValue());
    preferences.putString("boxId", String(boxId));
    strcpy(ownerPhone, custom_owner_phone.getValue());
    preferences.putString("ownerPhone", String(ownerPhone));
    Serial.println(F("Saved new config to NVS!"));
  }

  updateDisplay("ENTER OTP:");
}

void loop() {
  // Screen Saver Logic (turn off OLED after 30s of idle)
  if (currentState == STATE_IDLE && !isScreenOff && (millis() - lastInteractionTime > 30000)) {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    isScreenOff = true;
    Serial.println("Screen Saver Active (OLED off)");
  }

  // Limit Switch LID_LEFT_OPEN Logic
  if (digitalRead(SWITCH_PIN) == HIGH) {
    if (!isLidOpen) {
      isLidOpen = true;
      lidOpenedTime = millis();
      leftOpenAlertSent = false;
    } else if (!leftOpenAlertSent && (millis() - lidOpenedTime > 60000)) {
      sendLogToServer("LID_LEFT_OPEN");
      leftOpenAlertSent = true;
    }
  } else {
    if (isLidOpen) {
      isLidOpen = false;
      if (leftOpenAlertSent) {
        sendLogToServer("LID_CLOSED");
      }
    }
  }

  switch (currentState) {
    case STATE_IDLE:
      handleKeypadInput();
      break;
    case STATE_VERIFYING:
      verifyOTP();
      break;
    case STATE_AWAIT_DEPOSIT:
      awaitDeposit();
      break;
  }
}

void handleKeypadInput() {
  char key = keypad.getKey();
  
  // Serial fallback for testing without keypad
  if (Serial.available() > 0) {
    key = Serial.read();
    if (key == '\n' || key == '\r') return;
    key = toupper(key);
  }

  if (key) {
    // Wake up screen if necessary
    if (isScreenOff) {
      display.ssd1306_command(SSD1306_DISPLAYON);
      isScreenOff = false;
      Serial.println("Screen Woken Up");
    }
    lastInteractionTime = millis();

    if (key == '*' || key == '#') {
      currentInput = ""; // Clear input
      updateDisplay("ENTER OTP:");
    } 
    else if (key == 'A') {
      // Hidden calibration feature
      calibrateSensor();
    }
    else if (key == 'D') {
      // On-Demand AP Setup Mode
      updateDisplay("SETUP MODE");
      Serial.println("\n--- SETUP MODE ---");
      Serial.println("Starting Config Portal. Connect to 'SmartBin-Setup' Wi-Fi...");
      
      WiFiManager wm;
      WiFiManagerParameter custom_box_id("boxid", "Smart Bin ID (e.g. BIN-001)", boxId, 40);
      WiFiManagerParameter custom_owner_phone("ownerphone", "Owner WhatsApp (e.g. 919876543210)", ownerPhone, 20);
      wm.addParameter(&custom_box_id);
      wm.addParameter(&custom_owner_phone);
      wm.setSaveConfigCallback(saveConfigCallback);
      wm.setConfigPortalTimeout(120); // 2 minutes
      
      if(wm.startConfigPortal("SmartBin-Setup", "Admin1234")) {
        if (shouldSaveConfig) {
          strcpy(boxId, custom_box_id.getValue());
          preferences.putString("boxId", String(boxId));
          strcpy(ownerPhone, custom_owner_phone.getValue());
          preferences.putString("ownerPhone", String(ownerPhone));
          Serial.println(F("Saved new config to NVS!"));
        }
        Serial.println("Setup complete, rebooting...");
        delay(1000);
        ESP.restart();
      } else {
        Serial.println("Setup mode timed out. Resuming normal operation.");
      }
      currentInput = "";
      updateDisplay("ENTER OTP:");
    }
    else {
      currentInput += key;
      
      // Masking the display input for security
      String masked = "";
      for (int i=0; i<currentInput.length(); i++) masked += "*";
      
      display.clearDisplay();
      display.setCursor(0,0);
      display.println(F("ENTER OTP:"));
      display.setTextSize(2);
      display.println(masked);
      display.setTextSize(1);
      display.display();

      if (currentInput.length() == 4) {
        currentState = STATE_VERIFYING;
      }
    }
  }
}

void sendLogToServer(String event, String details = "") {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(5); 
    
    HTTPClient http;
    http.setTimeout(5000);
    
    String logUrl = String(API_URL);
    logUrl.replace("verify-otp", "log"); // Hack to reuse config URL base
    
    http.begin(client, logUrl);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"boxId\":\"" + String(boxId) + "\",\"ownerPhone\":\"" + String(ownerPhone) + "\",\"event\":\"" + event + "\",\"details\":\"" + details + "\"}";
    
    int httpResponseCode = http.POST(payload);
    Serial.println("[LOG SENDER] Event: " + event + " | Response: " + String(httpResponseCode));
    http.end();
  }
}

void verifyOTP() {
  updateDisplay("VERIFYING...");
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[NETWORK] Wi-Fi is connected. Starting HTTPS request...");
    WiFiClientSecure client;
    client.setInsecure(); // Bypass SSL verification
    
    // Set a strict timeout so it never hangs indefinitely (10 seconds)
    client.setTimeout(10); 
    
    HTTPClient http;
    http.setTimeout(10000); // 10000 milliseconds = 10 seconds
    
    Serial.println("[NETWORK] Connecting to: " + String(API_URL));
    http.begin(client, API_URL);
    http.addHeader("Content-Type", "application/json");

    // Construct the JSON payload
    String payload = "{\"boxId\":\"" + String(boxId) + "\",\"enteredCode\":\"" + currentInput + "\"}";
    Serial.println("[NETWORK] Sending Payload: " + payload);
    
    int httpResponseCode = http.POST(payload);
    Serial.println("[NETWORK] HTTP Response Code: " + String(httpResponseCode));

    if (httpResponseCode == 200) {
      String response = http.getString();
      Serial.println("[NETWORK] Server Response: " + response);
      if (response.indexOf("UNLOCK") > 0) {
        unlockBin();
        sendLogToServer("OPEN_SUCCESS");
        currentState = STATE_AWAIT_DEPOSIT;
        currentInput = "";
        http.end();
        return;
      }
    } else {
      Serial.println("[NETWORK] Request failed or invalid OTP!");
      if (httpResponseCode < 0) {
        Serial.println("[NETWORK] Error: " + http.errorToString(httpResponseCode));
      }
    }
    http.end();
  } else {
    Serial.println("\n[ERROR] Wi-Fi is NOT connected! Cannot verify OTP.");
  }
  
  // Failed or denied
  sendLogToServer("OPEN_FAIL", "OTP: " + currentInput);
  updateDisplay("ACCESS DENIED");
  delay(3000);
  currentInput = "";
  currentState = STATE_IDLE;
  updateDisplay("ENTER OTP:");
}

void unlockBin() {
  digitalWrite(RELAY_PIN, HIGH);
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println(F("UNLOCKED!"));
  display.setTextSize(1);
  display.println(F("Please insert package."));
  display.display();
}

long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout prevents hanging
  if (duration == 0) return 999; // Error / out of range
  return duration * 0.034 / 2;
}

void calibrateSensor() {
  updateDisplay("CALIBRATING...");
  long dist = getDistance();
  if (dist < 999 && dist > 0) {
      emptyBaselineDistance = dist;
      preferences.putLong("baseline", emptyBaselineDistance);
      updateDisplay("CALIBRATED!");
  } else {
      updateDisplay("CALIB FAILED");
  }
  delay(2000);
  currentInput = "";
  updateDisplay("ENTER OTP:");
}

void awaitDeposit() {
  long distance = getDistance();
  
  // If distance drops significantly below baseline, OR user types 'Y' in Serial
  bool simulatedDrop = false;
  if (Serial.available() > 0) {
    char c = toupper(Serial.read());
    if (c == 'Y') simulatedDrop = true;
  }

  if ((distance > 0 && distance < (emptyBaselineDistance - 10)) || simulatedDrop) {
    if (simulatedDrop) Serial.println("\n[SIMULATOR] Package drop detected via Serial!");
    
    updateDisplay("CLOSE LID");
    Serial.println("Waiting for limit switch to be pressed (lid closed)...");
    
    // Block infinitely until the limit switch is pressed (LOW)
    while (digitalRead(SWITCH_PIN) == HIGH) {
      delay(100);
    }
    
    digitalWrite(RELAY_PIN, LOW); // Retract Relay, locking the bin
    
    updateDisplay("SECURED!");
    sendLogToServer("PACKAGE_DEPOSITED");
    delay(3000);
    
    currentState = STATE_IDLE;
    lastInteractionTime = millis(); // Reset screen saver timer
    updateDisplay("ENTER OTP:");
  }
}

void updateDisplay(String msg) {
  Serial.println("\n[DISPLAY] " + msg);
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(msg);
  display.display();
}
