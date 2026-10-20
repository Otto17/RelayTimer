/*

  ================================================================
  Copyright (c) 2026 Otto
  License: GPL-3.0-or-later (см. LICENSE)
  ================================================================

  УПРАВЛЕНИЕ СЕМИСЕГМЕНТНЫМ ДИСПЛЕЕМ TM1637

*/

#include <Preferences.h>
#include "Config.h"
#include "Display.h"

// Сегменты цифр от 0 до 9
static const uint8_t DIGIT_SEG[10] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66,
  0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

static const uint8_t SEG_C = 0x39;      // Сегменты буквы 'C'
static const uint8_t SEG_MINUS = 0x40;  // Сегмент знака минус
static const uint8_t SEG_BLANK = 0x00;  // Пустой разряд
static const uint8_t SEG_DOT = 0x80;    // Сегмент точки

// TM1637Driver — Инициализирует выводы дисплея
TM1637Driver::TM1637Driver(uint8_t clkPin, uint8_t dioPin)
  : _clk(clkPin), _dio(dioPin) {}

// bitDelay — Задаёт задержку обмена
void TM1637Driver::bitDelay() {
  delayMicroseconds(5);
}

// begin — Инициализирует дисплей
void TM1637Driver::begin() {
  pinMode(_clk, OUTPUT);
  pinMode(_dio, OUTPUT);
  digitalWrite(_clk, HIGH);
  digitalWrite(_dio, HIGH);

  Preferences prefs;

  if (prefs.begin(NVS_NS_DISPLAY, true)) {
    _brightness = prefs.getUChar("brightness", 3);
    prefs.end();
  }

  _brightness = constrain(_brightness, 0, 3);
  setBrightness(_brightness);
}

// start — Начинает передачу TM1637
void TM1637Driver::start() {
  digitalWrite(_clk, HIGH);
  digitalWrite(_dio, HIGH);
  bitDelay();
  digitalWrite(_dio, LOW);
  bitDelay();
  digitalWrite(_clk, LOW);
  bitDelay();
}

// stop — Завершает передачу TM1637
void TM1637Driver::stop() {
  digitalWrite(_clk, LOW);
  bitDelay();
  digitalWrite(_dio, LOW);
  bitDelay();
  digitalWrite(_clk, HIGH);
  bitDelay();
  digitalWrite(_dio, HIGH);
  bitDelay();
}

// writeByte — Передаёт байт дисплею
void TM1637Driver::writeByte(uint8_t data) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(_clk, LOW);
    digitalWrite(_dio, (data & 0x01) ? HIGH : LOW);
    bitDelay();
    data >>= 1;
    digitalWrite(_clk, HIGH);
    bitDelay();
  }

  digitalWrite(_clk, LOW);
  pinMode(_dio, INPUT);
  bitDelay();
  digitalWrite(_clk, HIGH);
  bitDelay();
  digitalWrite(_clk, LOW);
  pinMode(_dio, OUTPUT);
  bitDelay();
}

// tm1637Control — Кодирует уровень яркости
static uint8_t tm1637Control(uint8_t level) {
  static const uint8_t pwm[4] = { 0, 1, 3, 7 };

  level = constrain(level, 0, 3);
  return level == 0 ? 0x80 : (uint8_t)(0x88 | pwm[level]);
}

// setBrightness — Устанавливает яркость дисплея
void TM1637Driver::setBrightness(uint8_t level) {
  _brightness = constrain(level, 0, 3);

  start();
  writeByte(tm1637Control(_brightness));
  stop();
}

// showRawSegments — Отображает сегменты дисплея
void TM1637Driver::showRawSegments(uint8_t s0, uint8_t s1, uint8_t s2, uint8_t s3) {
  start();
  writeByte(0x40);
  stop();

  start();
  writeByte(0xC0);
  writeByte(s0);
  writeByte(s1);
  writeByte(s2);
  writeByte(s3);
  stop();

  start();
  writeByte(tm1637Control(_brightness));
  stop();
}

// clear — Очищает дисплей
void TM1637Driver::clear() {
  showRawSegments(SEG_BLANK, SEG_BLANK, SEG_BLANK, SEG_BLANK);
}

// showTemperature — Отображает температуру
void TM1637Driver::showTemperature(float tempC) {
  bool negative = tempC < 0;
  float absTemp = fabs(tempC);
  if (absTemp > 99.9f) absTemp = 99.9f;

  int tens = ((int)(absTemp / 10)) % 10;
  int ones = ((int)absTemp) % 10;
  int tenths = ((int)round((absTemp - (int)absTemp) * 10)) % 10;

  uint8_t seg0, seg1, seg2, seg3;

  if (negative) {
    seg0 = SEG_MINUS;
  } else if (tens == 0) {
    seg0 = SEG_BLANK;
  } else {
    seg0 = DIGIT_SEG[tens];
  }

  seg1 = DIGIT_SEG[ones] | SEG_DOT;
  seg2 = DIGIT_SEG[tenths];
  seg3 = SEG_C;

  showRawSegments(seg0, seg1, seg2, seg3);
}

// showNumberPlain — Отображает число без ведущих нулей
void TM1637Driver::showNumberPlain(int value) {
  if (value < 0) value = 0;
  if (value > 9999) value = 9999;

  int digits[4];
  int v = value;
  digits[3] = v % 10;
  v /= 10;
  digits[2] = v % 10;
  v /= 10;
  digits[1] = v % 10;
  v /= 10;
  digits[0] = v % 10;

  uint8_t segs[4];
  bool leadingZero = true;

  for (int i = 0; i < 4; i++) {
    if (i == 3) {
      segs[i] = DIGIT_SEG[digits[i]];
    } else if (digits[i] != 0 || !leadingZero) {
      segs[i] = DIGIT_SEG[digits[i]];
      leadingZero = false;
    } else {
      segs[i] = SEG_BLANK;
    }
  }

  showRawSegments(segs[0], segs[1], segs[2], segs[3]);
}

// saveBrightness — Сохраняет яркость
void TM1637Driver::saveBrightness(uint8_t level) {
  level = constrain(level, 0, 3);

  setBrightness(level);

  Preferences prefs;

  if (prefs.begin(NVS_NS_DISPLAY, false)) {
    prefs.putUChar("brightness", level);
    prefs.end();
  }
}
