/*

  ================================================================
  Copyright (c) 2026 Otto
  License: GPL-3.0-or-later (см. LICENSE)
  ================================================================

  WEB-ИНТЕРФЕЙС, НАСТРОЙКИ И ОБНОВЛЕНИЕ ПРОШИВКИ

*/

#include "WebInterface.h"
#include <WiFi.h>
#include <esp_timer.h>
#include <Preferences.h>
#include <Update.h>
#include <ESPAsyncWebServer.h>


#include "Config.h"
#include "Display.h"
#include "TimeManager.h"
#include "RelayControl.h"
#include "WifiManager.h"
#include "Thermistor.h"

WebInterfaceClass WebInterface;

static AsyncWebServer server(80);           // server — HTTP-сервер
static AsyncEventSource events("/events");  // events — Обновления состояния

static bool s_restartPending = false;
static unsigned long s_restartAt = 0;

extern TM1637Driver display;

// escapeJson — Экранирует текст JSON
static String escapeJson(const String &input) {
  String output;
  output.reserve(input.length() + 8);

  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];

    switch (c) {
      case '"':
        output += "\\\"";
        break;

      case '\\':
        output += "\\\\";
        break;

      case '\n':
        output += "\\n";
        break;

      case '\r':
        output += "\\r";
        break;

      case '\t':
        output += "\\t";
        break;

      default:
        if ((uint8_t)c < 0x20) {
          char buf[7];
          snprintf(
            buf,
            sizeof(buf),
            "\\u%04x",
            (unsigned)(uint8_t)c);
          output += buf;
        } else {
          output += c;
        }
        break;
    }
  }

  return output;
}

// buildStatusJson — Формирует состояние устройства
static String buildStatusJson() {
  RelaySchedule sch = RelayControl.getSchedule();
  char buf[320];
  snprintf(buf, sizeof(buf),
           "{\"temp\":%.1f,\"tempOffset\":%.1f,\"datetime\":\"%s\",\"relay\":%s,\"manual\":%s,"
           "\"scheduleStart\":%u,\"scheduleEnd\":%u}",
           g_currentTemperature,
           thermistor.getOffset(),
           TimeManager.getFormattedDateTime().c_str(),
           RelayControl.isRelayOn() ? "true" : "false",
           RelayControl.getMode() == RELAY_MODE_MANUAL ? "true" : "false",
           (unsigned)sch.startSeconds,
           (unsigned)sch.endSeconds);
  return String(buf);
}

// parseHMS — Преобразует время в секунды
static bool parseHMS(const String &s, uint32_t &result) {
  int h = 0;
  int m = 0;
  int sec = 0;
  char extra;

  int count = sscanf(
    s.c_str(),
    "%d:%d:%d%c",
    &h, &m, &sec, &extra);

  if (count != 3) {
    return false;
  }

  if (h < 0 || h > 23 || m < 0 || m > 59 || sec < 0 || sec > 59) {
    return false;
  }

  result = (uint32_t)h * 3600UL + (uint32_t)m * 60UL + (uint32_t)sec;

  return true;
}

// INDEX_HTML — Веб-страница устройства
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Реле по времени</title>
<style>
:root {
    --bg: #121212;
    --card: #1e1e1e;
    --accent: #4caf50;
    --accent-off: #555;
    --accent-blue: #2196f3;
    --text: #e0e0e0;
    --text-dim: #9a9a9a;
    --danger: #e53935;
    --border: #2c2c2c;
}

* {
    box-sizing: border-box;
}

body {
    margin: 0;
    background: var(--bg);
    color: var(--text);
    font-family: 'Segoe UI', Roboto, Arial, sans-serif;
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 16px;
}

.container {
    width: 100%;
    max-width: 480px;
}

h1 {
    font-size: 20px;
    text-align: center;
    margin-bottom: 16px;
    font-weight: 500;
}

.tabs {
    display: flex;
    margin-bottom: 16px;
    border-radius: 10px;
    overflow: hidden;
    background: var(--card);
}

.tab-btn {
    flex: 1;
    padding: 12px;
    background: none;
    border: none;
    color: var(--text-dim);
    font-size: 15px;
    cursor: pointer;
    transition: 0.2s;
}

.tab-btn.active {
    background: var(--accent);
    color: #fff;
}

.tab-content {
    display: none;
}

.tab-content.active {
    display: block;
}

.card {
    background: var(--card);
    border-radius: 12px;
    padding: 16px;
    margin-bottom: 14px;
    border: 1px solid var(--border);
}

.card h2 {
    font-size: 14px;
    color: var(--text-dim);
    margin: 0 0 12px 0;
    text-transform: uppercase;
    letter-spacing: 0.5px;
}

.temp-value {
    font-size: 42px;
    text-align: center;
    font-weight: 600;
    color: var(--accent);
}

.calib-row {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 14px;
    margin-top: 12px;
}

