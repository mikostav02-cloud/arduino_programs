#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ElegantOTA.h>

// --- Піни основного RGB (м. Київ або загальний стан) ---
const int PIN_RED = 5;
const int PIN_GREEN = 6;
const int PIN_BLUE = 7;

// --- ДОДАТКОВО: Піни для Вінницької області ---
const int PIN_V_RED = 4;   // Червоний світлодіод Вінниці
const int PIN_V_GREEN = 10; // Зелений світлодіод Вінниці

// --- ДОДАЄМО: Піни для Волинської області ---
const int PIN_VOL_RED = 9;    
const int PIN_VOL_GREEN = 11;

// --- ДОДАЄМО: Піни для Луганської області ---
const int PIN_LUG_RED = 12;   
const int PIN_LUG_GREEN = 13;

// --- ДОДАЄМО: Піни для Донецької області ---
const int PIN_DON_RED = 14;   
const int PIN_DON_GREEN = 27;

// --- ДОДАЄМО: Піни для Запорізької області ---
const int PIN_ZP_RED = 15;   
const int PIN_ZP_GREEN = 16;

// --- ДОДАЄМО: Піни для Херсонської області ---
const int PIN_KHER_RED = 18;   
const int PIN_KHER_GREEN = 19;

// --- ДОДАЄМО: Піни для Одеської області ---
const int PIN_OD_RED = 21;   
const int PIN_OD_GREEN = 22;

const int PIN_BUZZER = 8;

// --- Настройки Звука ---
const int BUZZER_CHANNEL = 0;   
const int BUZZER_FREQ = 2000;   
const int BUZZER_RESOLUTION = 8; 
unsigned long alarmStartTime = 0;
const unsigned long ALARM_DURATION = 30000; // 30 сек тривоги
bool soundPlaying = false;

// --- Состояния RGB ---
enum DeviceState {
  STATE_CONFIG,       // Оранжевий (Точка доступу)
  STATE_CONNECTING,   // Мигаючий синій (Підключення)
  STATE_NORMAL,       // Зелений (Норма)
  STATE_ALARM         // Червоний + Звук (Тривога)
};

DeviceState currentState = STATE_CONFIG;

// --- Сетевые переменные ---
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;

String storedSsid = "";
String storedPassword = "";
String storedRegion = "Київська область";

unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 10000; // Перевірка тривоги раз на 10 сек
bool alarmActive = false;
bool alarmActiveVinnitsa = false;  // ДОДАТКОВО: статус тривоги для Вінниці
bool alarmActiveVolyn = false;     // ДОДАЄМО: Статус для Волині
bool alarmActiveLugansk = false;   // ДОДАЄМО: Статус для Луганщини
bool alarmActiveDonetsk = false;   // ДОДАЄМО: Статус для Донеччини
bool alarmActiveZaporizhzhia = false; // ДОДАЄМО: Статус для Запоріжжя
bool alarmActiveKherson = false;    // ДОДАЄМО: Статус для Херсона
bool alarmActiveOdesa = false;      // ДОДАЄМО: Статус для Одеси

