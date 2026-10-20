/*

  ================================================================
  Copyright (c) 2026 Otto
  License: GPL-3.0-or-later (см. LICENSE)
  ================================================================

  ОБЪЯВЛЕНИЯ ФУНКЦИЙ ДЛЯ УПРАВЛЕНИЯ РЕЛЕ

*/

#pragma once
#include <Arduino.h>

// RelayMode — Режим управления реле
enum RelayMode : uint8_t {
  RELAY_MODE_AUTO = 0,
  RELAY_MODE_MANUAL = 1
};

// RelaySchedule — Границы расписания
struct RelaySchedule {
  uint32_t startSeconds;
  uint32_t endSeconds;
};

// RelayControlClass — Управление реле
class RelayControlClass {
public:
  // begin — Инициализирует модуль
  void begin(uint8_t pin);
  // loop — Обрабатывает события модуля
  void loop();

  // saveScheduleStart — Сохраняет начало расписания
  void saveScheduleStart(uint32_t startSeconds);
  // saveScheduleEnd — Сохраняет конец расписания
  void saveScheduleEnd(uint32_t endSeconds);
  // saveSchedule — Сохраняет расписание
  void saveSchedule(uint32_t startSeconds, uint32_t endSeconds);

  // getSchedule — Возвращает расписание
  RelaySchedule getSchedule() const;

  // manualToggle — Переключает ручной режим
  void manualToggle();
  // manualSet — Задаёт ручное состояние реле
  void manualSet(bool on);
  // manualExit — Завершает ручной режим
  void manualExit();

  // getMode — Возвращает текущий режим
  RelayMode getMode() const;
  // isRelayOn — Возвращает состояние реле
  bool isRelayOn() const;

private:
  uint8_t _pin = 0;

  RelaySchedule _schedule = { 0, 0 };
  RelayMode _mode = RELAY_MODE_AUTO;
  bool _relayOn = false;

  mutable portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;

  bool _commandPending = false;
  bool _commandOn = false;

  // loadSchedule — Загружает расписание
  void loadSchedule();
  // applyRelay — Устанавливает состояние реле
  void applyRelay(bool on);
  // isInSchedule — Проверяет время расписания
  bool isInSchedule(uint32_t nowSeconds) const;
};

extern RelayControlClass RelayControl;  // RelayControl — Контроллер реле
