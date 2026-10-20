/*

  ================================================================
  Copyright (c) 2026 Otto
  License: GPL-3.0-or-later (см. LICENSE)
  ================================================================

  ОБЪЯВЛЕНИЯ ФУНКЦИЙ ДЛЯ СЕМИСЕГМЕНТНОГО ДИСПЛЕЕЯ TM1637

*/

#pragma once
#include <Arduino.h>

// TM1637Driver — Драйвер дисплея
class TM1637Driver {
public:
  // TM1637Driver — Инициализирует выводы дисплея
  TM1637Driver(uint8_t clkPin, uint8_t dioPin);

  // begin — Инициализирует модуль
  void begin();
  // setBrightness — Устанавливает яркость дисплея
  void setBrightness(uint8_t level);
  // clear — Очищает дисплей
  void clear();

  // showRawSegments — Отображает сегменты дисплея
  void showRawSegments(uint8_t s0, uint8_t s1, uint8_t s2, uint8_t s3);
  // showTemperature — Отображает температуру
  void showTemperature(float tempC);
  // showNumberPlain — Отображает число без ведущих нулей
  void showNumberPlain(int value);

  // getBrightness — Возвращает яркость
  uint8_t getBrightness() const {
    return _brightness;
  }

  // saveBrightness — Сохраняет яркость
  void saveBrightness(uint8_t level);

private:
  uint8_t _clk;
  uint8_t _dio;
  uint8_t _brightness = 3;

  // start — Начинает передачу TM1637
  void start();
  // stop — Завершает передачу TM1637
  void stop();
  // writeByte — Передаёт байт дисплею
  void writeByte(uint8_t data);
  // bitDelay — Задаёт задержку обмена
  void bitDelay();
};