// --- HTML Налаштувань (Captive Portal з кастомними кнопками) ---
const char CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Aerial Alarms - Налаштування</title>
    <style>
        :root {
            --bg-gradient: linear-gradient(135deg, #0f172a 0%, #1e1b4b 100%);
            --card-bg: rgba(30, 41, 59, 0.75);
            --accent: #f97316;
            --accent-hover: #ea580c;
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
            --border: rgba(255, 255, 255, 0.1);
            --error: #ef4444;
            --success: #22c55e;
        }
        * { 
            box-sizing: border-box; 
            margin: 0; 
            padding: 0; 
            font-family: 'Segoe UI', system-ui, sans-serif; 
            -webkit-tap-highlight-color: transparent; /* Видалення стандартного підсвічування тапа на мобільних */
        }
        body {
            background: var(--bg-gradient);
            color: var(--text-main);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .container {
            width: 100%;
            max-width: 440px;
            background: var(--card-bg);
            backdrop-filter: blur(16px);
            border: 1px solid var(--border);
            border-radius: 24px;
            padding: 32px;
            box-shadow: 0 20px 40px rgba(0,0,0,0.4);
            animation: fadeIn 0.8s cubic-bezier(0.16, 1, 0.3, 1);
            position: relative;
        }
        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(20px) scale(0.98); }
            to { opacity: 1; transform: translateY(0) scale(1); }
        }
        h2 { font-size: 1.8rem; margin-bottom: 8px; font-weight: 700; background: linear-gradient(to right, #f97316, #fb923c); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
        p { color: var(--text-muted); font-size: 0.95rem; margin-bottom: 24px; line-height: 1.5; }
        .form-group { margin-bottom: 20px; position: relative; }
        label { display: block; font-size: 0.85rem; font-weight: 600; text-transform: uppercase; letter-spacing: 0.05em; margin-bottom: 8px; color: var(--text-muted); }
        
        input {
            width: 100%;
            padding: 14px 45px 14px 16px;
            background: rgba(15, 23, 42, 0.6);
            border: 1px solid var(--border);
            border-radius: 12px;
            color: var(--text-main);
            font-size: 1rem;
            outline: none;
            transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
        }
        input:focus {
            border-color: var(--accent);
            box-shadow: 0 0 0 3px rgba(249, 115, 22, 0.2);
            transform: scale(1.01);
        }
        input.error {
            border-color: var(--error);
            box-shadow: 0 0 0 3px rgba(239, 68, 68, 0.2);
        }

        /* Кастомна кнопка ока */
        .toggle-password {
            position: absolute;
            right: 14px;
            top: 41px;
            cursor: pointer;
            color: var(--text-muted);
            width: 28px;
            height: 28px;
            display: flex;
            align-items: center;
            justify-content: center;
            border-radius: 50%;
            transition: all 0.2s ease;
        }
        .toggle-password:hover { color: var(--text-main); background: rgba(255, 255, 255, 0.05); }
        .toggle-password:active { transform: scale(0.85); }

        /* Тригери вибору (модалки) з кастомним натисканням */
        .select-trigger {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 14px 16px;
            background: rgba(15, 23, 42, 0.6);
            border: 1px solid var(--border);
            border-radius: 12px;
            cursor: pointer;
            transition: all 0.25s cubic-bezier(0.4, 0, 0.2, 1);
            font-size: 1rem;
            color: var(--text-main);
            user-select: none;
        }
        .select-trigger:hover { 
            border-color: rgba(255, 255, 255, 0.2); 
            background: rgba(15, 23, 42, 0.8);
        }
        .select-trigger:active { 
            transform: scale(0.98); 
            background: rgba(15, 23, 42, 0.9);
        }
        .select-trigger::after {
            content: '';
            width: 8px;
            height: 8px;
            border-right: 2px solid var(--text-muted);
            border-bottom: 2px solid var(--text-muted);
            transform: rotate(45deg);
            margin-left: 10px;
            flex-shrink: 0;
            transition: transform 0.2s ease;
        }

        /* Плавні модальні вікна */
        .modal-overlay {
            visibility: hidden;
            position: fixed;
            top: 0; left: 0; width: 100%; height: 100%;
            background: rgba(15, 23, 42, 0.8);
            backdrop-filter: blur(12px);
            z-index: 1000;
            display: flex;
            justify-content: center;
            align-items: flex-end;
            opacity: 0;
            transition: opacity 0.35s cubic-bezier(0.16, 1, 0.3, 1), visibility 0.35s;
        }
        @media (min-width: 640px) {
            .modal-overlay { align-items: center; }
        }
        .modal-overlay.active {
            visibility: visible;
            opacity: 1;
        }
        .modal-card {
            width: 100%;
            max-width: 440px;
            background: #0f172a;
            border: 1px solid var(--border);
            border-radius: 24px 24px 0 0;
            padding: 24px;
            max-height: 80vh;
            display: flex;
            flex-direction: column;
            box-shadow: 0 -10px 40px rgba(0,0,0,0.6);
            transform: translateY(100%);
            transition: transform 0.4s cubic-bezier(0.16, 1, 0.3, 1);
        }
        @media (min-width: 640px) {
            .modal-card {
                border-radius: 24px;
                transform: translateY(20px) scale(0.95);
                transition: transform 0.4s cubic-bezier(0.16, 1, 0.3, 1);
            }
            .modal-overlay.active .modal-card {
                transform: translateY(0) scale(1);
            }
        }
        .modal-overlay.active .modal-card {
            transform: translateY(0);
        }
        .modal-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 16px;
            padding-bottom: 12px;
            border-bottom: 1px solid var(--border);
        }
        .modal-title { font-size: 1.2rem; font-weight: 600; color: var(--text-main); }
        .modal-close {
            background: rgba(255,255,255,0.05);
            border: none;
            color: var(--text-muted);
            width: 32px; height: 32px;
            border-radius: 50%;
            cursor: pointer;
            font-size: 1.1rem;
            display: flex; align-items: center; justify-content: center;
            transition: all 0.2s ease;
        }
        .modal-close:hover { background: rgba(255,255,255,0.15); color: var(--text-main); }
        .modal-close:active { transform: scale(0.85); }

        .modal-body {
            overflow-y: auto;
            display: flex;
            flex-direction: column;
            gap: 8px;
            padding-right: 4px;
        }
        
        .modal-option {
            padding: 14px 16px;
            background: rgba(30, 41, 59, 0.4);
            border: 1px solid var(--border);
            border-radius: 12px;
            cursor: pointer;
            transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
            display: flex;
            align-items: center;
            justify-content: space-between;
            font-size: 1rem;
            user-select: none;
        }
        .modal-option:hover {
            background: rgba(249, 115, 22, 0.15);
            border-color: rgba(249, 115, 22, 0.4);
            color: var(--accent);
        }
        .modal-option:active {
            transform: scale(0.98);
            background: rgba(249, 115, 22, 0.25);
        }
        .modal-option.selected {
            background: rgba(249, 115, 22, 0.25);
            border-color: var(--accent);
            color: var(--accent);
            font-weight: 600;
        }

        .net-info { display: flex; align-items: center; gap: 10px; }
        .net-meta { display: flex; align-items: center; gap: 8px; font-size: 0.85rem; color: var(--text-muted); }
        .badge-open { background: rgba(34, 197, 94, 0.15); color: var(--success); padding: 3px 8px; border-radius: 6px; font-size: 0.75rem; font-weight: 600; }
        .wifi-icon { width: 18px; height: 18px; fill: currentColor; flex-shrink: 0; }

        #password-group {
            max-height: 100px;
            overflow: hidden;
            transition: max-height 0.35s ease, opacity 0.35s ease, margin-bottom 0.35s ease;
        }
        #password-group.hidden {
            max-height: 0;
            opacity: 0;
            margin-bottom: 0;
            pointer-events: none;
        }
        
        /* Кнопки з покращеним кастомним натисканням */
        .btn {
            width: 100%;
            padding: 14px 20px;
            background: var(--accent);
            color: white;
            border: none;
            border-radius: 12px;
            font-size: 1rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.25s cubic-bezier(0.4, 0, 0.2, 1);
            box-shadow: 0 4px 14px rgba(249, 115, 22, 0.35);
            margin-top: 10px;
            display: inline-flex;
            justify-content: center;
            align-items: center;
            text-align: center;
            text-decoration: none;
            user-select: none;
        }
        .btn:hover {
            background: var(--accent-hover);
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(249, 115, 22, 0.5);
        }
        .btn:active { 
            transform: translateY(1px) scale(0.97); 
            box-shadow: 0 2px 8px rgba(249, 115, 22, 0.4);
        }
        
        .refresh-btn {
            background: rgba(255, 255, 255, 0.05);
            border: 1px solid var(--border);
            color: var(--text-muted);
            margin-top: 8px;
            box-shadow: none;
        }
        .refresh-btn:hover {
            background: rgba(255, 255, 255, 0.1);
            color: var(--text-main);
            transform: translateY(-1px);
            box-shadow: none;
        }
        .refresh-btn:active {
            transform: translateY(1px) scale(0.97);
            background: rgba(255, 255, 255, 0.15);
        }

        .loader-overlay {
            display: none; position: absolute; top: 0; left: 0; width: 100%; height: 100%;
            background: rgba(15, 23, 42, 0.95); backdrop-filter: blur(10px);
            z-index: 999; justify-content: center; align-items: center; flex-direction: column;
            text-align: center; padding: 20px; border-radius: 24px;
        }
        .spinner {
            width: 50px; height: 50px; border: 4px solid rgba(249, 115, 22, 0.2);
            border-top: 4px solid var(--accent); border-radius: 50%;
            animation: spin 1s linear infinite; margin-bottom: 16px;
        }
        @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }
        
        .success-icon {
            width: 60px; height: 60px; background: rgba(34, 197, 94, 0.2);
            color: var(--success); border-radius: 50%; display: flex; align-items: center;
            justify-content: center; font-size: 30px; margin-bottom: 16px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="loader-overlay" id="loader">
            <div id="loader-content">
                <div class="spinner"></div>
                <p id="loader-text" style="color: #fff; font-weight: 600;">Збереження та підключення...</p>
            </div>
        </div>

        <h2>Налаштування пристрою</h2>
        <p>Виберіть вашу мережу Wi-Fi та регіон для отримання актуальних сповіщень.</p>
        
        <form id="config-form" onsubmit="return handleFormSubmit(event)">
            <input type="hidden" name="ssid" id="ssid-input" required>
            
            <div class="form-group">
                <label for="ssid">Мережа Wi-Fi (SSID)</label>
                <div class="select-trigger" id="wifi-trigger" onclick="openModal('wifi-modal')">Сканування мереж...</div>
                <button type="button" class="btn refresh-btn" onclick="scanNetworks()">Оновити список мереж</button>
            </div>
            
            <div class="form-group" id="password-group">
                <label for="password">Пароль Wi-Fi</label>
                <input type="password" name="password" id="password" placeholder="Введіть пароль">
                <span class="toggle-password" onclick="togglePassword()">
                    <svg id="eye-icon" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path><circle cx="12" cy="12" r="3"></circle></svg>
                </span>
            </div>

            <input type="hidden" name="region" id="region-input" value="🇺🇦 Вся Україна (Всі регіони)">

            <div class="form-group">
                <label for="region">Область / Місто</label>
                <div class="select-trigger" id="region-trigger" onclick="openModal('region-modal')">🇺🇦 Вся Україна (Всі регіони)</div>
            </div>
            
            <button type="submit" class="btn">Зберегти та підключити</button>
        </form>
    </div>

    <!-- МОДАЛЬНЕ ВІКНО ДЛЯ WIFI -->
    <div class="modal-overlay" id="wifi-modal">
        <div class="modal-card">
            <div class="modal-header">
                <span class="modal-title">Виберіть мережу Wi-Fi</span>
                <button class="modal-close" onclick="closeModal('wifi-modal')">&times;</button>
            </div>
            <div class="modal-body" id="wifi-modal-options">
                <div class="modal-option">Сканування мереж...</div>
            </div>
        </div>
    </div>

    <!-- МОДАЛЬНЕ ВІКНО ДЛЯ РЕГІОНІВ -->
    <div class="modal-overlay" id="region-modal">
        <div class="modal-card">
            <div class="modal-header">
                <span class="modal-title">Виберіть область / місто</span>
                <button class="modal-close" onclick="closeModal('region-modal')">&times;</button>
            </div>
            <div class="modal-body" id="region-modal-options"></div>
        </div>
    </div>

    <script>
        function openModal(modalId) {
            document.getElementById(modalId).classList.add('active');
        }
        function closeModal(modalId) {
            document.getElementById(modalId).classList.remove('active');
        }

        window.addEventListener('click', (e) => {
            if (e.target.classList.contains('modal-overlay')) {
                e.target.classList.remove('active');
            }
        });

        const regions = [
            "🇺🇦 Вся Україна (Всі регіони)",
            "Київська область", "м. Київ", "Вінницька область", "Волинська область", 
            "Дніпропетровська область", "Донецька область", "Житомирська область", 
            "Закарпатська область", "Запорізька область", "Івано-Франківська область", 
            "Кіровоградська область", "Луганська область", "Львівська область", 
            "Миколаївська область", "Одеська область", "Полтавська область", 
            "Рівненська область", "Сумська область", "Тернопільська область", 
            "Харківська область", "Херсонська область", "Хмельницька область", 
            "Черкаська область", "Чернівецька область", "Чернігівська область"
        ];

        const regionModalOptions = document.getElementById('region-modal-options');
        regions.forEach((reg, idx) => {
            let opt = document.createElement('div');
            opt.className = 'modal-option' + (idx === 0 ? ' selected' : '');
            opt.textContent = reg;
            opt.addEventListener('click', () => {
                regionModalOptions.querySelectorAll('.modal-option').forEach(o => o.classList.remove('selected'));
                opt.classList.add('selected');
                document.getElementById('region-trigger').textContent = reg;
                document.getElementById('region-input').value = reg;
                closeModal('region-modal');
            });
            regionModalOptions.appendChild(opt);
        });

        function getWifiIconSvg(rssi) {
            return `<svg class="wifi-icon" viewBox="0 0 24 24"><path d="M12 3c-4.97 0-9.5 2.01-12.78 5.25l1.42 1.42C3.41 6.84 7.42 5 12 5s8.59 1.84 11.36 4.67l1.42-1.42C21.5 5.01 16.97 3 12 3zm0 6c-3.14 0-6 1.28-8.07 3.35l1.42 1.42C7.03 12.3 9.4 11.5 12 11.5s4.97.8 6.65 2.27l1.42-1.42C18 10.28 15.14 9 12 9zm0 6c-1.31 0-2.5.53-3.35 1.38L12 19.8l3.35-3.42C14.5 15.53 13.31 15 12 15z"/></svg>`;
        }

        // Перевірка на відкриту мережу (враховує encryption, open та auth mode)
        function isNetworkOpen(net) {
            if (net.open === true || net.open === "true") return true;
            if (net.encryption === 0 || net.encryption === "0" || net.encryption === "WIFI_AUTH_OPEN") return true;
            if (net.authmode === 0 || net.authmode === "OPEN") return true;
            return false;
        }

        function scanNetworks(retries = 2) {
            let trigger = document.getElementById('wifi-trigger');
            let modalOptions = document.getElementById('wifi-modal-options');
            let ssidInput = document.getElementById('ssid-input');
            
            trigger.textContent = "Сканування мереж...";
            
            fetch('/scan')
                .then(response => response.json())
                .then(data => {
                    modalOptions.innerHTML = '';
                    if (!data || data.length === 0) {
                        if (retries > 0) {
                            setTimeout(() => scanNetworks(retries - 1), 1500);
                            return;
                        }
                        trigger.textContent = "Мереж не знайдено";
                        ssidInput.value = "";
                        return;
                    }

                    data.forEach((net, index) => {
                        let opt = document.createElement('div');
                        opt.className = 'modal-option';
                        if (index === 0) opt.classList.add('selected');
                        
                        let isOpen = isNetworkOpen(net);
                        
                        let leftPart = `<div class="net-info"><span>${net.ssid}</span></div>`;
                        let rightPart = `<div class="net-meta">` + 
                                         (isOpen ? `<span class="badge-open">Відкрита</span>` : ``) + 
                                        `</div>`;
                        
                        opt.innerHTML = leftPart + rightPart;
                        
                        opt.addEventListener('click', () => {
                            modalOptions.querySelectorAll('.modal-option').forEach(o => o.classList.remove('selected'));
                            opt.classList.add('selected');
                            
                            trigger.innerHTML = `<div class="net-info" style="width:100%; justify-content:space-between;">${leftPart} ${rightPart}</div>`;
                            ssidInput.value = net.ssid;
                            
                            let passGroup = document.getElementById('password-group');
                            let passInput = document.getElementById('password');
                            passInput.classList.remove('error');

                            if (isOpen) {
                                passGroup.classList.add('hidden');
                                passInput.value = "";
                                passInput.removeAttribute('required');
                            } else {
                                passGroup.classList.remove('hidden');
                                passInput.setAttribute('required', 'true');
                            }

                            closeModal('wifi-modal');
                        });
                        
                        modalOptions.appendChild(opt);
                    });

                    let firstNet = data[0];
                    let isFirstOpen = isNetworkOpen(firstNet);
                    
                    let firstLeft = `<div class="net-info">${getWifiIconSvg(firstNet.rssi)} <span>${firstNet.ssid}</span></div>`;
                    let firstRight = `<div class="net-meta">` + (isFirstOpen ? `<span class="badge-open">Відкрита</span>` : ``) + `<span>${firstNet.rssi} dBm</span></div>`;
                    
                    trigger.innerHTML = `<div class="net-info" style="width:100%; justify-content:space-between;">${firstLeft} ${firstRight}</div>`;
                    ssidInput.value = firstNet.ssid;
                    
                    let passGroup = document.getElementById('password-group');
                    let passInput = document.getElementById('password');
                    
                    if (isFirstOpen) {
                        passGroup.classList.add('hidden');
                        passInput.value = "";
                        passInput.removeAttribute('required');
                    } else {
                        passGroup.classList.remove('hidden');
                        passInput.setAttribute('required', 'true');
                    }
                })
                .catch(() => {
                    if (retries > 0) {
                        setTimeout(() => scanNetworks(retries - 1), 1500);
                    } else {
                        trigger.textContent = "Помилка сканування";
                    }
                });
        }

        function togglePassword() {
            let passInput = document.getElementById('password');
            let eyeIcon = document.getElementById('eye-icon');
            if (passInput.type === 'password') {
                passInput.type = 'text';
                eyeIcon.innerHTML = '<path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19m-6.72-1.07a3 3 0 1 1-4.24-4.24"></path><line x1="1" y1="1" x2="23" y2="23"></line>';
            } else {
                passInput.type = 'password';
                eyeIcon.innerHTML = '<path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path><circle cx="12" cy="12" r="3"></circle>';
            }
        }

        function handleFormSubmit(event) {
            event.preventDefault();
            
            let passInput = document.getElementById('password');
            let passGroup = document.getElementById('password-group');
            
            if (!passGroup.classList.contains('hidden') && !passInput.value.trim()) {
                passInput.classList.add('error');
                passInput.focus();
                return false;
            }

            let loader = document.getElementById('loader');
            let loaderContent = document.getElementById('loader-content');
            loader.style.display = 'flex';

            let formData = new URLSearchParams(new FormData(document.getElementById('config-form')));

            fetch('/save', {
                method: 'POST',
                body: formData
            })
            .then(response => {
                if (response.ok) {
                    loaderContent.innerHTML = `
                        <div class="success-icon">✓</div>
                        <p style="color: #fff; font-weight: 600; font-size: 1.1rem; margin-bottom: 8px;">Підключення успішне!</p>
                        <p style="font-size: 0.9rem;">Пристрій підключається до мережі та перезавантажується...</p>
                    `;
                } else {
                    throw new Error();
                }
            })
            .catch(() => {
                loaderContent.innerHTML = `
                    <div class="success-icon">✓</div>
                    <p style="color: #fff; font-weight: 600; font-size: 1.1rem; margin-bottom: 8px;">Збережено!</p>
                    <p style="font-size: 0.9rem;">Налаштування прийнято. Пристрій перезавантажується.</p>
                `;
            });

            return false;
        }

        window.onload = () => scanNetworks();
    </script>
</body>
</html>
)rawliteral";

