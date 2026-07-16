document.addEventListener('DOMContentLoaded', () => {
    const form = document.getElementById('otp-form');
    const submitBtn = document.getElementById('generate-btn');
    const btnText = submitBtn.querySelector('span');
    const loader = document.getElementById('btn-loader');
    const statusMsg = document.getElementById('status-message');
    const otpsList = document.getElementById('otps-list');

    function showStatus(message, isError) {
        statusMsg.textContent = message;
        statusMsg.className = `status-msg ${isError ? 'error' : 'success'}`;
        statusMsg.classList.remove('hidden');
        setTimeout(() => statusMsg.classList.add('hidden'), 5000);
    }

    form.addEventListener('submit', async (e) => {
        e.preventDefault();
        
        // UI Loading State
        submitBtn.disabled = true;
        btnText.textContent = 'Generating...';
        loader.classList.remove('hidden');
        statusMsg.classList.add('hidden');

        const payload = {
            boxId: document.getElementById('boxId').value,
            phoneNumber: document.getElementById('phoneNumber').value,
            title: document.getElementById('title').value,
            description: document.getElementById('description').value,
            pin: document.getElementById('pin').value
        };

        try {
            const res = await fetch('/api/generate-otp', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const data = await res.json();

            if (!res.ok || data.status === 'error') {
                showStatus(data.message || 'An error occurred', true);
            } else {
                showStatus(data.message, false);
                form.reset();
                fetchActiveOTPs();
            }
        } catch (error) {
            showStatus('Failed to connect to the server.', true);
        } finally {
            submitBtn.disabled = false;
            btnText.textContent = 'Generate OTP';
            loader.classList.add('hidden');
        }
    });

    async function fetchActiveOTPs() {
        try {
            const res = await fetch('/api/active-otps');
            const { data } = await res.json();
            
            otpsList.innerHTML = '';
            if (data.length === 0) {
                otpsList.innerHTML = '<p style="color: var(--text-secondary); font-size: 0.85rem;">No active deliveries.</p>';
                return;
            }

            data.forEach(otp => {
                const card = document.createElement('div');
                card.className = 'otp-card';
                card.innerHTML = `
                    <div class="otp-info">
                        <span class="otp-box">${otp.boxId}</span>
                        <span class="otp-desc">${otp.title} (${otp.phoneNumber})</span>
                    </div>
                    <div class="otp-timer" data-ttl="${otp.remainingSeconds}">${formatTime(otp.remainingSeconds)}</div>
                `;
                otpsList.appendChild(card);
            });
        } catch (error) {
            console.error('Failed to fetch OTPs', error);
        }
    }

    function formatTime(seconds) {
        const m = Math.floor(seconds / 60).toString().padStart(2, '0');
        const s = (seconds % 60).toString().padStart(2, '0');
        return `${m}:${s}`;
    }

    // Update timers locally
    setInterval(() => {
        document.querySelectorAll('.otp-timer').forEach(el => {
            let ttl = parseInt(el.getAttribute('data-ttl'));
            if (ttl > 0) {
                ttl--;
                el.setAttribute('data-ttl', ttl);
                el.textContent = formatTime(ttl);
            } else {
                fetchActiveOTPs(); // Refresh list if something expired
            }
        });
    }, 1000);

    // Clear Logs functionality
    document.getElementById('clear-logs-btn').addEventListener('click', async () => {
        const boxId = document.getElementById('clear-box-id').value.trim();
        const pin = document.getElementById('pin').value.trim();
        const statusEl = document.getElementById('clear-status');
        
        if (!boxId) {
            statusEl.textContent = 'Please enter a Box ID';
            statusEl.className = 'status-msg error';
            statusEl.classList.remove('hidden');
            return;
        }
        
        if (!pin) {
            statusEl.textContent = 'Admin PIN required (enter in the main form above)';
            statusEl.className = 'status-msg error';
            statusEl.classList.remove('hidden');
            return;
        }
        
        try {
            const res = await fetch(`/api/logs/${boxId}`, {
                method: 'DELETE',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ pin })
            });
            const data = await res.json();
            
            if (data.status === 'success') {
                statusEl.textContent = data.message;
                statusEl.className = 'status-msg success';
            } else {
                statusEl.textContent = data.message || 'Failed to clear logs';
                statusEl.className = 'status-msg error';
            }
            statusEl.classList.remove('hidden');
            
            // Hide after 3 seconds
            setTimeout(() => {
                statusEl.classList.add('hidden');
            }, 3000);
            
        } catch (err) {
            statusEl.textContent = 'Network error while clearing logs';
            statusEl.className = 'status-msg error';
            statusEl.classList.remove('hidden');
        }
    });

    // Initial fetch
    fetchActiveOTPs();
});
