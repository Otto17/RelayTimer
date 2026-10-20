/*

  ================================================================
  Copyright (c) 2026 Otto
  License: GPL-3.0-or-later (см. LICENSE)
  ================================================================

  ОСНОВНОЙ ФАЙЛ СКЕТЧА, ИНИЦИАЛИЗАЦИЯ И ГЛАВНЫЙ ЦИКЛ

*/

#include <WiFi.h>

#include "Config.h"
#include "Display.h"
#include "Thermistor.h"
#include "TimeManager.h"
#include "RelayControl.h"
#include "WifiManager.h"
#include "WebInterface.h"

TM1637Driver display(PIN_TM1637_CLK, PIN_TM1637_DIO);  // Дисплей TM1637

// showAPDisplay — Отображает режим AP
void showAPDisplay() {

  const uint8_t SEG_A_LETTER = 0x77;
  const uint8_t SEG_P_LETTER = 0x37;

  display.showRawSegments(
    0x00,
    SEG_A_LETTER,
    SEG_P_LETTER,
    0x00);
}

float g_currentTemperature = 0.0f;  // Текущая температура

static unsigned long lastTempRead = 0;
static unsigned long lastStatusBroadcast = 0;

// DisplayOperatingState — Режим отображения
enum DisplayOperatingState : uint8_t {
  DISPLAY_WAIT_WIFI = 0,
  DISPLAY_SHOW_IP,
  DISPLAY_SHOW_TEMP,
  DISPLAY_SHOW_AP

};

static DisplayOperatingState displayState = DISPLAY_WAIT_WIFI;  // Режим дисплея
static WifiOperatingMode lastDisplayWifiMode = WIFI_MGR_MODE_AP;
static bool displayWifiInitialized = false;
static uint32_t touchBase = 0;     // Уровень покоя
static bool touchPressed = false;  // Подтверждённое нажатие
static unsigned long touchChangedAt = 0;
static unsigned long touchLastSampleAt = 0;
static unsigned long touchReadyAt = 0;
static unsigned long touchLastTriggerAt = 0;
static bool touchCandidate = false;  // Ожидаемое состояние
static const unsigned long TOUCH_SAMPLE_MS = 10;
static const unsigned long TOUCH_STARTUP_DELAY_MS = 2000;
static const unsigned long TOUCH_PRESS_CONFIRM_MS = 50;
static const unsigned long TOUCH_RELEASE_CONFIRM_MS = 150;
static const unsigned long TOUCH_RETRIGGER_MS = 400;
static const uint32_t TOUCH_PRESS_THRESHOLD = 15000;
static const uint32_t TOUCH_RELEASE_THRESHOLD = 10000;

// initTouchRelay — Калибрует сенсорную кнопку
void initTouchRelay() {
  const int SAMPLE_COUNT = 31;
  uint32_t samples[SAMPLE_COUNT];

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    samples[i] = touchRead(PIN_TOUCH_RELAY);
    delay(20);
  }

  for (int i = 1; i < SAMPLE_COUNT; i++) {
    uint32_t key = samples[i];
    int j = i - 1;
    while (j >= 0 && samples[j] > key) {
      samples[j + 1] = samples[j];
      j--;
    }

    samples[j + 1] = key;
  }

  touchBase = samples[SAMPLE_COUNT / 2];  // Медиана исключает случайные выбросы
  touchPressed = false;
  touchCandidate = false;

  touchChangedAt = millis();

  touchLastSampleAt = 0;
  touchLastTriggerAt = 0;
  touchReadyAt = millis() + TOUCH_STARTUP_DELAY_MS;  // Сенсор стабилизируется после запуска
}

// handleTouchRelay — Обрабатывает прикосновения к сенсорной кнопке
void handleTouchRelay() {

  unsigned long now = millis();
  if (now - touchLastSampleAt < TOUCH_SAMPLE_MS) {
    return;
  }

  touchLastSampleAt = now;

  uint32_t raw = touchRead(PIN_TOUCH_RELAY);
  if (raw == 0 || touchBase == 0) {
    return;
  }
  if ((int32_t)(now - touchReadyAt) < 0) {
    return;
  }

  bool detected;

  if (touchPressed) {
    detected = raw > TOUCH_RELEASE_THRESHOLD;  // Гистерезис исключает дребезг

  } else {
    detected = raw >= TOUCH_PRESS_THRESHOLD;
  }

  if (detected != touchCandidate) {
    touchCandidate = detected;
    touchChangedAt = now;
  }

  unsigned long confirmTime = touchCandidate
                                ? TOUCH_PRESS_CONFIRM_MS
                                : TOUCH_RELEASE_CONFIRM_MS;

  // Подтверждение исключает короткие помехи
  if (now - touchChangedAt < confirmTime) {
    return;
  }
  if (touchPressed == touchCandidate) {
    return;
  }

  touchPressed = touchCandidate;

  if (touchPressed) {
    if (touchLastTriggerAt != 0 && now - touchLastTriggerAt < TOUCH_RETRIGGER_MS) {
      return;
    }

    touchLastTriggerAt = now;
    RelayControl.manualToggle();
  }
}

// IpDisplayState — Состояние показа IP
struct IpDisplayState {
  bool active = false;
  IPAddress ip;
  int index = 0;
  unsigned long nextChangeAt = 0;
};