// --- HTML Панелі керування (Dashboard з кастомними кнопками та живим оновленням) ---
const char DASHBOARD_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Aerial Alarms - Керування</title>
    <style>
        :root {
            --bg-gradient: linear-gradient(135deg, #0f172a 0%, #1e1b4b 100%);
            --card-bg: rgba(30, 41, 59, 0.75);
            --success: #22c55e;
            --danger: #ef4444;
            --danger-hover: #dc2626;
            --info: #0284c7;
            --info-hover: #0369a1;
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
            --border: rgba(255, 255, 255, 0.1);
        }
        * { 
            box-sizing: border-box; 
            margin: 0; 
            padding: 0; 
            font-family: 'Segoe UI', system-ui, sans-serif; 
            -webkit-tap-highlight-color: transparent; 
        }
        body {
            background: var(--bg-gradient);
            color: var(--text-main);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .container {
            width: 100%;
            max-width: 440px;
            background: var(--card-bg);
            backdrop-filter: blur(16px);
            border: 1px solid var(--border);
            border-radius: 24px;
            padding: 32px;
            box-shadow: 0 20px 40px rgba(0,0,0,0.4);
            text-align: center;
            animation: fadeIn 0.8s cubic-bezier(0.16, 1, 0.3, 1);
        }
        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(20px) scale(0.98); }
            to { opacity: 1; transform: translateY(0) scale(1); }
        }
        h2 { 
            font-size: 1.8rem; 
            margin-bottom: 8px; 
            font-weight: 700; 
            background: linear-gradient(to right, #38bdf8, #818cf8); 
            -webkit-background-clip: text; 
            -webkit-text-fill-color: transparent; 
        }
        p { color: var(--text-muted); font-size: 0.95rem; margin-bottom: 24px; line-height: 1.5; }
        
        .status-badge {
            display: inline-flex; 
            align-items: center; 
            gap: 10px;
            padding: 12px 24px; 
            border-radius: 50px; 
            font-weight: 600; 
            font-size: 0.95rem;
            margin-bottom: 28px; 
            background: rgba(34, 197, 94, 0.1); 
            border: 1px solid rgba(34, 197, 94, 0.25);
            color: #86efac;
            transition: all 0.4s cubic-bezier(0.4, 0, 0.2, 1);
        }
        .dot { 
            width: 10px; 
            height: 10px; 
            border-radius: 50%; 
            background: var(--success); 
            box-shadow: 0 0 10px var(--success); 
            animation: pulseDot 2s infinite;
        }
        @keyframes pulseDot {
            0% { transform: scale(0.95); box-shadow: 0 0 0 0 rgba(34, 197, 94, 0.7); }
            70% { transform: scale(1); box-shadow: 0 0 0 8px rgba(34, 197, 94, 0); }
            100% { transform: scale(0.95); box-shadow: 0 0 0 0 rgba(34, 197, 94, 0); }
        }

        .status-badge.alarm { 
            background: rgba(239, 68, 68, 0.15); 
            border-color: rgba(239, 68, 68, 0.4); 
            color: #fca5a5; 
        }
        .status-badge.alarm .dot { 
            background: var(--danger); 
            box-shadow: 0 0 12px var(--danger); 
            animation: pulseAlarmDot 1s infinite;
        }
        @keyframes pulseAlarmDot {
            0% { transform: scale(0.95); box-shadow: 0 0 0 0 rgba(239, 68, 68, 0.8); }
            70% { transform: scale(1); box-shadow: 0 0 0 10px rgba(239, 68, 68, 0); }
            100% { transform: scale(0.95); box-shadow: 0 0 0 0 rgba(239, 68, 68, 0); }
        }

        /* КНОПКИ З ПОКРАЩЕНИМ НАТИСКАННЯМ */
        .btn {
            width: 100%;
            padding: 14px 20px;
            background: #334155;
            color: white;
            border: none;
            border-radius: 12px;
            font-size: 1rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.25s cubic-bezier(0.4, 0, 0.2, 1);
            text-decoration: none;
            display: inline-flex;
            justify-content: center;
            align-items: center;
            margin-bottom: 12px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.2);
            user-select: none;
        }
        .btn:hover {
            background: #475569;
            transform: translateY(-2px);
            box-shadow: 0 6px 16px rgba(0,0,0,0.3);
        }
        .btn:active {
            transform: translateY(1px) scale(0.97);
            box-shadow: 0 2px 6px rgba(0,0,0,0.2);
        }

        .btn-ota {
            background: var(--info);
            box-shadow: 0 4px 14px rgba(2, 132, 199, 0.35);
        }
        .btn-ota:hover {
            background: var(--info-hover);
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(2, 132, 199, 0.5);
        }
        .btn-ota:active {
            transform: translateY(1px) scale(0.97);
            box-shadow: 0 2px 8px rgba(2, 132, 199, 0.3);
        }

        .btn-danger {
            background: var(--danger);
            box-shadow: 0 4px 14px rgba(239, 68, 68, 0.35);
        }
        .btn-danger:hover {
            background: var(--danger-hover);
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(239, 68, 68, 0.5);
        }
        .btn-danger:active {
            transform: translateY(1px) scale(0.97);
            box-shadow: 0 2px 8px rgba(239, 68, 68, 0.3);
        }
    </style>