.calib-btn {
    width: 36px;
    height: 36px;
    border-radius: 50%;
    border: none;
    background: #333;
    color: var(--text);
    font-size: 20px;
    cursor: pointer;
    line-height: 1;
}

.calib-btn:active {
    background: var(--accent);
}

.calib-label {
    font-size: 13px;
    color: var(--text-dim);
    min-width: 120px;
    text-align: center;
}

.datetime {
    text-align: center;
    color: var(--accent-blue);
    font-size: 20px;
    font-weight: 600;
    margin-top: 4px;
}

.row {
    display: flex;
    gap: 10px;
    margin-bottom: 10px;
}

.row>div {
    flex: 1;
}

label {
    display: block;
    font-size: 13px;
    color: var(--text-dim);
    margin-bottom: 4px;
}

input[type=time],
input[type=text],
input[type=password],
input[type=number] {
    width: 100%;
    padding: 10px;
    border-radius: 8px;
    border: 1px solid var(--border);
    background: #2a2a2a;
    color: var(--text);
    font-size: 15px;
}

button.action {
    padding: 10px 12px;
    border: none;
    border-radius: 8px;
    font-size: 14px;
    cursor: pointer;
    margin-top: 6px;
    transition: 0.2s;
    font-weight: 500;
}

button.action:disabled {
    opacity: 0.5;
    cursor: not-allowed;
}

.btn-row {
    display: flex;
    gap: 8px;
}

.btn-row>button {
    flex: 1;
    min-width: 0;
}

.device-actions {
    display: flex;
    flex-direction: column;
    gap: 6px;
}

.full {
    width: 100%;
}

.btn-primary {
    background: var(--accent);
    color: #fff;
}

.btn-manual {
    background: var(--accent-blue);
    color: #fff;
    width: 100%;
}

.btn-danger {
    background: var(--danger);
    color: #fff;
}

.btn-secondary {
    background: #333;
    color: var(--text);
}

.status-line {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 10px;
}

.indicator {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    font-size: 14px;
}

.dot {
    width: 10px;
    height: 10px;
    border-radius: 50%;
    background: var(--accent-off);
}

.dot.active {
    background: var(--accent);
    box-shadow: 0 0 8px var(--accent);
}

.dot.manual {
    background: #ff9800;
    box-shadow: 0 0 8px #ff9800;
}

.hint {
    font-size: 12px;
    color: var(--text-dim);
    margin-top: 4px;
}

.modal-overlay {
    position: fixed;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background: rgba(0, 0, 0, 0.75);
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 1000;
}

.modal-box {
    background: var(--card);
    border-radius: 12px;
    padding: 16px;
    width: 90%;
    max-width: 420px;
    max-height: 70vh;
    overflow-y: auto;
    border: 1px solid var(--border);
}

.modal-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 12px;
    font-size: 16px;
}

.modal-close {
    background: none;
    border: none;
    color: var(--text);
    font-size: 20px;
    cursor: pointer;
}

.scan-item {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 10px;
    border-bottom: 1px solid var(--border);
    cursor: pointer;
    border-radius: 6px;
}

.scan-item:hover {
    background: #2a2a2a;
}

.scan-item .ssid-name {
    font-size: 14px;
}

.scan-meta {
    display: flex;
    align-items: center;
    gap: 6px;
    color: var(--text-dim);
    font-size: 13px;
    white-space: nowrap;
}

.scan-loading {
    text-align: center;
    color: var(--text-dim);
    padding: 20px;
}

.info-row {
    display: flex;
    justify-content: space-between;
    gap: 12px;
    padding: 5px 0;
    font-size: 13px;
    border-bottom: 1px solid var(--border);
}

.info-row:last-child {
    border-bottom: 0;
}

.info-row span:first-child {
    color: var(--text-dim);
}

