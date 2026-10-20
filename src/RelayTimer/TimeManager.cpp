/*

  ================================================================
  Copyright (c) 2026 Otto
  License: GPL-3.0-or-later (см. LICENSE)
  ================================================================

  ПОЛУЧЕНИЕ, СИНХРОНИЗАЦИЯ И ОБРАБОТКА ВРЕМЕНИ

*/

#include "TimeManager.h"
#include <Preferences.h>
#include "Config.h"
#include "WifiManager.h"

TimeManagerClass TimeManager;  // TimeManager — Служба времени

// loadSettings — Загружает настройки
void TimeManagerClass::loadSettings() {
  Preferences prefs;
  prefs.begin(NVS_NS_NTP, true);
  String srv = prefs.getString("server", "ru.pool.ntp.org");
  int gmt = prefs.getInt("gmt", 6);
  uint32_t interval = prefs.getUInt("interval", 90);
  prefs.end();

  strncpy(_settings.server, srv.c_str(), sizeof(_settings.server) - 1);
  _settings.server[sizeof(_settings.server) - 1] = '\0';
  _settings.gmtOffsetHours = gmt;
  _settings.updateIntervalMinutes = interval;
}

// applyConfigTime — Применяет настройки NTP
void TimeManagerClass::applyConfigTime() {
  configTime(_settings.gmtOffsetHours * 3600, 0, _settings.server);
}

// begin — Запускает службу времени
void TimeManagerClass::begin() {
  loadSettings();

  _synced = false;
  _ntpStarted = false;
  _wifiConnectedAt = 0;
  _ntpStartedAt = 0;
  _lastNtpAttempt = 0;
  _lastSync = 0;
  _lastSyncMillis = 0;
}

// syncNow — Запускает синхронизацию времени
bool TimeManagerClass::syncNow() {
  if (!WifiManager.isStaConnected())
    return false;

  applyConfigTime();

  _ntpStarted = true;
  _ntpStartedAt = millis();
  _lastNtpAttempt = millis();

  return true;
}

// loop — Обслуживает синхронизацию NTP
void TimeManagerClass::loop() {
  unsigned long now = millis();
  if (!WifiManager.isStaConnected()) {
    _wifiConnectedAt = 0;
    _ntpStarted = false;
    return;
  }

  if (_wifiConnectedAt == 0) {
    _wifiConnectedAt = now;
    return;
  }

  if (now - _wifiConnectedAt < 5000UL)
    return;

  if (!_ntpStarted) {
    syncNow();
    return;
  }

  if (!_synced) {
    time_t currentTime = time(nullptr);
    if (currentTime > 1700000000) {
      _synced = true;
      _lastSync = currentTime;
      _lastSyncMillis = now;
      return;
    }

    if (now - _lastNtpAttempt >= 15000UL) {
      syncNow();
    }

    return;
  }
  unsigned long intervalMs = max(_settings.updateIntervalMinutes, 1UL) * 60000UL;

  if (now - _lastSyncMillis >= intervalMs && now - _lastNtpAttempt >= 15000UL) {
    syncNow();
    _lastSyncMillis = now;
  }
}

// getSecondsSinceMidnight — Возвращает секунды от полуночи
uint32_t TimeManagerClass::getSecondsSinceMidnight() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) return 0;
  return timeinfo.tm_hour * 3600UL + timeinfo.tm_min * 60UL + timeinfo.tm_sec;
}

// getFormattedDateTime — Возвращает дату и время
String TimeManagerClass::getFormattedDateTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) return "Нет данных";
  char buf[32];
  snprintf(buf, sizeof(buf), "%02d.%02d.%04d %02d:%02d:%02d",
           timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return String(buf);
}

// getLastSyncFormatted — Возвращает время синхронизации
String TimeManagerClass::getLastSyncFormatted() {
  if (_lastSync == 0) return "Еще не выполнялась";
  struct tm timeinfo;
  localtime_r(&_lastSync, &timeinfo);
  char buf[32];
  snprintf(buf, sizeof(buf), "%02d.%02d.%04d %02d:%02d:%02d",
           timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return String(buf);
}

// saveSettings — Сохраняет настройки NTP
void TimeManagerClass::saveSettings(const String& server, int gmtOffsetHours, uint32_t intervalMinutes) {
  Preferences prefs;
  prefs.begin(NVS_NS_NTP, false);
  prefs.putString("server", server);
  prefs.putInt("gmt", gmtOffsetHours);
  prefs.putUInt("interval", intervalMinutes);
  prefs.end();
}