</head>
<body>
    <div class="container">
        <h2>Статус пристрою</h2>
        <p id="region-title">Регіон моніторингу</p>
        
        <div class="status-badge" id="statusBadge">
            <div class="dot"></div>
            <span id="statusText">Оновлення статусу...</span>
        </div>

        <a href="/update" class="btn btn-ota">Оновлення прошивки (OTA)</a>
        
        <form action="/reset" method="POST">
            <button type="submit" class="btn btn-danger" onclick="return confirm('Ви впевнені, що хочете скинути налаштування Wi-Fi?');">Скинути налаштування</button>
        </form>
    </div>

    <script>
        function updateStatus() {
            fetch('/status')
                .then(res => res.json())
                .then(data => {
                    document.getElementById('region-title').textContent = data.region;
                    let badge = document.getElementById('statusBadge');
                    let text = document.getElementById('statusText');
                    
                    if (data.alarm) {
                        badge.classList.add('alarm');
                        text.textContent = 'УВАГА! Повітряна тривога!';
                    } else {
                        badge.classList.remove('alarm');
                        text.textContent = 'Все спокійно (Немає тривоги)';
                    }
                })
                .catch(err => console.log(err));
        }
        setInterval(updateStatus, 3000);
        updateStatus();
    </script>
</body>
</html>
)rawliteral";