.info-row span:last-child {
    text-align: right;
    overflow-wrap: anywhere;
}
</style>
</head>
<body>
    <div class="container">
        <h1>⏱ Управление реле</h1>

        <div class="tabs">
            <button class="tab-btn active" onclick="switchTab('main', this)">Главная</button>
            <button class="tab-btn" onclick="switchTab('params', this)">Параметры</button>
        </div>

        <div id="tab-main" class="tab-content active">
            <div class="card">
                <h2>Температура</h2>
                <div class="temp-value" id="temp">--.-°C</div>
                <div class="calib-row">
                    <button class="calib-btn" onclick="adjustTempOffset(-0.1)">−</button>
                    <span class="calib-label">Калибровка: <span id="tempOffset">0.0</span>°C</span>
                    <button class="calib-btn" onclick="adjustTempOffset(0.1)">+</button>
                </div>
            </div>

            <div class="card">
                <h2>Дата и время</h2>
                <div class="datetime" id="datetime">--</div>
            </div>

            <div class="card">
                <div class="status-line">
                    <span class="indicator"><span class="dot" id="relayDot"></span> Реле</span>
                    <span class="indicator"><span class="dot" id="manualDot"></span> Ручной режим</span>
                </div>
                <button class="action btn-manual" id="manualBtn" onclick="toggleManual()">...</button>
            </div>

            <div class="card">
                <h2>Расписание работы</h2>

                <div class="row">
                    <div>
                        <label>Включить в</label>
                        <input type="time" step="1" id="startTime" value="00:00:00" oninput="markDirty('start')">
                    </div>

                    <div>
                        <label>Выключить в</label>
                        <input type="time" step="1" id="endTime" value="00:00:00" oninput="markDirty('end')">
                    </div>
                </div>

                <button class="action btn-primary full" onclick="saveSchedule()">
                    Установить
                </button>

                <div class="hint">
                    Если время выключения меньше времени включения — интервал проходит через полночь.
                    Одинаковое время включения и выключения отключает расписание.
                </div>
            </div>
        </div>

        <div id="tab-params" class="tab-content">

            <div class="card">
                <h2>Информация об устройстве</h2>
                <div class="info-row"><span>Сеть (SSID)</span><span id="infoSsid">--</span></div>
                <div class="info-row"><span>IP адрес</span><span id="infoIp">--</span></div>
                <div class="info-row"><span>MAC ESP</span><span id="infoMac">--</span></div>
                <div class="info-row"><span>Уровень сигнала</span><span id="infoSignal">--</span></div>
                <div class="info-row"><span>Время работы ESP</span><span id="infoUptime">--</span></div>
                <div class="info-row"><span>Дата сборки</span><span id="infoBuild">--</span></div>
                <div class="info-row"><span>Версия прошивки</span><span id="infoVersion">--</span></div>
            </div>

            <div class="card">
                <h2>WiFi подключение</h2>
                <label>SSID</label>
                <div class="row">
                    <input type="text" id="wifiSsid" placeholder="Имя сети" style="flex:2;">
                    <button class="action btn-secondary" style="flex:1;margin-top:0;" onclick="openScanModal()">Сканировать</button>
                </div>
                <label style="margin-top:8px;">Пароль</label>
                <input type="text" id="wifiPass" placeholder="Пароль">
                <button class="action btn-primary full" onclick="saveWifi()">Сохранить и перезагрузить</button>

            </div>

            <div class="card">
                <h2>Яркость дисплея TM1637</h2>
                <div class="calib-row">
                    <button class="calib-btn" onclick="adjustBrightness(-1)">−</button>
                    <span class="calib-label">
                        Яркость: <span id="displayBrightness">3</span> / 3
                    </span>
                    <button class="calib-btn" onclick="adjustBrightness(1)">+</button>
                </div>
            </div>

            <div class="card">
                <h2>Настройки NTP</h2>
                <label>Сервер NTP</label>
                <input type="text" id="ntpServer" placeholder="ru.pool.ntp.org">
                <div class="row" style="margin-top:8px;">
                    <div><label>Часовой пояс (GMT)</label><input type="number" id="ntpGmt" value="6"></div>
                    <div><label>Период обновления (мин)</label><input type="number" id="ntpInterval" value="90"></div>
                </div>
                <div class="hint">Последняя синхронизация: <span id="lastSync">--</span></div>
                <button class="action btn-secondary full" onclick="syncNtp()">Синхронизировать NTP</button>
                <button class="action btn-primary full" onclick="saveNtp()">Применить с перезагрузкой</button>
            </div>

            <div class="card">
                <h2>Управление устройством</h2>
                <div class="device-actions">

                    <button class="action btn-secondary full" onclick="restartEsp()">
                        Перезагрузить ESP
                    </button>

                    <button class="action btn-primary full" id="wifiModeBtn" onclick="switchWifiMode()">
                        Определение режима...
                    </button>

                </div>
                <div class="hint" id="wifiModeHint"></div>
            </div>

            <div class="card">
                <h2>Обновление прошивки</h2>

                <input type="file" id="otaFile" accept=".bin" style="display:none" onchange="uploadOta()">

                <button class="action btn-primary full" id="otaBtn" onclick="document.getElementById('otaFile').click()">
                    Обновить прошивку
                </button>

                <div class="hint" id="otaStatus"></div>

            </div>

            <div class="card">
                <h2>Сброс настроек</h2>
                <button class="action btn-danger full" onclick="resetDevice()">Сброс настроек ESP</button>
                <div class="hint">Так же доступен сброс удержанием кнопки BOOT на плате более 8 секунд.</div>
            </div>
        </div>
    </div>

    <div id="scanModal" class="modal-overlay" style="display:none;">
        <div class="modal-box">
            <div class="modal-header">
                <span>Доступные сети</span>
                <button class="modal-close" onclick="closeScanModal()">✕</button>
            </div>
            <div id="scanList" class="scan-list">
                <div class="scan-loading">Сканирование...</div>
            </div>
        </div>
    </div>
