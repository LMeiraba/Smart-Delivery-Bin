import express from 'express';
import makeWASocket, { useMultiFileAuthState, DisconnectReason } from '@whiskeysockets/baileys';
import qrcode from 'qrcode-terminal';
import pino from 'pino';
import NodeCache from 'node-cache';
import rateLimit from 'express-rate-limit';
import path from 'path';
import { fileURLToPath } from 'url';
import helmet from 'helmet';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
app.use(helmet());
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
            console.log('Connection closed due to error. Reconnecting in 5 seconds:', shouldReconnect);
            if (shouldReconnect) {
                // 5-second backoff to prevent tight loop OOM/CPU spikes
                setTimeout(connectToWhatsApp, 5000); 
            }
        } else if (connection === 'open') {
            console.log('WhatsApp Bot is connected and ready!');
        }
    });
}

connectToWhatsApp();

// --- WHATSAPP MESSAGE POOLING ---
const messageQueue = new Map(); // Stores pending events per phone number

setInterval(async () => {
    if (!sock) return; // Wait until WhatsApp is fully connected
    
    if (messageQueue.size === 0) return;

    for (const [phone, events] of messageQueue.entries()) {
        if (events.length === 0) continue;
        
        const chatId = `${phone}@s.whatsapp.net`;
        // We'll just grab the boxId from the first event for the title
        const boxId = events[0].boxId; 
        
        let msg = `📊 *SMART BIN ACTIVITY REPORT*\n`;
        msg += `━━━━━━━━━━━━━━━━━━━━━━\n`;
        msg += `🏢 *Box ID:* ${boxId}\n`;
        msg += `📝 *Events:* ${events.length} new update(s)\n\n`;
        
        events.forEach(e => {
            // Format time for India timezone (IST)
            const timeString = new Date(e.timestamp).toLocaleTimeString('en-IN', { timeZone: 'Asia/Kolkata', hour: '2-digit', minute:'2-digit' });
            
            let icon = "🔹";
            if (e.event === "OPEN_SUCCESS") icon = "🟢";
            else if (e.event === "OPEN_FAIL") icon = "❌";
            else if (e.event === "PACKAGE_DEPOSITED") icon = "📦";
            else if (e.event === "LID_LEFT_OPEN") icon = "⚠️";
            else if (e.event === "LID_CLOSED") icon = "🔒";
            else if (e.event === "AUTO_RELOCKED") icon = "🤖";
            else if (e.event === "BIN_FULL") icon = "📈";
            else if (e.event === "TAMPER_ALERT") icon = "🚨";
            
            msg += `*[${timeString}]* ${icon} *${e.event.replace(/_/g, ' ')}*\n`;
            if (e.details) msg += `   ↳ _${e.details}_\n`;
        });
        
        msg += `\n━━━━━━━━━━━━━━━━━━━━━━`;
        
        try {
            await sock.sendMessage(chatId, { text: msg });
            console.log(`[WHATSAPP] Sent pooled report to ${phone} with ${events.length} events.`);
        } catch (error) {
            console.error(`[WHATSAPP] Failed to send pooled report to ${phone}:`, error);
        }
    }
    
    // Clear the queue after sending
    messageQueue.clear();
}, 60000); // Run exactly every 60 seconds

// --- API ENDPOINTS ---

// In-Memory Storage for Event Logs (max 500)
const eventLogs = [];

// Global API rate limiter (Max 5 OTP requests per IP every 5 minutes)
const apiLimiter = rateLimit({
    windowMs: 5 * 60 * 1000, // 5 minutes
    max: 5, // Limit each IP to 5 requests per `window` (here, per 5 minutes)
    message: { status: "error", message: "Too many OTP requests from this device. Please wait 5 minutes before trying again." }
});

const ADMIN_PIN = process.env.ADMIN_PIN || "123456";