// --- Функції керування обладнанням ---
void setRGB(bool r, bool g, bool b) {
  digitalWrite(PIN_RED, r ? HIGH : LOW);
  digitalWrite(PIN_GREEN, g ? HIGH : LOW);
  digitalWrite(PIN_BLUE, b ? HIGH : LOW);
}

// Керування світлодіодом Вінницької області
void setVinnitsaLED(bool alert) {
  if (alert) {
    digitalWrite(PIN_V_RED, HIGH);   // Горітиме червоний (є тривога)
    digitalWrite(PIN_V_GREEN, LOW);  // Зелений вимкнений
  } else {
    digitalWrite(PIN_V_RED, LOW);    // Червоний вимкнений
    digitalWrite(PIN_V_GREEN, HIGH); // Горітиме зелений (все спокійно)
  }
}

// ДОДАЄМО: Керування світлодіодом Волинської області
void setVolynLED(bool alert) {
  if (alert) {
    digitalWrite(PIN_VOL_RED, HIGH);   // Червоний горить (є тривога)
    digitalWrite(PIN_VOL_GREEN, LOW);  // Зелений вимкнений
  } else {
    digitalWrite(PIN_VOL_RED, LOW);    // Червоний вимкнений
    digitalWrite(PIN_VOL_GREEN, HIGH); // Зелений горить (все спокійно)
  }
}

