/*

  ================================================================
  Copyright (c) 2026 Otto
  License: GPL-3.0-or-later (см. LICENSE)
  ================================================================

  УПРАВЛЕНИЕ РЕЛЕ И ЛОГИКА ЕГО РАБОТЫ

*/

#include "RelayControl.h"
#include <Preferences.h>
#include "Config.h"
#include "TimeManager.h"

RelayControlClass RelayControl;  // RelayControl — Управление реле

// begin — Инициализирует реле
void RelayControlClass::begin(uint8_t pin) {
  _pin = pin;
  // HIGH исключает включение реле при старте
  digitalWrite(_pin, HIGH);
  pinMode(_pin, OUTPUT);

  _relayOn = false;
  _mode = RELAY_MODE_AUTO;

  loadSchedule();
}

// loadSchedule — Загружает расписание
void RelayControlClass::loadSchedule() {
  Preferences prefs;

  if (prefs.begin(NVS_NS_RELAY, true)) {
    _schedule.startSeconds = prefs.getUInt("start", 0);
    _schedule.endSeconds = prefs.getUInt("end", 0);
    prefs.end();
  }

  if (_schedule.startSeconds >= 86400 || _schedule.endSeconds >= 86400) {
    _schedule = { 0, 0 };
  }
}

// getSchedule — Возвращает расписание
RelaySchedule RelayControlClass::getSchedule() const {
  portENTER_CRITICAL(&_mux);
  RelaySchedule result = _schedule;
  portEXIT_CRITICAL(&_mux);
  return result;
}

// getMode — Возвращает текущий режим
RelayMode RelayControlClass::getMode() const {
  portENTER_CRITICAL(&_mux);
  RelayMode result = _mode;
  portEXIT_CRITICAL(&_mux);
  return result;
}

// isRelayOn — Возвращает состояние реле
bool RelayControlClass::isRelayOn() const {
  portENTER_CRITICAL(&_mux);
  bool result = _relayOn;
  portEXIT_CRITICAL(&_mux);
  return result;
}

// saveSchedule — Сохраняет расписание
void RelayControlClass::saveSchedule(
  uint32_t startSeconds,
  uint32_t endSeconds) {
  if (startSeconds >= 86400 || endSeconds >= 86400)
    return;

  Preferences prefs;

  if (prefs.begin(NVS_NS_RELAY, false)) {
    prefs.putUInt("start", startSeconds);
    prefs.putUInt("end", endSeconds);
    prefs.end();
  }

  portENTER_CRITICAL(&_mux);
  _schedule.startSeconds = startSeconds;
  _schedule.endSeconds = endSeconds;
  portEXIT_CRITICAL(&_mux);
}

// saveScheduleStart — Сохраняет начало расписания
void RelayControlClass::saveScheduleStart(uint32_t startSeconds) {
  RelaySchedule s = getSchedule();
  saveSchedule(startSeconds, s.endSeconds);
}

// saveScheduleEnd — Сохраняет конец расписания
void RelayControlClass::saveScheduleEnd(uint32_t endSeconds) {
  RelaySchedule s = getSchedule();
  saveSchedule(s.startSeconds, endSeconds);
}

// isInSchedule — Проверяет время расписания
bool RelayControlClass::isInSchedule(uint32_t nowSeconds) const {
  RelaySchedule s = getSchedule();

  if (s.startSeconds == s.endSeconds)
    return false;

  if (s.startSeconds < s.endSeconds) {
    return nowSeconds >= s.startSeconds && nowSeconds < s.endSeconds;
  }

  return nowSeconds >= s.startSeconds || nowSeconds < s.endSeconds;
}

// applyRelay — Устанавливает состояние реле
void RelayControlClass::applyRelay(bool on) {
  if (_relayOn == on)
    return;

  digitalWrite(_pin, on ? LOW : HIGH);

  portENTER_CRITICAL(&_mux);
  _relayOn = on;
  portEXIT_CRITICAL(&_mux);
}

// manualToggle — Переключает ручной режим
void RelayControlClass::manualToggle() {
  portENTER_CRITICAL(&_mux);
  if (_mode == RELAY_MODE_MANUAL) {
    _mode = RELAY_MODE_AUTO;
  } else {
    _commandOn = !_relayOn;
    _mode = RELAY_MODE_MANUAL;
  }

  _commandPending = true;

  portEXIT_CRITICAL(&_mux);
}

// manualSet — Задаёт ручное состояние реле
void RelayControlClass::manualSet(bool on) {
  portENTER_CRITICAL(&_mux);

  _mode = RELAY_MODE_MANUAL;
  _commandOn = on;
  _commandPending = true;

  portEXIT_CRITICAL(&_mux);
}

// manualExit — Завершает ручной режим
void RelayControlClass::manualExit() {
  portENTER_CRITICAL(&_mux);

  _mode = RELAY_MODE_AUTO;
  _commandPending = true;

  portEXIT_CRITICAL(&_mux);
}

// loop — Обрабатывает управление реле
void RelayControlClass::loop() {
  static unsigned long lastCheck = 0;

  unsigned long now = millis();

  RelayMode mode;
  bool commandPending;
  bool commandOn;

  portENTER_CRITICAL(&_mux);

  mode = _mode;
  commandPending = _commandPending;
  commandOn = _commandOn;
  _commandPending = false;

  portEXIT_CRITICAL(&_mux);

  if (mode == RELAY_MODE_MANUAL) {
    if (commandPending)
      applyRelay(commandOn);

    return;
  }

  if (!commandPending && now - lastCheck < 1000)
    return;

  lastCheck = now;

  // Без точного времени расписание не применяется
  if (!TimeManager.isSynced()) {
    applyRelay(false);
    return;
  }

  uint32_t seconds = TimeManager.getSecondsSinceMidnight();
  bool shouldBeOn = isInSchedule(seconds);
  applyRelay(shouldBeOn);
}
