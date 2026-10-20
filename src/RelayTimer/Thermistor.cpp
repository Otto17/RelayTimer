/*

  ================================================================
  Copyright (c) 2026 Otto
  License: GPL-3.0-or-later (см. LICENSE)
  ================================================================

  СЧИТЫВАНИЕ И РАСЧЁТ ТЕМПЕРАТУРЫ ТЕРМИСТОРА

*/

#include "Thermistor.h"
#include <Preferences.h>

Thermistor thermistor(thermistorConfig);  // thermistor — Датчик температуры

// Thermistor — Создаёт термистор
Thermistor::Thermistor(const ThermistorConfig& cfg)
  : _cfg(cfg) {}

// loadOffset — Загружает поправку температуры
void Thermistor::loadOffset() {
  Preferences prefs;
  prefs.begin(NVS_NS_TEMP, true);
  _offset = prefs.getFloat("offset", 0.0f);
  prefs.end();
}

// setOffset — Сохраняет поправку температуры
void Thermistor::setOffset(float offsetC) {
  _offset = offsetC;
  Preferences prefs;
  prefs.begin(NVS_NS_TEMP, false);
  prefs.putFloat("offset", offsetC);
  prefs.end();
}

// begin — Инициализирует термистор
void Thermistor::begin() {
  pinMode(_cfg.vccPin, OUTPUT);
  pinMode(_cfg.gndPin, OUTPUT);
  digitalWrite(_cfg.gndPin, LOW);
  digitalWrite(_cfg.vccPin, LOW);
  analogReadResolution(12);
  analogSetPinAttenuation(_cfg.adcPin, ADC_11db);
  loadOffset();
}

// readRawTemperatureC — Измеряет температуру
float Thermistor::readRawTemperatureC() {
  digitalWrite(_cfg.vccPin, HIGH);
  delay(15);

  long sum = 0;
  const int samples = 8;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(_cfg.adcPin);
    delayMicroseconds(200);
  }
  int raw = sum / samples;

  digitalWrite(_cfg.vccPin, LOW);
  raw = constrain(raw, 1, (int)_cfg.adcMax - 1);

  float resistance = _cfg.seriesResistor * raw / (_cfg.adcMax - raw);
  float steinhart = resistance / _cfg.nominalResistance;
  steinhart = log(steinhart);
  steinhart /= _cfg.bCoefficient;
  steinhart += 1.0f / (_cfg.nominalTempC + 273.15f);
  steinhart = 1.0f / steinhart;
  steinhart -= 273.15f;

  return steinhart;
}

// readTemperatureC — Возвращает температуру с поправкой
float Thermistor::readTemperatureC() {
  return readRawTemperatureC() + _offset;
}