// ДОДАЄМО: Керування світлодіодом Луганської області
void setLuganskLED(bool alert) {
  if (alert) {
    digitalWrite(PIN_LUG_RED, HIGH);   
    digitalWrite(PIN_LUG_GREEN, LOW);  
  } else {
    digitalWrite(PIN_LUG_RED, LOW);    
    digitalWrite(PIN_LUG_GREEN, HIGH); 
  }
}

// ДОДАЄМО: Керування світлодіодом Донецької області
void setDonetskLED(bool alert) {
  if (alert) {
    digitalWrite(PIN_DON_RED, HIGH);   
    digitalWrite(PIN_DON_GREEN, LOW);  
  } else {
    digitalWrite(PIN_DON_RED, LOW);    
    digitalWrite(PIN_DON_GREEN, HIGH); 
  }
}

// ДОДАЄМО: Керування світлодіодом Запорізької області
void setZaporizhzhiaLED(bool alert) {
  if (alert) {
    digitalWrite(PIN_ZP_RED, HIGH);   
    digitalWrite(PIN_ZP_GREEN, LOW);  
  } else {
    digitalWrite(PIN_ZP_RED, LOW);    
    digitalWrite(PIN_ZP_GREEN, HIGH); 
  }
}

// ДОДАЄМО: Керування світлодіодом Херсонської області
void setKhersonLED(bool alert) {
  if (alert) {
    digitalWrite(PIN_KHER_RED, HIGH);   
    digitalWrite(PIN_KHER_GREEN, LOW);  
  } else {
    digitalWrite(PIN_KHER_RED, LOW);    
    digitalWrite(PIN_KHER_GREEN, HIGH); 
  }
}

// ДОДАЄМО: Керування світлодіодом Одеської області
void setOdesaLED(bool alert) {
  if (alert) {
    digitalWrite(PIN_OD_RED, HIGH);   
    digitalWrite(PIN_OD_GREEN, LOW);  
  } else {
    digitalWrite(PIN_OD_RED, LOW);    
    digitalWrite(PIN_OD_GREEN, HIGH); 
  }
}

