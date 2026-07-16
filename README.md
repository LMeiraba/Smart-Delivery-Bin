# Smart Delivery Bin (IoT System)

A complete hardware and software solution for a decentralized IoT Smart Delivery Bin. The system consists of an ESP32 edge device, a Node.js backend, and a modern web dashboard. 

It allows you to generate a one-time password (OTP) via a web dashboard, which is instantly sent to a courier's WhatsApp. The courier inputs the OTP on the physical bin to unlock it. The bin tracks hardware events (lid open/close, deposits) and sends pooled Activity Reports back to your WhatsApp!

## 🌟 Key Features

- **WhatsApp Integration**: Uses `@whiskeysockets/baileys` to send OTPs to couriers and Activity Reports to the owner directly on WhatsApp, bypassing SMS costs.
- **ESP32 Edge Device**: 
  - Dual Mode (Normal / Setup Captive Portal)
  - 4x4 Matrix Keypad + OLED Display Interface
  - Ultrasonic Package Detection & Magnetic Lid Switch Sensors
  - Solenoid Relay Locking Mechanism
- **Modern Dashboard**: A clean, dark-themed, glassmorphism web dashboard to manage deliveries, view live logs, and generate OTPs.
- **In-Memory TTL Caching**: OTPs automatically expire after 5 minutes for security.
- **Robust Security**: 
  - Strictly limits 1 active OTP per Delivery Bin.
  - Enforces a 5-minute cooldown per Courier Phone Number.
  - Pin-protected Admin Controls (Clear Logs).

## 🛠️ Hardware Requirements (ESP32)

- ESP32 Development Board
- 4x4 Matrix Membrane Keypad
- SSD1306 0.96" OLED Display (I2C)
- HC-SR04 Ultrasonic Sensor
- Magnetic Reed Switch (Door Sensor)
- 5V Relay Module (to trigger 12V Solenoid Lock)
- 12V Power Supply + 12V Solenoid Lock

## 💻 Backend Installation (Node.js)

1. Clone the repository and install dependencies:
   ```bash
   git clone <your-repo-url>
   cd delivery-box
   npm install
   ```
2. Start the server:
   ```bash
   npm start
   ```
3. **Authenticate the Bot**: A QR code will print in the terminal. Open WhatsApp, go to **Linked Devices**, and scan it. The session saves securely in `auth_info_baileys/`.

## 🌐 API Endpoints & Architecture

- `POST /api/generate-otp` : Generates OTP & sends to courier via WhatsApp.
- `POST /api/verify-otp` : ESP32 endpoint to verify entered OTPs.
- `POST /api/logs` : ESP32 endpoint to push hardware logs (Lid Open, Deposited, etc.).
- `GET /api/logs/:boxId` : Fetches event history for a specific bin.
- `DELETE /api/logs/:boxId` : Admin endpoint to wipe event history (Requires PIN).
- `GET /api/active-otps` : Returns currently active deliveries.

## 🔌 ESP32 Setup & Wiring

The `esp32_smart_bin.ino` sketch uses the `WiFiManager` library to avoid hardcoding Wi-Fi credentials. 

To configure a new ESP32:
1. Boot the ESP32. If it cannot connect to Wi-Fi, it broadcasts `SmartBin-Setup`.
2. Connect to the network and navigate to `192.168.4.1` on your phone.
3. Enter your Home Wi-Fi credentials, your **Smart Bin ID** (e.g. `BIN-001`), and your **Owner WhatsApp Number** (10-digits).
4. Click Save. The ESP32 will reboot and connect!
