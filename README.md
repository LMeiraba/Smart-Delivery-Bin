# Decentralized Smart Delivery Bin (IoT)

A complete Node.js backend and modern web dashboard for an IoT-enabled Smart Delivery Bin. The system allows an apartment owner to generate a one-time password (OTP) via a web dashboard, which is instantly sent to a courier's WhatsApp. The courier inputs the OTP on the physical bin to unlock it.

## Features

- **WhatsApp Integration**: Uses `@whiskeysockets/baileys` to send OTPs and instructions to the courier directly on WhatsApp, bypassing SMS costs.
- **Modern Dashboard**: A clean, dark-themed, glassmorphism web dashboard to manage and monitor active deliveries.
- **In-Memory TTL Caching**: Uses `node-cache` for high performance, lightweight state management. OTPs automatically expire after 5 minutes.
- **Security & Constraints**: 
  - Strictly limits 1 active OTP per Delivery Bin.
  - Enforces a 5-minute cooldown per Courier Phone Number.
  - Validates and enforces proper 10-digit phone number formatting.

## Prerequisites

- [Node.js](https://nodejs.org/) (v16 or higher)
- WhatsApp app on your phone (for scanning the QR code and linking the bot)

## Installation

1. Clone the repository:
   ```bash
   git clone <your-repo-url>
   cd delivery-box
   ```

2. Install dependencies:
   ```bash
   npm install
   ```

## Usage

1. Start the server:
   ```bash
   npm start
   ```

2. **Authenticate the Bot**: Check your terminal. A QR code will be printed. Open WhatsApp on your phone, go to **Linked Devices**, and scan the QR code. Your session will be securely saved in `auth_info_baileys/` (ignored by git).

3. **Access the Dashboard**: Open your web browser and navigate to `http://localhost:3000`.

4. **Generate OTP**: Enter a Box ID, Courier Phone Number, and optionally a Package Title and Instructions. Click Generate, and the courier will instantly receive the OTP.

## API Endpoints

### `POST /api/generate-otp`
Generates an OTP and sends it via WhatsApp.
- **Body**: `{ boxId: "string", phoneNumber: "string", title?: "string", description?: "string" }`

### `POST /api/verify-otp`
Endpoint intended for the ESP8266/ESP32 Edge Microcontroller to verify an entered OTP.
- **Body**: `{ boxId: "string", enteredCode: "string" }`
- **Response**: `{ status: "success", command: "UNLOCK" }` or `{ status: "fail", command: "KEEP_LOCKED" }`

### `GET /api/active-otps`
Returns currently active deliveries with masked phone numbers and remaining TTL.