void setBuzzerState(bool state) {
  if (state) {
    if (!soundPlaying) {
      soundPlaying = true;
      alarmStartTime = millis();
    }
    if (ALARM_DURATION > 0 && (millis() - alarmStartTime > ALARM_DURATION)) {
      ledcWrite(BUZZER_CHANNEL, 0); 
      return;
    }
    int freq = 1500 + 500 * sin(millis() / 200.0); 
    ledcSetup(BUZZER_CHANNEL, freq, BUZZER_RESOLUTION);
    ledcWrite(BUZZER_CHANNEL, 128); 
  } else {
    soundPlaying = false;
    alarmStartTime = 0;
    ledcWrite(BUZZER_CHANNEL, 0);
  }
}

void updateOutputs() {
  // Завжди оновлюємо незалежні світлодіоди областей
  setVinnitsaLED(alarmActiveVinnitsa);
  setVolynLED(alarmActiveVolyn); // ДОДАЄМО виклик для Волині
  setLuganskLED(alarmActiveLugansk); // ДОДАЄМО виклик для Луганщини
  setDonetskLED(alarmActiveDonetsk); // ДОДАЄМО виклик для Донеччини
  setZaporizhzhiaLED(alarmActiveZaporizhzhia); // ДОДАЄМО виклик для Запоріжжя
setKhersonLED(alarmActiveKherson); // ДОДАЄМО виклик для Херсона
setOdesaLED(alarmActiveOdesa); // ДОДАЄМО виклик для Одеси

  // Логіка для основного RGB (м. Київ або режим "Вся Україна")
  switch (currentState) {
    case STATE_CONFIG:
      setRGB(true, true, false); 
      setBuzzerState(false);
      break;
    case STATE_CONNECTING:
      setRGB(false, false, true); 
      setBuzzerState(false);
      break;
    case STATE_NORMAL:
      setRGB(false, true, false); // Світиться зеленим, коли немає тривог
      setBuzzerState(false);
      break;
    case STATE_ALARM:
      setRGB(true, false, false); // Світиться червоним при тривозі
      setBuzzerState(true);
      break;
  }
}

// --- Перевірка тривоги через API ---
void checkAlarms() {
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.println("[API] Перевірка статусу тривоги для: [" + storedRegion + "]");
  HTTPClient http;
  http.begin("http://ubilling.net.ua/aerialalerts/");
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    String payload = http.getString();
    DynamicJsonDocument doc(98304);
    DeserializationError error = deserializeJson(doc, payload);

if (!error) {
JsonObject states = doc["states"];
      
      // Перевірка Вінниці
      if (states.containsKey("Вінницька область")) {
        alarmActiveVinnitsa = states["Вінницька область"]["alertnow"];
      }

      // ДОДАЄМО: Перевірка Волині
      if (states.containsKey("Волинська область")) {
        alarmActiveVolyn = states["Волинська область"]["alertnow"];
      }

      // ДОДАЄМО: Перевірка Луганщини
      if (states.containsKey("Луганська область")) {
        alarmActiveLugansk = states["Луганська область"]["alertnow"];
      }

      // ДОДАЄМО: Перевірка Донеччини
      if (states.containsKey("Донецька область")) {
        alarmActiveDonetsk = states["Донецька область"]["alertnow"];
      }
      // ДОДАЄМО: Перевірка Запоріжжя
  if (states.containsKey("Запорізька область")) {
    alarmActiveZaporizhzhia = states["Запорізька область"]["alertnow"];
  }

      // ДОДАЄМО: Перевірка Херсонщини
  if (states.containsKey("Херсонська область")) {
    alarmActiveKherson = states["Херсонська область"]["alertnow"];
  }

// ДОДАЄМО: Перевірка Одещини
  if (states.containsKey("Одеська область")) {
    alarmActiveOdesa = states["Одеська область"]["alertnow"];
  }

      // Обробка основного регіону (зокрема "Вся Україна")
      if (storedRegion == "🇺🇦 Вся Україна (Всі регіони)") {
        bool anyAlert = false;
        for (JsonPair kv : states) {
          if (kv.value()["alertnow"].as<bool>()) {
            anyAlert = true;
            break;
          }
        }
        alarmActive = anyAlert;
      } 
      else {
        if (states.containsKey(storedRegion)) {
          alarmActive = states[storedRegion]["alertnow"];
        }
      }

      // Примусово переводимо стан у тривогу або норму
      currentState = alarmActive ? STATE_ALARM : STATE_NORMAL;
      updateOutputs();
    }
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n--- Старт ESP32 Aerial Alarms ---");
/*
preferences.begin("config", false);
preferences.clear(); // Очищує всі збережені дані Wi-Fi та регіон
preferences.end();
*/
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);

  // ДОДАТКОВО: Ініціалізація пінів світлодіода Вінниці
  pinMode(PIN_V_RED, OUTPUT);
  pinMode(PIN_V_GREEN, OUTPUT);

  // ДОДАЄМО: Ініціалізація пінів Волині
  pinMode(PIN_VOL_RED, OUTPUT);
  pinMode(PIN_VOL_GREEN, OUTPUT);

  // ДОДАЄМО: Ініціалізація пінів Луганщини
  pinMode(PIN_LUG_RED, OUTPUT);
  pinMode(PIN_LUG_GREEN, OUTPUT);

  // ДОДАЄМО: Ініціалізація пінів Донеччини
  pinMode(PIN_DON_RED, OUTPUT);
  pinMode(PIN_DON_GREEN, OUTPUT);

// ДОДАЄМО: Ініціалізація пінів Запоріжжя
  pinMode(PIN_ZP_RED, OUTPUT);
  pinMode(PIN_ZP_GREEN, OUTPUT);

// ДОДАЄМО: Ініціалізація пінів Херсона
  pinMode(PIN_KHER_RED, OUTPUT);
  pinMode(PIN_KHER_GREEN, OUTPUT);