<script>
let displayBrightness = 3;
let deviceInfoTimer = null;

// loadDeviceInfo — Загружает сведения об устройстве

function loadDeviceInfo() {
    fetch('/api/deviceinfo').then(r => r.json()).then(d => {
        const values = {
            infoSsid: d.ssid,
            infoIp: d.ip,
            infoMac: d.mac,
            infoSignal: d.signal,
            infoUptime: d.uptime,
            infoBuild: d.build,
            infoVersion: d.version
        };
        Object.entries(values).forEach(([id, value]) => {
            document.getElementById(id).textContent = value;
        });
    }).catch(console.error);
}

// loadBrightness — Загружает яркость

function loadBrightness() {
    fetch('/api/brightness')
        .then(r => r.json())
        .then(data => {
            displayBrightness = data.brightness;
            document.getElementById('displayBrightness').textContent = displayBrightness;
        });
}

// adjustBrightness — Изменяет яркость

function adjustBrightness(delta) {
    displayBrightness = Math.max(0, Math.min(3, displayBrightness + delta));

    document.getElementById('displayBrightness').textContent = displayBrightness;

    fetch('/api/brightness', {
        method: 'POST',
        body: new URLSearchParams({
            value: displayBrightness
        })
    }).catch(console.error);
}



let tempOffset = 0;
let dirty = {
    start: false,
    end: false
};

// switchTab — Переключает вкладки

function switchTab(name, btn) {
    document.querySelectorAll('.tab-content').forEach(e => e.classList.remove('active'));
    document.querySelectorAll('.tab-btn').forEach(e => e.classList.remove('active'));
    document.getElementById('tab-' + name).classList.add('active');
    btn.classList.add('active');
    if (deviceInfoTimer) {
        clearInterval(deviceInfoTimer);
        deviceInfoTimer = null;
    }

    if (name === 'params') {
        loadConfig();
        loadBrightness();
        loadDeviceInfo();
        loadWifiMode();

        deviceInfoTimer = setInterval(() => {
            loadDeviceInfo();
            loadWifiMode();
        }, 5000);
    }
}

// secToTime — Форматирует время

function secToTime(sec) {
    sec = parseInt(sec) || 0;
    const h = String(Math.floor(sec / 3600)).padStart(2, '0');
    const m = String(Math.floor((sec % 3600) / 60)).padStart(2, '0');
    const s = String(sec % 60).padStart(2, '0');
    return h + ':' + m + ':' + s;
}

// markDirty — Отмечает несохранённое время

function markDirty(field) {
    dirty[field] = true;
}

// updateStatus — Обновляет состояние страницы

function updateStatus(data) {
    currentRelayState = data.relay;
    document.getElementById('temp').textContent = data.temp.toFixed(1) + '°C';
    document.getElementById('datetime').textContent = data.datetime;
    document.getElementById('relayDot').classList.toggle('active', data.relay);
    document.getElementById('manualDot').classList.toggle('manual', data.manual);

    document.getElementById('manualBtn').textContent = data.relay ? 'Выключить' : 'Включить';

    tempOffset = data.tempOffset;
    document.getElementById('tempOffset').textContent = tempOffset.toFixed(1);

    if (!dirty.start) {
        document.getElementById('startTime').value = secToTime(data.scheduleStart);
    }
    if (!dirty.end) {
        document.getElementById('endTime').value = secToTime(data.scheduleEnd);
    }
}

let relayRequestPending = false;
let currentRelayState = false;

// toggleManual — Переключает реле

function toggleManual() {
    if (relayRequestPending) return;

    relayRequestPending = true;

    fetch('/api/manual', {
            method: 'POST'
        })
        .then(r => {
            if (!r.ok) throw new Error('HTTP ' + r.status);
        })
        .catch(console.error)
        .finally(() => {
            relayRequestPending = false;
        });
}

// adjustTempOffset — Изменяет поправку температуры

function adjustTempOffset(delta) {
    tempOffset = Math.round((tempOffset + delta) * 10) / 10;
    document.getElementById('tempOffset').textContent = tempOffset.toFixed(1);
    fetch('/api/tempcal', {
        method: 'POST',
        body: new URLSearchParams({
            offset: tempOffset
        })
    });
}

// saveSchedule — Сохраняет расписание

function saveSchedule() {
    const startInput = document.getElementById('startTime');
    const endInput = document.getElementById('endTime');

    const start = startInput.value || '00:00:00';
    const end = endInput.value || '00:00:00';

    startInput.value = start;
    endInput.value = end;

    fetch('/api/schedule', {
            method: 'POST',
            body: new URLSearchParams({
                start,
                end
            })
        })
        .then(r => {
            if (!r.ok) throw new Error('Ошибка сохранения');

            dirty.start = false;
            dirty.end = false;
        })
        .catch(err => alert(err.message));
}

// loadConfig — Загружает настройки

