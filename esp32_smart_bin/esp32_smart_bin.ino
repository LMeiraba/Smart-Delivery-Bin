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
// Custom I2C pins for OLED on the left side
#define OLED_SDA 27
#define OLED_SCL 26
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Keypad (All 8 pins contiguously on the RIGHT side - BOOT button side)
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {15, 4, 16, 17}; 
byte colPins[COLS] = {5, 18, 19, 23}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Hardware (All other components moved to the LEFT side)
#define RELAY_PIN 25
#define TRIG_PIN 33
#define ECHO_PIN 32
#define SWITCH_PIN 14

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
WiFiManagerParameter* p_custom_box_id = NULL;
WiFiManagerParameter* p_custom_owner_phone = NULL;

void saveConfigCallback() {
  if (p_custom_box_id && p_custom_owner_phone) {
    Serial.println("--- CALLBACK TRIGGERED ---");
    
    // Print what WiFiManager captured
    String newPhone = String(p_custom_owner_phone->getValue());
    Serial.println("New Phone from Portal: '" + newPhone + "'");
    
    // Store in global array
    newPhone.toCharArray(ownerPhone, 20);
    preferences.putString("ownerPhone", String(ownerPhone));
    
    // Check Box ID as well
    String newBox = String(p_custom_box_id->getValue());
    newBox.toCharArray(boxId, 40);
    preferences.putString("boxId", String(boxId));
    
    Serial.println("NVS Save Complete! Memory now holds: " + String(ownerPhone));
    Serial.println("--------------------------");
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
    display.setTextColor(SSD1306_WHITE);
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
  
  Serial.println("\n--- DEBUG INFO ---");
  Serial.println("Loaded Box ID: " + String(boxId));
  Serial.println("Loaded Phone : '" + String(ownerPhone) + "'");
  Serial.println("------------------\n");

  // Dual Mode / Captive Portal
  WiFiManager wm;
  
  // Create custom input fields using global pointers
  if (!p_custom_box_id) p_custom_box_id = new WiFiManagerParameter("boxid", "Smart Bin ID (e.g. BIN-001)", boxId, 40);
  if (!p_custom_owner_phone) p_custom_owner_phone = new WiFiManagerParameter("ownerphone", "Owner WhatsApp (e.g. 919876543210)", ownerPhone, 20);
  
  wm.addParameter(p_custom_box_id);
  wm.addParameter(p_custom_owner_phone);
  wm.setSaveConfigCallback(saveConfigCallback);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println(F("SMART BIN SETUP"));
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  
  display.setCursor(0, 18);
  display.println(F("Connecting to WiFi.."));
  display.println(F("If failed, join AP:"));
  display.println(F("> SmartBin-Setup"));
  display.display();

  // This will try to connect. If it fails, it spins up an AP named "SmartBin-Setup"
  if (!wm.autoConnect("SmartBin-Setup")) {
    Serial.println(F("Failed to connect and hit timeout"));
    delay(3000);
    ESP.restart();
  }

  // We no longer need to manually check shouldSaveConfig here 
  // because it's handled instantly inside saveConfigCallback!

  display.clearDisplay();
  updateDisplay("ENTER OTP:");
}

void loop() {
  // Wi-Fi Connection Monitoring
  static bool wasConnected = true;
  if (WiFi.status() != WL_CONNECTED) {
    if (wasConnected) {
      wasConnected = false;
      updateDisplay("NO WI-FI!");
      Serial.println("[NETWORK] Wi-Fi Disconnected!");
    }
  } else {
    if (!wasConnected) {
      wasConnected = true;
      updateDisplay("ENTER OTP:");
      Serial.println("[NETWORK] Wi-Fi Reconnected!");
    }
  }

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
      
      if (currentState == STATE_IDLE) {
        sendLogToServer("TAMPER_ALERT", "Lid forced open without OTP!");
        // Optional: Rapidly toggle relay to make noise
        for (int i = 0; i < 5; i++) {
          digitalWrite(RELAY_PIN, HIGH); delay(100);
          digitalWrite(RELAY_PIN, LOW); delay(100);
        }
      }
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
      // Check Capacity on close
      delay(1000); // Wait for packages to settle
      long currentDist = getDistance();
      if (currentDist > 0 && currentDist < (emptyBaselineDistance * 0.25)) {
        sendLogToServer("BIN_FULL", "Capacity > 75% full");
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
      if (!p_custom_box_id) p_custom_box_id = new WiFiManagerParameter("boxid", "Smart Bin ID (e.g. BIN-001)", boxId, 40);
      if (!p_custom_owner_phone) p_custom_owner_phone = new WiFiManagerParameter("ownerphone", "Owner WhatsApp (e.g. 919876543210)", ownerPhone, 20);
      
      wm.addParameter(p_custom_box_id);
      wm.addParameter(p_custom_owner_phone);
      wm.setSaveConfigCallback(saveConfigCallback);
      
      // Strict 2-minute timeout so they can't get stuck forever
      wm.setConfigPortalTimeout(120); 
      wm.startConfigPortal("SmartBin-Setup");
      
      // If they clicked Save, saveConfigCallback was already executed.
      Serial.println("Exited Setup Mode. Resuming normal operation.");
      
      currentInput = "";
      updateDisplay("ENTER OTP:");
    }
    else {
      currentInput += key;
      
      // Show unmasked input for better UX
      display.clearDisplay();
      
      // Top Status Bar
      display.setTextSize(1);
      display.setCursor(0,0);
      display.print(boxId);
      display.setCursor(100,0);
      if (WiFi.status() == WL_CONNECTED) display.print("WiFi");
      else display.print("...");
      
      display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
      
      // OTP Entry
      display.setCursor(0, 25);
      display.println(F("Enter OTP:"));
      
      display.setTextSize(2);
      display.setCursor(0, 42);
      
      // Draw OTP with underscores for empty slots
      String displayStr = "";
      for (int i=0; i<4; i++) {
        if (i < currentInput.length()) displayStr += currentInput.charAt(i);
        else displayStr += "_";
        if (i < 3) displayStr += " ";
      }
      display.print(displayStr);
      
      display.setTextSize(1);
      display.display();

      if (currentInput.length() == 4) {
        currentState = STATE_VERIFYING;
      }
    }
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
        lastInteractionTime = millis(); // Start auto-relock timer
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
  // Auto-Relock if left alone for 30 seconds
  if (millis() - lastInteractionTime > 30000 && digitalRead(SWITCH_PIN) == LOW) {
      digitalWrite(RELAY_PIN, LOW); // Lock
      sendLogToServer("AUTO_RELOCKED", "Lid not opened in 30s");
      updateDisplay("RELOCKED!");
      delay(2000);
      currentState = STATE_IDLE;
      updateDisplay("Enter OTP:");
      return;
  }

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
  
  // Top Status Bar
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print(boxId);
  display.setCursor(100,0);
  if (WiFi.status() == WL_CONNECTED) display.print("WiFi");
  else display.print("...");
  
  display.drawLine(0, 10, 128, 10, WHITE);
  
  display.setCursor(0, 25);
  display.setTextSize(2);
  display.println(msg);
  display.setTextSize(1);
  display.display();
}
