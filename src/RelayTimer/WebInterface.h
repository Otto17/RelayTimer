/*

  ================================================================
  Copyright (c) 2026 Otto
  License: GPL-3.0-or-later (см. LICENSE)
  ================================================================

  ОБЪЯВЛЕНИЯ ФУНКЦИЙ ДЛЯ РАБОТЫ С WEB-ИНТЕРФЕЙСОМ

*/

#pragma once
#include <Arduino.h>

// WebInterfaceClass — Веб-интерфейс
class WebInterfaceClass {
public:
  // begin — Инициализирует модуль
  void begin();
  // loop — Обрабатывает события модуля
  void loop();
  // broadcastStatus — Рассылает состояние клиентам
  void broadcastStatus();
  // requestRestart — Планирует перезагрузку
  void requestRestart(uint32_t delayMs);
};

extern WebInterfaceClass WebInterface;  // WebInterface — Веб-сервер