function loadConfig() {
    fetch('/api/config').then(r => r.json()).then(data => {
        document.getElementById('ntpServer').value = data.ntpServer || '';
        document.getElementById('ntpGmt').value = data.gmt;
        document.getElementById('ntpInterval').value = data.interval;
        document.getElementById('lastSync').textContent = data.lastSync;


    });
}

// saveWifi — Сохраняет настройки WiFi

function saveWifi() {
    const ssid = document.getElementById('wifiSsid').value.trim();
    const pass = document.getElementById('wifiPass').value;

    if (!ssid) {
        alert('Укажите SSID');
        return;
    }

    if (!confirm(
            'Сохранить настройки WiFi?\n\n' +
            'Сеть: ' + ssid + '\n\n' +
            'После сохранения ESP32-S2 будет перезагружена.'
        )) {
        return;
    }

    fetch('/api/wifi', {
            method: 'POST',
            body: new URLSearchParams({
                ssid: ssid,
                pass: pass
            })
        })
        .then(response => {

            if (!response.ok) {
                throw new Error('Ошибка сохранения WiFi');
            }

            alert('Настройки WiFi сохранены. ESP перезагружается...');
        })
        .catch(error => {
            alert(error.message);
        });
}

// saveNtp — Сохраняет настройки NTP

function saveNtp() {
    const server = document.getElementById('ntpServer').value.trim();
    const gmt = document.getElementById('ntpGmt').value;
    const interval = document.getElementById('ntpInterval').value;

    if (!server) {
        alert('Укажите NTP-сервер');
        return;
    }

    if (!confirm(
            'Сохранить настройки NTP?\n\n' +
            'Сервер: ' + server + '\n' +
            'Часовой пояс: GMT ' + gmt + '\n' +
            'Период обновления: ' + interval + ' мин.\n\n' +
            'ESP32-S2 будет перезагружена.'
        )) {
        return;
    }

    fetch('/api/ntp', {
            method: 'POST',
            body: new URLSearchParams({
                server: server,
                gmt: gmt,
                interval: interval
            })
        })
        .then(response => {

            if (!response.ok) {
                throw new Error('Ошибка сохранения NTP');
            }

            alert('Настройки NTP сохранены. ESP перезагружается...');
        })
        .catch(error => {
            alert(error.message);
        });
}

// syncNtp — Запрашивает синхронизацию NTP

function syncNtp() {
    fetch('/api/ntpsync', {
        method: 'POST'
    }).then(r => r.json()).then(() => {
        loadConfig();
    });
}
let currentWifiMode = null;

// loadWifiMode — Загружает режим WiFi

function loadWifiMode() {

    fetch('/api/wifimode')
        .then(response => {

            if (!response.ok) {
                throw new Error('Не удалось получить режим WiFi');
            }

            return response.json();
        })
        .then(data => {

            currentWifiMode = data.mode;

            const btn = document.getElementById('wifiModeBtn');
            const hint = document.getElementById('wifiModeHint');

            if (currentWifiMode === 'STA') {

                btn.textContent = 'Переключить в AP';

                hint.textContent =
                    'Текущий режим: STA (подключение к роутеру)';

                btn.disabled = false;

            } else if (currentWifiMode === 'AP') {

                btn.textContent = 'Переключить в STA';

                hint.textContent =
                    'Текущий режим: AP (точка доступа)';
                btn.disabled = !data.hasCredentials;

                if (!data.hasCredentials) {

                    hint.textContent +=
                        '. Для перехода в STA сначала настройте WiFi.';
                }

            } else {

                btn.textContent = 'Режим неизвестен';

                btn.disabled = true;
            }
        })
        .catch(error => {

            console.error(error);

            document.getElementById('wifiModeBtn').disabled = true;

            document.getElementById('wifiModeHint').textContent =
                'Не удалось определить режим WiFi';
        });
}

// restartEsp — Перезагружает устройство

function restartEsp() {

    if (!confirm(
            'Перезагрузить ESP32-S2?\n\n' +
            'Соединение с устройством временно прервётся.'
        )) {
        return;
    }

    fetch('/api/restart', {
            method: 'POST'
        })
        .then(response => {

            if (!response.ok) {
                throw new Error('Ошибка запроса перезагрузки');
            }

            alert('ESP32-S2 перезагружается...');
        })
        .catch(error => {
            alert(error.message);
        });
}

// switchWifiMode — Запрашивает смену режима WiFi