app.post('/api/generate-otp', apiLimiter, async (req, res) => {
    const { phoneNumber, boxId, title, description, pin } = req.body;

    if (pin !== ADMIN_PIN) {
        return res.status(401).json({ status: "error", message: "Unauthorized: Invalid Admin PIN" });
    }

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

    let messageText = `📦 *SECURE DELIVERY AUTHORIZATION*\n`;
    messageText += `━━━━━━━━━━━━━━━━━━━━━━\n\n`;
    messageText += `📌 *Package:* ${title || 'New Delivery'}\n`;
    messageText += `🏢 *Box ID:* ${boxId}\n\n`;
    messageText += `🔑 *YOUR UNLOCK OTP:  [ ${otp} ]*\n\n`;
    
    if (description) {
        messageText += `📝 *Instructions:*\n_${description}_\n\n`;
    }
    
    messageText += `⏳ _This OTP is valid for exactly 5 minutes._\n`;
    messageText += `━━━━━━━━━━━━━━━━━━━━━━\n`;
    messageText += `_Please enter the 4-digit code on the physical keypad of the delivery box to unlock it._`;

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

// Rate limiter for Event Logging to prevent spam/WhatsApp bans
const logLimiter = rateLimit({
    windowMs: 1 * 60 * 1000, // 1 minute
    max: 10, // Max 10 logs per IP per minute
    message: { status: "error", message: "Too many log requests." }
});

app.post('/api/log', logLimiter, (req, res) => {
    const { boxId, ownerPhone, event, details } = req.body;
    
    if (!boxId || !event) {
        return res.status(400).json({ status: "error", message: "boxId and event are required" });
    }

    const logEntry = {
        id: Date.now().toString(),
        timestamp: new Date().toISOString(),
        boxId,
        ownerPhone: ownerPhone || "",
        event,
        details: details || ""
    };

    eventLogs.unshift(logEntry); // Add newest to front
    if (eventLogs.length > 500) {
        eventLogs.pop(); // Keep array size bounded
    }

    console.log(`[LOG] Box ${boxId} Event: ${event}`);

    // Add to WhatsApp queue if ownerPhone is provided
    if (ownerPhone && ownerPhone.length >= 10) {
        let cleanPhone = ownerPhone.replace(/[\+\s\-]/g, '');
        if (cleanPhone.length === 10) cleanPhone = "91" + cleanPhone;
        
        if (!messageQueue.has(cleanPhone)) {
            messageQueue.set(cleanPhone, []);
        }
        messageQueue.get(cleanPhone).push(logEntry);
    }
    
    res.json({ status: "success", message: "Log stored and queued for WhatsApp" });
});

app.get('/api/logs', (req, res) => {
    // Return a list of all unique Box IDs that currently have logs
    const uniqueBoxes = [...new Set(eventLogs.map(log => log.boxId))];
    res.json({ status: "success", data: uniqueBoxes });
});

app.get('/api/logs/:boxId', (req, res) => {
    const { boxId } = req.params;
    const boxLogs = eventLogs.filter(log => log.boxId === boxId);
    res.json({ status: "success", data: boxLogs });
});

app.delete('/api/logs/:boxId', (req, res) => {
    const { boxId } = req.params;
    const { pin } = req.body;
    
    if (pin !== ADMIN_PIN) {
        return res.status(401).json({ status: "error", message: "Unauthorized: Invalid Admin PIN" });
    }
    
    // Remove all logs matching the boxId
    let removedCount = 0;
    for (let i = eventLogs.length - 1; i >= 0; i--) {
        if (eventLogs[i].boxId === boxId) {
            eventLogs.splice(i, 1);
            removedCount++;
        }
    }
    
    res.json({ status: "success", message: `Cleared ${removedCount} logs for ${boxId}` });
});

app.get('/api/active-otps', (req, res) => {
    const keys = cache.keys();
    const active = keys.map(key => {
        const data = cache.get(key);
        if (!data) return null;
        
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
    }).filter(Boolean);
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