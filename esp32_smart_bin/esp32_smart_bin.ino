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
byte colPins[COLS] = {16, 4, 2, 15};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Hardware
#define RELAY_PIN 13
#define TRIG_PIN 12
#define ECHO_PIN 14

// --- CONFIGURATION ---
// API_URL is now securely defined in config.h
Preferences preferences;
char boxId[40] = "BIN-001";
long emptyBaselineDistance = 0;

// State Machine
enum State { STATE_IDLE, STATE_VERIFYING, STATE_AWAIT_DEPOSIT };
State currentState = STATE_IDLE;
String currentInput = "";
bool shouldSaveConfig = false;

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

  // Init OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println(F("Booting up..."));
  display.display();

  // Init NVS (Preferences)
  preferences.begin("smartbin", false);
  String savedBoxId = preferences.getString("boxId", "BIN-001");
  savedBoxId.toCharArray(boxId, 40);
  emptyBaselineDistance = preferences.getLong("baseline", 50); // Default 50cm calibration

  // Dual Mode / Captive Portal
  WiFiManager wm;
  
  // Create a custom input field on the captive portal to ask for the Box ID
  WiFiManagerParameter custom_box_id("boxid", "Smart Bin ID (e.g. BIN-001)", boxId, 40);
  wm.addParameter(&custom_box_id);
  wm.setSaveConfigCallback(saveConfigCallback);

  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("Connecting to WiFi..."));
  display.println(F("If no connection,"));
  display.println(F("connect to AP:"));
  display.println(F("SmartBin-Setup"));
  display.display();

  // This will try to connect. If it fails, it spins up an AP named "SmartBin-Setup"
  if (!wm.autoConnect("SmartBin-Setup")) {
    Serial.println(F("Failed to connect and hit timeout"));
    delay(3000);
    ESP.restart();
  }

  // Connected to Home Wi-Fi!
  if (shouldSaveConfig) {
    strcpy(boxId, custom_box_id.getValue());
    preferences.putString("boxId", String(boxId));
    Serial.println(F("Saved new Box ID to NVS!"));
  }

  updateDisplay("ENTER OTP:");
}

void loop() {
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
  if (key) {
    if (key == '*' || key == '#') {
      currentInput = ""; // Clear input
      updateDisplay("ENTER OTP:");
    } 
    else if (key == 'A') {
      // Hidden calibration feature
      calibrateSensor();
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

void verifyOTP() {
  updateDisplay("VERIFYING...");
  
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); // Bypass SSL verification to easily support HTTPS routing
    HTTPClient http;
    
    http.begin(client, API_URL);
    http.addHeader("Content-Type", "application/json");

    // Construct the JSON payload
    String payload = "{\"boxId\":\"" + String(boxId) + "\",\"enteredCode\":\"" + currentInput + "\"}";
    
    int httpResponseCode = http.POST(payload);

    if (httpResponseCode == 200) {
      String response = http.getString();
      if (response.indexOf("UNLOCK") > 0) {
        unlockBin();
        currentState = STATE_AWAIT_DEPOSIT;
        currentInput = "";
        http.end();
        return;
      }
    }
    http.end();
  }
  
  // Failed or denied
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
  
  // If distance drops significantly below baseline, the package has been deposited
  if (distance > 0 && distance < (emptyBaselineDistance - 10)) {
    // Wait 5 seconds to ensure the courier removes their hand and closes the lid
    delay(5000); 
    digitalWrite(RELAY_PIN, LOW); // Retract Relay, locking the bin
    
    updateDisplay("SECURED!");
    delay(3000);
    
    currentState = STATE_IDLE;
    updateDisplay("ENTER OTP:");
  }
}

void updateDisplay(String msg) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(msg);
  display.display();
}
