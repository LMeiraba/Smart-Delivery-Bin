import express from 'express';
import makeWASocket, { useMultiFileAuthState, DisconnectReason } from '@whiskeysockets/baileys';
import qrcode from 'qrcode-terminal';
import pino from 'pino';
import NodeCache from 'node-cache';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
app.use(express.urlencoded({ extended: true }));
app.use(express.json());
app.use(express.static('public', { extensions: ['html'] }));

// Cache with 5 minutes (300s) TTL
const cache = new NodeCache({ stdTTL: 300, checkperiod: 60 });
let sock; // Global variable to hold the WhatsApp socket connection

async function connectToWhatsApp() {
    const { state, saveCreds } = await useMultiFileAuthState('auth_info_baileys');

    sock = makeWASocket({
        auth: state,
        printQRInTerminal: false,
        logger: pino({ level: "silent" })
    });

    sock.ev.on('creds.update', saveCreds);

    sock.ev.on('connection.update', (update) => {
        const { connection, lastDisconnect, qr } = update;
        
        if (qr) {
            console.log("Scan this QR code with your WhatsApp app:");
            qrcode.generate(qr, { small: true });
        }
        
        if (connection === 'close') {
            const shouldReconnect = (lastDisconnect.error)?.output?.statusCode !== DisconnectReason.loggedOut;
            console.log('Connection closed due to error. Reconnecting:', shouldReconnect);
            if (shouldReconnect) connectToWhatsApp();
        } else if (connection === 'open') {
            console.log('WhatsApp Bot is connected and ready!');
        }
    });
}

connectToWhatsApp();

// --- API ENDPOINTS ---

app.post('/api/generate-otp', async (req, res) => {
    const { phoneNumber, boxId, title, description } = req.body;

    if (!phoneNumber || !boxId) {
        return res.status(400).json({ status: "error", message: "Phone Number and Box ID are required." });
    }

    let cleanPhone = phoneNumber.replace(/[\+\s\-]/g, '');
    if (cleanPhone.length === 10) cleanPhone = "91" + cleanPhone;
    
    if (!/^91\d{10}$/.test(cleanPhone)) {
        return res.status(400).json({ status: "error", message: "Only Indian phone numbers (10 digits) are supported." });
    }

    // Constraints Validation
    let boxKeyToDelete = null;
    const keys = cache.keys();
    for (const key of keys) {
        const data = cache.get(key);
        if (data) {
            if (data.phoneNumber === cleanPhone) {
                return res.status(400).json({ status: "error", message: "This phone number already has an active OTP (5 min cooldown)." });
            }
            if (data.boxId === boxId) {
                boxKeyToDelete = key;
            }
        }
    }
    if (boxKeyToDelete) cache.del(boxKeyToDelete);

    // Generate new unique 4-digit OTP
    let otp;
    do {
        otp = Math.floor(1000 + Math.random() * 9000).toString();
    } while (cache.has(otp));

    const chatId = `${cleanPhone}@s.whatsapp.net`;
    
    // Store in cache
    cache.set(otp, { phoneNumber: cleanPhone, boxId, title, description });
    console.log(`Generated OTP: ${otp} for box ${boxId} (Phone: ${cleanPhone})`);

    let messageText = `📦 *${title || 'New Package Delivery'}*\n\n`;
    if (description) {
        messageText += `${description}\n\n`;
    }
    messageText += `*Box ID:* ${boxId}\n*Your Unlock OTP:* ${otp}\n\n`;
    messageText += `_Instructions: Please enter the 4-digit OTP on the physical keypad of the delivery box to unlock it. This OTP will expire in 5 minutes._`;

    try {
        await sock.sendMessage(chatId, { text: messageText });
        res.json({ status: "success", message: "OTP Sent to WhatsApp successfully!" }); 
    } catch (error) {
        console.error("WhatsApp Error:", error);
        cache.del(otp); // rollback cache if message fails
        res.status(500).json({ status: "error", message: "Error sending WhatsApp message." });
    }
});

app.post('/api/verify-otp', (req, res) => {
    const { enteredCode, boxId } = req.body;

    const data = cache.get(enteredCode);

    if (data && data.boxId === boxId) {
        cache.del(enteredCode); 
        console.log(`[SUCCESS] Box ${boxId} unlocked with OTP ${enteredCode}`);
        res.status(200).json({ status: "success", command: "UNLOCK" });
    } else {
        console.log(`[FAIL] Invalid OTP attempt for Box ${boxId}: ${enteredCode}`);
        res.status(401).json({ status: "fail", command: "KEEP_LOCKED" });
    }
});

app.get('/api/active-otps', (req, res) => {
    const keys = cache.keys();
    const active = keys.map(key => {
        const data = cache.get(key);
        // Calculate remaining TTL
        const ttl = cache.getTtl(key);
        const remainingSeconds = ttl ? Math.max(0, Math.floor((ttl - Date.now()) / 1000)) : 0;
        
        // Mask phone number for privacy (e.g., 9198******10)
        const maskedPhone = data.phoneNumber.substring(0, 4) + '******' + data.phoneNumber.substring(10);
        
        return {
            boxId: data.boxId,
            phoneNumber: maskedPhone,
            title: data.title,
            remainingSeconds
        };
    });
    res.json({ status: "success", data: active });
});

const PORT = process.env.PORT || 3001;
const server = app.listen(PORT, () => console.log(`Smart Bin Server running on port ${PORT}`));

// --- GRACEFUL SHUTDOWN (PM2) ---
process.on('SIGINT', async () => {
    console.log('SIGINT signal received: gracefully shutting down...');
    
    // Close cache
    cache.flushAll();
    cache.close();
    
    // Close WhatsApp connection
    if (sock) {
        sock.end(undefined);
    }
    
    // Close HTTP Server
    server.close(() => {
        console.log('HTTP server closed. Exiting process.');
        process.exit(0);
    });
});