static IpDisplayState ipDisplay;

// startIpDisplay — Запускает показ IP-адреса
void startIpDisplay(IPAddress ip) {
  ipDisplay.active = true;
  ipDisplay.ip = ip;
  ipDisplay.index = 0;
  ipDisplay.nextChangeAt = 0;
  displayState = DISPLAY_SHOW_IP;
}

// handleIpDisplay — Переключает октеты IP
void handleIpDisplay() {
  if (!ipDisplay.active) {
    return;
  }

  unsigned long now = millis();
  if (ipDisplay.index == 0 && ipDisplay.nextChangeAt == 0) {
    display.showNumberPlain(ipDisplay.ip[0]);
    ipDisplay.nextChangeAt = now + IP_DISPLAY_STEP_MS;
    ipDisplay.index = 1;

    return;
  }

  if ((int32_t)(now - ipDisplay.nextChangeAt) < 0) {
    return;
  }

  if (ipDisplay.index < 4) {

    display.showNumberPlain(ipDisplay.ip[ipDisplay.index]);
    ipDisplay.index++;
    ipDisplay.nextChangeAt = now + IP_DISPLAY_STEP_MS;

    return;
  }

  ipDisplay.active = false;

  displayState = DISPLAY_SHOW_TEMP;
  lastTempRead = 0;
}

// onFactoryResetVisualFeedback — Показывает подтверждение сброса
void onFactoryResetVisualFeedback() {
  display.showRawSegments(0x40, 0x40, 0x40, 0x40);
  delay(1000);
}

// handleDisplayWifiMode — Синхронизирует дисплей с WiFi
void handleDisplayWifiMode() {

  WifiOperatingMode currentMode = WifiManager.getMode();

  if (!displayWifiInitialized) {
    displayWifiInitialized = true;
    lastDisplayWifiMode = currentMode;

    if (currentMode == WIFI_MGR_MODE_AP) {
      ipDisplay.active = false;
      displayState = DISPLAY_SHOW_AP;
      showAPDisplay();

    } else {
      displayState = DISPLAY_WAIT_WIFI;
      display.clear();
    }

    return;
  }

  if (currentMode == lastDisplayWifiMode) {
    return;
  }

  lastDisplayWifiMode = currentMode;

  if (currentMode == WIFI_MGR_MODE_AP) {
    ipDisplay.active = false;
    displayState = DISPLAY_SHOW_AP;
    showAPDisplay();

    return;
  }

  if (currentMode == WIFI_MGR_MODE_STA) {
    ipDisplay.active = false;
    displayState = DISPLAY_WAIT_WIFI;
    display.clear();

    return;
  }
}

// setup — Инициализирует устройство
void setup() {
  digitalWrite(PIN_RELAY, HIGH);
  pinMode(PIN_RELAY, OUTPUT);

  display.begin();
  display.clear();  // До IP дисплей остаётся пустым
  displayState = DISPLAY_WAIT_WIFI;

  thermistor.begin();

  RelayControl.begin(PIN_RELAY);
  initTouchRelay();

  WifiManager.setFactoryResetCallback(onFactoryResetVisualFeedback);

  WifiManager.begin();
  TimeManager.begin();
  WebInterface.begin();
}

// loop — Обрабатывает работу устройства
void loop() {
  WifiManager.loop();
  handleTouchRelay();
  TimeManager.loop();
  RelayControl.loop();
  WebInterface.loop();
  static bool lastRelayState = false;
  static RelayMode lastRelayMode = RELAY_MODE_AUTO;
  static bool statusInitialized = false;

  bool relayState = RelayControl.isRelayOn();
  RelayMode relayMode = RelayControl.getMode();

  if (!statusInitialized || relayState != lastRelayState || relayMode != lastRelayMode) {

    lastRelayState = relayState;
    lastRelayMode = relayMode;
    statusInitialized = true;

    WebInterface.broadcastStatus();
  }

  handleDisplayWifiMode();

  if (WifiManager.consumeJustConnectedFlag()) {
    if (WifiManager.getMode() == WIFI_MGR_MODE_STA && WifiManager.isStaConnected()) {
      startIpDisplay(WiFi.localIP());
    }
  }

  if (WifiManager.getMode() == WIFI_MGR_MODE_STA && !WifiManager.isStaConnected()) {
    if (displayState != DISPLAY_WAIT_WIFI) {
      ipDisplay.active = false;
      displayState = DISPLAY_WAIT_WIFI;
      display.clear();
    }
  }

  if (displayState == DISPLAY_SHOW_IP) {
    handleIpDisplay();
  }

  if (millis() - lastTempRead >= THERM_READ_INTERVAL_MS || lastTempRead == 0) {
    lastTempRead = millis();
    g_currentTemperature = thermistor.readTemperatureC();

    if (displayState == DISPLAY_SHOW_TEMP && WifiManager.getMode() == WIFI_MGR_MODE_STA && WifiManager.isStaConnected()) {
      float displayTemp =
        (g_currentTemperature < 0.0f)
          ? 0.0f
          : g_currentTemperature;

      display.showTemperature(displayTemp);
    }
  }

  if (millis() - lastStatusBroadcast > 1000) {
    lastStatusBroadcast = millis();
    WebInterface.broadcastStatus();
  }
}
