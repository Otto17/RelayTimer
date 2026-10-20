/*

  ================================================================
  Copyright (c) 2026 Otto
  License: GPL-3.0-or-later (см. LICENSE)
  ================================================================

  ОБЪЯВЛЕНИЯ ФУНКЦИЙ ДЛЯ УПРАВЛЕНИЯ ВРЕМЕНЕМ

*/

#pragma once
#include <Arduino.h>
#include <time.h>

// NtpSettings — Настройки времени
struct NtpSettings {
  char server[64];
  int gmtOffsetHours;
  uint32_t updateIntervalMinutes;
};

// TimeManagerClass — Служба времени
class TimeManagerClass {
public:
  // begin — Инициализирует модуль
  void begin();
  // loop — Обрабатывает события модуля
  void loop();
  // syncNow — Запускает синхронизацию времени
  bool syncNow();

  // isSynced — Проверяет синхронизацию времени
  bool isSynced() const {
    return _synced;
  }

  // getSecondsSinceMidnight — Возвращает секунды от полуночи
  uint32_t getSecondsSinceMidnight();
  // getFormattedDateTime — Возвращает дату и время
  String getFormattedDateTime();
  // getLastSyncFormatted — Возвращает время синхронизации
  String getLastSyncFormatted();

  // getSettings — Возвращает настройки NTP
  NtpSettings getSettings() const {
    return _settings;
  }

  // saveSettings — Сохраняет настройки NTP
  void saveSettings(const String& server, int gmtOffsetHours, uint32_t intervalMinutes);

private:
  NtpSettings _settings;
  bool _synced = false;
  time_t _lastSync = 0;
  unsigned long _lastSyncMillis = 0;

  bool _ntpStarted = false;
  unsigned long _wifiConnectedAt = 0;
  unsigned long _ntpStartedAt = 0;
  unsigned long _lastNtpAttempt = 0;

  // loadSettings — Загружает настройки
  void loadSettings();
  // applyConfigTime — Применяет настройки NTP
  void applyConfigTime();
};

extern TimeManagerClass TimeManager;  // TimeManager — Контроллер времени