function switchWifiMode() {

    if (currentWifiMode !== 'STA' &&
        currentWifiMode !== 'AP') {

        return;
    }

    const targetMode =
        currentWifiMode === 'STA' ? 'AP' : 'STA';

    const message =
        targetMode === 'AP' ?
        'Переключить ESP32-S2 в режим точки доступа AP?\n\n' +
        'Текущее WiFi-соединение будет разорвано.' :
        'Переключить ESP32-S2 в режим станции STA?\n\n' +
        'Устройство попытается подключиться к сохранённой сети WiFi.';

    if (!confirm(message)) {
        return;
    }

    const btn = document.getElementById('wifiModeBtn');

    btn.disabled = true;

    fetch('/api/wifimode', {
            method: 'POST',
            body: new URLSearchParams({
                mode: targetMode
            })
        })
        .then(async response => {

            if (!response.ok) {

                const errorText = await response.text();

                throw new Error(errorText || 'Ошибка переключения режима');
            }

            alert(
                'Переключение в режим ' + targetMode + ' запущено.\n\n' +
                'Соединение с ESP32-S2 будет временно разорвано.'
            );
        })
        .catch(error => {

            alert(error.message);

            loadWifiMode();
        });
}

// resetDevice — Сбрасывает настройки

function resetDevice() {
    if (!confirm('Точно сбросить все настройки?')) return;
    fetch('/api/reset', {
            method: 'POST'
        })
        .then(() => alert('Настройки сброшены. Устройство перезагружается...'));
}

// uploadOta — Загружает прошивку

function uploadOta() {

    const fileInput = document.getElementById('otaFile');
    const otaBtn = document.getElementById('otaBtn');
    const otaStatus = document.getElementById('otaStatus');

    if (!fileInput.files.length) {
        return;
    }

    const file = fileInput.files[0];
    if (!file.name.toLowerCase().endsWith('.bin')) {

        alert('Необходимо выбрать файл прошивки .bin');

        fileInput.value = '';

        return;
    }
    if (!confirm(
            'Обновить прошивку ESP32-S2?\n\n' +
            'Файл: ' + file.name + '\n\n' +
            'Во время обновления не отключайте питание.'
        )) {

        fileInput.value = '';

        return;
    }

    const formData = new FormData();

    formData.append('update', file);

    otaBtn.disabled = true;

    otaStatus.textContent = 'Загрузка прошивки...';

    fetch('/update', {
            method: 'POST',
            body: formData
        })
        .then(async response => {

            const result = await response.text();

            if (!response.ok || result.trim() !== 'OK') {
                throw new Error('Ошибка обновления прошивки');
            }

            otaStatus.textContent =
                'Прошивка обновлена. Перезагрузка ESP...';
        })
        .catch(error => {

            otaStatus.textContent = error.message;

            otaBtn.disabled = false;
        })
        .finally(() => {

            fileInput.value = '';
        });
}
let scanInterval = null;

// openScanModal — Открывает список сетей

function openScanModal() {
    document.getElementById('scanModal').style.display = 'flex';
    document.getElementById('scanList').innerHTML = '<div class="scan-loading">Сканирование...</div>';
    fetch('/api/wifiscan?start=1').then(() => {
        scanInterval = setInterval(pollScan, 1000);
    });
}

// closeScanModal — Закрывает список сетей

function closeScanModal() {
    document.getElementById('scanModal').style.display = 'none';
    if (scanInterval) {
        clearInterval(scanInterval);
        scanInterval = null;
    }
}

// pollScan — Проверяет сканирование сетей

function pollScan() {
    fetch('/api/wifiscan').then(r => r.json()).then(data => {
        if (data.status === 'done') {
            clearInterval(scanInterval);
            scanInterval = null;
            renderScanResults(data.networks);
        }
    });
}

// signalIcon — Выбирает значок сигнала

function signalIcon(rssi) {
    if (rssi >= -60) return '📶';
    if (rssi >= -75) return '📡';
    return '〰️';
}

// renderScanResults — Отображает найденные сети

function renderScanResults(networks) {
    const list = document.getElementById('scanList');
    if (!networks || networks.length === 0) {
        list.innerHTML = '<div class="scan-loading">Сети не найдены</div>';
        return;
    }
    networks.sort((a, b) => b.rssi - a.rssi);
    list.innerHTML = '';
    networks.forEach(net => {
        const item = document.createElement('div');
        item.className = 'scan-item';
        const ssidSpan = document.createElement('span');
        ssidSpan.className = 'ssid-name';
        ssidSpan.textContent = net.ssid || '(скрытая сеть)';

        const metaSpan = document.createElement('span');
        metaSpan.className = 'scan-meta';
        metaSpan.textContent = (net.secure ? '🔒 ' : '') + signalIcon(net.rssi) + ' ' + net.rssi + ' dBm';

        item.appendChild(ssidSpan);
        item.appendChild(metaSpan);
        item.onclick = () => {
            document.getElementById('wifiSsid').value = net.ssid;
            closeScanModal();
        };
        list.appendChild(item);
    });
}

fetch('/api/status').then(r => r.json()).then(updateStatus);

const evtSource = new EventSource('/events');
evtSource.addEventListener('status', function(e) {
    updateStatus(JSON.parse(e.data));
});
</script>
</body>
</html>
)rawliteral";