// ДОДАЄМО: Ініціалізація пінів Одеси
  pinMode(PIN_OD_RED, OUTPUT);
  pinMode(PIN_OD_GREEN, OUTPUT);
  
  ledcSetup(BUZZER_CHANNEL, BUZZER_FREQ, BUZZER_RESOLUTION);
  ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);
  ledcWrite(BUZZER_CHANNEL, 0);

  currentState = STATE_CONFIG;
  updateOutputs();

  preferences.begin("config", false);
  storedSsid = preferences.getString("ssid", "");
  storedPassword = preferences.getString("pass", "");
  storedRegion = preferences.getString("region", "🇺🇦 Вся Україна (Всі регіони)");
  preferences.end();

  if (storedSsid != "") {
    currentState = STATE_CONNECTING;
    updateOutputs();
    WiFi.begin(storedSsid.c_str(), storedPassword.c_str());

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
  }

  if (WiFi.status() == WL_CONNECTED) {
    currentState = STATE_NORMAL;
    updateOutputs();

    Serial.println("[WiFi] Успішно підключено! IP: " + WiFi.localIP().toString());

    if (MDNS.begin("esp32alarm")) {
      Serial.println("[mDNS] http://esp32alarm.local");
    }

    server.on("/", []() {
      server.send(200, "text/html", DASHBOARD_PAGE);
    });

    server.on("/status", []() {
      String json = "{\"alarm\":" + String(alarmActive ? "true" : "false") + 
                    ",\"region\":\"" + storedRegion + "\"}";
      server.send(200, "application/json", json);
    });

    server.on("/reset", HTTP_POST, []() {
      preferences.begin("config", false);
      preferences.clear();
      preferences.end();

      String html = "<html lang='uk'><head><meta charset='UTF-8'>";
      html += "<meta http-equiv='refresh' content='7;url=http://192.168.4.1'>";
      html += "<style>body{background:#0f172a;color:#fff;font-family:sans-serif;display:flex;justify-content:center;align-items:center;height:100vh;flex-direction:column;text-align:center;}";
      html += ".spinner{width:40px;height:40px;border:4px solid rgba(239,68,68,0.2);border-top:4px solid #ef4444;border-radius:50%;animation:spin 1s linear infinite;margin-bottom:20px;}";
      html += "@keyframes spin{0%{transform:rotate(0deg);}100%{transform:rotate(360deg);}}</style>";
      html += "</head><body><div class='spinner'></div>";
      html += "<h2>Налаштування скинуто!</h2><p>Пристрій повертається до заводських налаштувань.<br>Підключіться до мережі точки доступу <b>ESP32-Alarm-Config</b>.</p>";
      html += "</body></html>";

      server.send(200, "text/html", html);
      delay(2000);
      ESP.restart();
    });

    ElegantOTA.begin(&server);
    server.begin();
    Serial.println("[HTTP] Веб-сервер та ElegantOTA запущені.");

  } else {
    currentState = STATE_CONFIG;
    updateOutputs();

    WiFi.softAP("ESP32-Alarm-Config");
    dnsServer.start(53, "*", WiFi.softAPIP());

    server.on("/", []() {
      server.send(200, "text/html", CONFIG_PAGE);
    });

server.on("/scan", []() {
      int n = WiFi.scanNetworks();
      String json = "[";
      for (int i = 0; i < n; ++i) {
        if (i) json += ",";
        
        // Отримуємо тип шифрування мережі
        wifi_auth_mode_t encryptionType = WiFi.encryptionType(i);
        bool isOpen = (encryptionType == WIFI_AUTH_OPEN);

        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\"";
        json += ",\"rssi\":" + String(WiFi.RSSI(i));
        json += ",\"open\":" + String(isOpen ? "true" : "false") + "}";
      }
      json += "]";
      server.send(200, "application/json", json);
    });

    server.on("/save", HTTP_POST, []() {
      storedSsid = server.arg("ssid");
      storedPassword = server.arg("password");
      storedRegion = server.arg("region");

      preferences.begin("config", false);
      preferences.putString("ssid", storedSsid);
      preferences.putString("pass", storedPassword);
      preferences.putString("region", storedRegion);
      preferences.end();

      String html = "<html lang='uk'><head><meta charset='UTF-8'>";
      html += "<meta http-equiv='refresh' content='7;url=http://esp32alarm.local'>";
      html += "<style>body{background:#0f172a;color:#fff;font-family:sans-serif;display:flex;justify-content:center;align-items:center;height:100vh;flex-direction:column;text-align:center;}";
      html += ".spinner{width:40px;height:40px;border:4px solid rgba(249,115,22,0.2);border-top:4px solid #f97316;border-radius:50%;animation:spin 1s linear infinite;margin-bottom:20px;}";
      html += "@keyframes spin{0%{transform:rotate(0deg);}100%{transform:rotate(360deg);}}</style>";
      html += "</head><body><div class='spinner'></div>";
      html += "<h2>Налаштування збережено!</h2><p>Пристрій перезавантажується і підключається до Wi-Fi.<br>Зачекайте, зараз сторінка оновиться автоматично...</p>";
      html += "</body></html>";

      server.send(200, "text/html", html);
      delay(2000);
      ESP.restart();
    });

    server.onNotFound([]() {
      server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
      server.send(302, "text/plain", "");
    });

    server.begin();
    Serial.println("[HTTP] Captive Portal запущено.");
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
    ElegantOTA.loop();
    
    if (currentState == STATE_ALARM) {
      setBuzzerState(true);
    }
    
    if (millis() - lastCheckTime > checkInterval) {
      lastCheckTime = millis();
      checkAlarms();
    }
  } else {
    dnsServer.processNextRequest();
    server.handleClient();
  }
}