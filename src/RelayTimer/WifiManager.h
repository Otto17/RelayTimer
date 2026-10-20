/*

  ================================================================
  Copyright (c) 2026 Otto
  License: GPL-3.0-or-later (см. LICENSE)
  ================================================================

  ОБЪЯВЛЕНИЯ ФУНКЦИЙ ДЛЯ ПОДКЛЮЧЕНИЯ WI-FI

*/

#pragma once
#include <Arduino.h>

// WifiOperatingMode — Режим работы WiFi
enum WifiOperatingMode : uint8_t {
  WIFI_MGR_MODE_AP = 0,
  WIFI_MGR_MODE_STA = 1
};

typedef void (*WifiResetCallback)();

// WifiManagerClass — Управление WiFi
class WifiManagerClass {
public:
  // begin — Инициализирует модуль
  void begin();
  // loop — Обрабатывает события модуля
  void loop();

  // getMode — Возвращает текущий режим
  WifiOperatingMode getMode() const {
    return _mode;
  }

  // isStaConnected — Проверяет подключение STA
  bool isStaConnected() const;
  // getSavedSSID — Возвращает сохранённый SSID
  String getSavedSSID() const;
  // getCurrentIP — Возвращает IP-адрес
  String getCurrentIP() const;

  // saveCredentials — Сохраняет данные WiFi
  void saveCredentials(const String& ssid, const String& pass);
  // eraseAllSettings — Удаляет настройки устройства
  void eraseAllSettings();
  // requestModeSwitch — Запрашивает смену режима WiFi
  void requestModeSwitch(WifiOperatingMode mode);

  // setFactoryResetCallback — Назначает обработчик сброса
  void setFactoryResetCallback(WifiResetCallback cb) {
    _resetCallback = cb;
  }

  // consumeJustConnectedFlag — Возвращает признак нового IP
  bool consumeJustConnectedFlag();

private:
  WifiOperatingMode _mode = WIFI_MGR_MODE_AP;
  bool _modeSwitchPending = false;
  WifiOperatingMode _requestedMode = WIFI_MGR_MODE_AP;
  unsigned long _modeSwitchAt = 0;

  char _ssid[64] = { 0 };
  char _pass[64] = { 0 };

  bool _buttonActive = false;
  unsigned long _buttonPressStart = 0;
  bool _holdThresholdReached = false;

  unsigned long _lastLedToggle = 0;
  bool _ledState = false;

  WifiResetCallback _resetCallback = nullptr;

  // loadCredentials — Загружает данные WiFi
  void loadCredentials();
  // startAP — Запускает точку доступа
  void startAP();
  // startSTA — Подключает к роутеру
  void startSTA();
  // handleButton — Обрабатывает кнопку BOOT
  void handleButton();
  // registerEvents — Регистрирует события WiFi
  void registerEvents();
  // updateLed — Управляет светодиодом
  void updateLed();
};

extern WifiManagerClass WifiManager;  // WifiManager — Контроллер WiFi
