/*

  ================================================================
  Copyright (c) 2026 Otto
  License: GPL-3.0-or-later (см. LICENSE)
  ================================================================

  ПОДКЛЮЧЕНИЕ WI-FI, РЕЖИМЫ STA И AP

*/

#include "WifiManager.h"
#include <WiFi.h>
#include <Preferences.h>
#include "Config.h"
#include "esp_wifi.h"

WifiManagerClass WifiManager;  // WifiManager — Управление подключением

static volatile bool s_justGotIp = false;  // s_justGotIp — Признак нового адреса

// onWiFiEvent — Обрабатывает события WiFi
static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_AP_START:
      break;
    case ARDUINO_EVENT_WIFI_AP_STOP:
      break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      s_justGotIp = true;
      break;
    default:
      break;
  }
}

// loadCredentials — Загружает данные WiFi
void WifiManagerClass::loadCredentials() {
  Preferences prefs;
  prefs.begin(NVS_NS_WIFI, true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();

  strncpy(_ssid, ssid.c_str(), sizeof(_ssid) - 1);
  strncpy(_pass, pass.c_str(), sizeof(_pass) - 1);
}

// saveCredentials — Сохраняет данные WiFi
void WifiManagerClass::saveCredentials(const String& ssid, const String& pass) {
  Preferences prefs;
  prefs.begin(NVS_NS_WIFI, false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();

  strncpy(_ssid, ssid.c_str(), sizeof(_ssid) - 1);
  strncpy(_pass, pass.c_str(), sizeof(_pass) - 1);
}

// eraseAllSettings — Удаляет настройки устройства
void WifiManagerClass::eraseAllSettings() {
  Preferences prefs;
  prefs.begin(NVS_NS_WIFI, false);
  prefs.clear();
  prefs.end();

  prefs.begin(NVS_NS_NTP, false);
  prefs.clear();
  prefs.end();

  prefs.begin(NVS_NS_RELAY, false);
  prefs.clear();
  prefs.end();

  prefs.begin(NVS_NS_TEMP, false);
  prefs.clear();
  prefs.end();

  prefs.begin(NVS_NS_DISPLAY, false);
  prefs.clear();
  prefs.end();
}

// startAP — Запускает точку доступа
void WifiManagerClass::startAP() {
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  delay(500);

  WiFi.mode(WIFI_AP);
  delay(500);

  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, gateway, subnet);

  WiFi.softAP(AP_SSID_DEFAULT, AP_PASS_DEFAULT, AP_CHANNEL, 0, AP_MAX_CLIENTS);
  esp_wifi_set_ps(WIFI_PS_NONE);  // Отключение энергосбережения повышает совместимость AP

  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_AP, &conf);
  conf.ap.pmf_cfg.capable = true;
  conf.ap.pmf_cfg.required = false;  // Не все клиенты поддерживают обязательный PMF
  esp_wifi_set_config(WIFI_IF_AP, &conf);

  esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

  _mode = WIFI_MGR_MODE_AP;
}

// startSTA — Подключает к роутеру
void WifiManagerClass::startSTA() {
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  delay(500);

  WiFi.mode(WIFI_STA);
  delay(500);

  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

  WiFi.setTxPower(WIFI_POWER_11dBm);

  WiFi.begin(_ssid, _pass);
  _mode = WIFI_MGR_MODE_STA;
}

// registerEvents — Регистрирует события WiFi
void WifiManagerClass::registerEvents() {
  WiFi.onEvent(onWiFiEvent);
}

// begin — Запускает WiFi
void WifiManagerClass::begin() {
  pinMode(PIN_RESET_BUTTON, INPUT_PULLUP);
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);

  registerEvents();

  loadCredentials();

  if (strlen(_ssid) > 0) {
    startSTA();
  } else {
    startAP();
  }
}

// handleButton — Обрабатывает кнопку BOOT
void WifiManagerClass::handleButton() {
  bool pressed = (digitalRead(PIN_RESET_BUTTON) == LOW);

  if (pressed) {
    if (!_buttonActive) {
      _buttonActive = true;
      _buttonPressStart = millis();
      _holdThresholdReached = false;
      _ledState = false;
      digitalWrite(PIN_STATUS_LED, LOW);
    }

    unsigned long held = millis() - _buttonPressStart;

    if (held >= RESET_HOLD_MS) {
      _holdThresholdReached = true;
      updateLed();
    }

  } else {
    if (_buttonActive) {
      unsigned long held = millis() - _buttonPressStart;
      digitalWrite(PIN_STATUS_LED, LOW);

      if (_holdThresholdReached) {
        if (_resetCallback) _resetCallback();

        eraseAllSettings();
        delay(300);
        ESP.restart();

      } else if (held >= BUTTON_DEBOUNCE_MS) {
        if (_mode == WIFI_MGR_MODE_AP) {
          if (strlen(_ssid) > 0) {

            startSTA();
          }
        } else {

          startAP();
        }
      }
    }
    _buttonActive = false;
    _holdThresholdReached = false;
  }
}

// updateLed — Управляет светодиодом
void WifiManagerClass::updateLed() {
  if (millis() - _lastLedToggle >= LED_BLINK_FAST_MS) {
    _lastLedToggle = millis();
    _ledState = !_ledState;
    digitalWrite(PIN_STATUS_LED, _ledState ? HIGH : LOW);
  }
}

// requestModeSwitch — Запрашивает смену режима WiFi
void WifiManagerClass::requestModeSwitch(WifiOperatingMode mode) {
  if (_mode == mode) {
    return;
  }
  if (mode == WIFI_MGR_MODE_STA && strlen(_ssid) == 0) {

    return;
  }
  _requestedMode = mode;
  _modeSwitchAt = millis() + 1500UL;  // HTTP-ответ успевает дойти до браузера

  _modeSwitchPending = true;
}

// loop — Обрабатывает WiFi и кнопку BOOT
void WifiManagerClass::loop() {
  handleButton();
  if (_modeSwitchPending) {

    unsigned long now = millis();

    if ((int32_t)(now - _modeSwitchAt) >= 0) {

      _modeSwitchPending = false;

      if (_requestedMode == WIFI_MGR_MODE_AP) {

        startAP();

      } else if (_requestedMode == WIFI_MGR_MODE_STA) {
        if (strlen(_ssid) > 0) {

          startSTA();
        }
      }
    }
  }
}

// isStaConnected — Проверяет подключение STA
bool WifiManagerClass::isStaConnected() const {
  return (_mode == WIFI_MGR_MODE_STA) && (WiFi.status() == WL_CONNECTED);
}

// getSavedSSID — Возвращает сохранённый SSID
String WifiManagerClass::getSavedSSID() const {
  return String(_ssid);
}

// getCurrentIP — Возвращает IP-адрес
String WifiManagerClass::getCurrentIP() const {
  if (_mode == WIFI_MGR_MODE_AP) {
    return WiFi.softAPIP().toString();
  }
  return isStaConnected() ? WiFi.localIP().toString() : "не подключено";
}

// consumeJustConnectedFlag — Возвращает признак нового IP
bool WifiManagerClass::consumeJustConnectedFlag() {
  if (s_justGotIp) {
    s_justGotIp = false;
    return true;
  }
  return false;
}
