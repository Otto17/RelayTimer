/*

  ================================================================
  Copyright (c) 2026 Otto
  License: GPL-3.0-or-later (см. LICENSE)
  ================================================================

  ОБЪЯВЛЕНИЯ ФУНКЦИЙ ИЗМЕРЕНИЯ ТЕМПЕРАТУРЫ

*/

#pragma once
#include <Arduino.h>
#include "Config.h"

// Thermistor — Измерение температуры
class Thermistor {
public:
  // Thermistor — Создаёт термистор
  explicit Thermistor(const ThermistorConfig& cfg);
  // begin — Инициализирует модуль
  void begin();
  // readTemperatureC — Возвращает температуру с поправкой
  float readTemperatureC();

  // setOffset — Сохраняет поправку температуры
  void setOffset(float offsetC);
  // getOffset — Возвращает поправку температуры
  float getOffset() const {
    return _offset;
  }

private:
  ThermistorConfig _cfg;
  float _offset = 0.0f;

  // loadOffset — Загружает поправку температуры
  void loadOffset();
  // readRawTemperatureC — Измеряет температуру
  float readRawTemperatureC();
};

extern Thermistor thermistor;  // thermistor — Датчик температуры