// begin — Запускает веб-сервер
void WebInterfaceClass::begin() {
  events.onConnect([](AsyncEventSourceClient *client) {
    client->send(buildStatusJson().c_str(), "status", millis(), 1000);
  });
  server.addHandler(&events);

  server.on("/", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/api/status", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", buildStatusJson());
  });

  server.on("/api/config", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    NtpSettings ns = TimeManager.getSettings();
    char buf[300];
    snprintf(buf, sizeof(buf),
             "{\"connected\":%s,\"ip\":\"%s\","
             "\"ntpServer\":\"%s\",\"gmt\":%d,\"interval\":%u,\"lastSync\":\"%s\"}",
             WifiManager.isStaConnected() ? "true" : "false",
             WifiManager.getCurrentIP().c_str(),
             ns.server, ns.gmtOffsetHours, (unsigned)ns.updateIntervalMinutes,
             TimeManager.getLastSyncFormatted().c_str());
    request->send(200, "application/json", buf);
  });

  server.on("/api/schedule", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("start", true) || !request->hasParam("end", true)) {
      request->send(400, "text/plain", "Missing params");
      return;
    }

    uint32_t start, end;

    if (!parseHMS(
          request->getParam("start", true)->value(),
          start)
        || !parseHMS(
          request->getParam("end", true)->value(),
          end)) {
      request->send(400, "text/plain", "Invalid time");
      return;
    }

    RelayControl.saveSchedule(start, end);

    request->send(200, "text/plain", "OK");
  });

  server.on("/api/manual", HTTP_POST, [](AsyncWebServerRequest *request) {
    RelayControl.manualToggle();
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/auto", AsyncWebRequestMethod::HTTP_POST, [](AsyncWebServerRequest *request) {
    RelayControl.manualExit();
    request->send(200, "application/json", buildStatusJson());
  });

  server.on("/api/wifi", AsyncWebRequestMethod::HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true)) {
      String ssid = request->getParam("ssid", true)->value();
      String pass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";
      WifiManager.saveCredentials(ssid, pass);
      request->send(200, "text/plain", "OK");
      WebInterface.requestRestart(1000);
    } else {
      request->send(400, "text/plain", "Missing ssid");
    }
  });

  server.on("/api/ntp", AsyncWebRequestMethod::HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("server", true)) {
      String srv = request->getParam("server", true)->value();
      int gmt = request->hasParam("gmt", true) ? request->getParam("gmt", true)->value().toInt() : 6;
      int intervalValue = request->hasParam("interval", true)
                            ? request->getParam("interval", true)->value().toInt()
                            : 90;

      uint32_t interval = constrain(intervalValue, 1, 1440);
      TimeManager.saveSettings(srv, gmt, interval);
      request->send(200, "text/plain", "OK");
      WebInterface.requestRestart(1000);
    } else {
      request->send(400, "text/plain", "Missing server");
    }
  });

  server.on("/api/ntpsync", AsyncWebRequestMethod::HTTP_POST, [](AsyncWebServerRequest *request) {
    bool ok = TimeManager.syncNow();
    request->send(200, "application/json", ok ? "{\"success\":true}" : "{\"success\":false}");
  });
  server.on("/api/tempcal", AsyncWebRequestMethod::HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("offset", true)) {
      float offset = request->getParam("offset", true)->value().toFloat();
      thermistor.setOffset(offset);
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing offset");
    }
  });
  server.on("/api/wifiscan", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("start")) {
      WiFi.scanDelete();
      WiFi.scanNetworks(true);  // Сканирование не блокирует веб-сервер
      request->send(200, "application/json", "{\"status\":\"started\"}");
      return;
    }

    int n = WiFi.scanComplete();

    if (n == WIFI_SCAN_RUNNING) {
      request->send(200, "application/json", "{\"status\":\"scanning\"}");
      return;
    }
    if (n == WIFI_SCAN_FAILED) {
      request->send(200, "application/json", "{\"status\":\"idle\"}");
      return;
    }

    String json = "{\"status\":\"done\",\"networks\":[";
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      json += "{\"ssid\":\"" + escapeJson(WiFi.SSID(i)) + "\",";
      json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
      json += "\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
      json += "}";
    }
    json += "]}";
    WiFi.scanDelete();
    request->send(200, "application/json", json);
  });

  server.on("/api/reset", AsyncWebRequestMethod::HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "OK");
    WifiManager.eraseAllSettings();
    WebInterface.requestRestart(500);
  });
  server.on(
    "/update", AsyncWebRequestMethod::HTTP_POST, [](AsyncWebServerRequest *request) {
      bool ok = !Update.hasError();
      request->send(200, "text/plain", ok ? "OK" : "FAIL");
      if (ok) WebInterface.requestRestart(800);
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (index == 0) {
        Update.begin(UPDATE_SIZE_UNKNOWN);
      }
      if (Update.isRunning()) {
        Update.write(data, len);
      }
      if (final && Update.isRunning()) {
        Update.end(true);
      }
    });

  server.on("/api/deviceinfo", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool connected = WifiManager.isStaConnected();
    int rssi = connected ? WiFi.RSSI() : -127;
    // Процент сигнала остаётся приблизительной оценкой
    int quality = connected ? constrain(2 * (rssi + 100), 0, 100) : 0;
    String signal = connected ? String(quality) + "% (" + String(rssi) + " dBm)" : "Нет соединения";
    String ssid = connected ? WiFi.SSID() : (WifiManager.getMode() == WIFI_MGR_MODE_AP ? WiFi.softAPSSID() : "Нет соединения");
    String ip = WifiManager.getCurrentIP();
    String mac = connected ? WiFi.macAddress() : WiFi.softAPmacAddress();

    uint64_t seconds = (uint64_t)esp_timer_get_time() / 1000000ULL;
    unsigned long days = seconds / 86400ULL;
    unsigned hours = (seconds / 3600ULL) % 24ULL;
    unsigned minutes = (seconds / 60ULL) % 60ULL;
    unsigned secs = seconds % 60ULL;
    char uptime[40];
    if (days) snprintf(uptime, sizeof(uptime), "%luд %02u:%02u:%02u", days, hours, minutes, secs);
    else snprintf(uptime, sizeof(uptime), "%02u:%02u:%02u", hours, minutes, secs);

    const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char mon[4] = { 0 };
    int day = 0, year = 0, hour = 0, minute = 0, second = 0;
    sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);
    int month = 1;
    for (int i = 0; i < 12; ++i) {
      if (strncmp(mon, months + i * 3, 3) == 0) {
        month = i + 1;
        break;
      }
    }
    char build[32];
    snprintf(build, sizeof(build), "%02d.%02d.%04d %02d:%02d:%02d", day, month, year, hour, minute, second);

    String json = "{\"ssid\":\"" + escapeJson(ssid) + "\",";
    json += "\"ip\":\"" + escapeJson(ip) + "\",";
    json += "\"mac\":\"" + escapeJson(mac) + "\",";
    json += "\"signal\":\"" + escapeJson(signal) + "\",";
    json += "\"uptime\":\"" + String(uptime) + "\",";
    json += "\"build\":\"" + String(build) + "\",";
    json += "\"version\":\"" + String(FIRMWARE_VERSION) + "\"}";
    request->send(200, "application/json", json);
  });

  server.on("/api/brightness", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"brightness\":" + String(display.getBrightness()) + "}";
    request->send(200, "application/json", json);
  });

  server.on("/api/brightness", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("value", true)) {
      request->send(400, "text/plain", "Missing value");
      return;
    }

    int value = request->getParam("value", true)->value().toInt();
    value = constrain(value, 0, 3);

    display.saveBrightness(value);

    request->send(200, "text/plain", "OK");
  });

  server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(
      200,
      "text/plain",
      "OK");
    WebInterface.requestRestart(1500);  // HTTP-ответ успевает отправиться
  });

  server.on("/api/wifimode", HTTP_GET, [](AsyncWebServerRequest *request) {
    String mode =
      WifiManager.getMode() == WIFI_MGR_MODE_AP
        ? "AP"
        : "STA";

    bool hasCredentials =
      WifiManager.getSavedSSID().length() > 0;

    String json = "{\"mode\":\"" + mode + "\",";
    json += "\"hasCredentials\":";
    json += hasCredentials ? "true" : "false";
    json += "}";

    request->send(
      200,
      "application/json",
      json);
  });

  server.on("/api/wifimode", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("mode", true)) {

      request->send(
        400,
        "text/plain",
        "Missing mode");

      return;
    }

    String mode =
      request->getParam("mode", true)->value();

    if (mode == "AP") {

      request->send(
        200,
        "text/plain",
        "OK");

      WifiManager.requestModeSwitch(
        WIFI_MGR_MODE_AP);

    } else if (mode == "STA") {

      if (WifiManager.getSavedSSID().length() == 0) {

        request->send(
          400,
          "text/plain",
          "WiFi credentials not configured");

        return;
      }

      request->send(
        200,
        "text/plain",
        "OK");

      WifiManager.requestModeSwitch(
        WIFI_MGR_MODE_STA);

    } else {

      request->send(
        400,
        "text/plain",
        "Invalid mode");
    }
  });

  server.begin();
}

// broadcastStatus — Рассылает состояние клиентам
void WebInterfaceClass::broadcastStatus() {
  events.send(buildStatusJson().c_str(), "status", millis());
}

// requestRestart — Планирует перезагрузку
void WebInterfaceClass::requestRestart(uint32_t delayMs) {
  s_restartPending = true;
  s_restartAt = millis() + delayMs;
}

// loop — Обрабатывает отложенный перезапуск
void WebInterfaceClass::loop() {
  if (s_restartPending && (long)(millis() - s_restartAt) >= 0) {
    delay(100);
    ESP.restart();
  }
}
