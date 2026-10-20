/*

  ================================================================
  Copyright (c) 2026 Otto
  License: GPL-3.0-or-later (см. LICENSE)
  ================================================================

  НАСТРОЙКИ ПРОЕКТА, GPIO-ПИНЫ И КОНСТАНТЫ

*/

#pragma once
#include <Arduino.h>

// GPIO пины
#define PIN_THERM_VCC 1     // Питание термистора
#define PIN_THERM_GND 2     // Земля термистора
#define PIN_THERM_ADC 4     // Вход АЦП
#define PIN_RELAY 14        // Выход реле
#define PIN_TOUCH_RELAY 7   // Сенсор
#define PIN_RESET_BUTTON 0  // Кнопки BOOT на плате
#define PIN_STATUS_LED 15   // LED состояния на плате
#define PIN_TM1637_CLK 12   // Тактирования дисплея (CLK)
#define PIN_TM1637_DIO 13   // Данные дисплея (DIO)

#define FIRMWARE_VERSION "19.10.26"  // Версия прошивки

#define RESET_HOLD_MS 8000UL     // Время удержания кнопки для сброса настроек
#define BUTTON_DEBOUNCE_MS 50UL  // Время подавления дребезга кнопки сброса настроек

#define LED_BLINK_FAST_MS 100UL  // Интервал быстрого мигания LED на плате

#define AP_SSID_DEFAULT "RelayTimer"  // Имя точки доступа (AP)
#define AP_PASS_DEFAULT "123456789"   // Пароль точки доступа (AP)
#define AP_CHANNEL 6                  // Канал точки доступа (AP)
#define AP_MAX_CLIENTS 4              // Максимальное число подключений (AP)

#define THERM_READ_INTERVAL_MS 5000UL  // Интервал измерения температуры
#define IP_DISPLAY_STEP_MS 1500UL      // Время показа одного октета IP

// NVS
#define NVS_NS_DISPLAY "displaycfg"  // Раздел настроек дисплея
#define NVS_NS_WIFI "wificfg"        // Раздел настроек WiFi
#define NVS_NS_NTP "ntpcfg"          // Раздел настроек NTP
#define NVS_NS_RELAY "relaycfg"      // Раздел настроек реле
#define NVS_NS_TEMP "tempcfg"        // Раздел настроек температуры

// ThermistorConfig — Параметры термистора
struct ThermistorConfig {
  uint8_t vccPin;
  uint8_t gndPin;
  uint8_t adcPin;
  float seriesResistor;
  float nominalResistance;
  float nominalTempC;
  float bCoefficient;
  float adcMax;
};

// thermistorConfig — Настройка делителя напряжения
static const ThermistorConfig thermistorConfig = {
  PIN_THERM_VCC,  // Вывод питания делителя
  PIN_THERM_GND,  // Вывод земли делителя
  PIN_THERM_ADC,  // Вывод измерения напряжения
  9980.0f,        // Сопротивление постоянного резистора, Ом
  10000.0f,       // Сопротивление термистора при +25 °C, Ом
  25.0f,          // Опорная температура термистора, °C
  3950.0f,        // Бета-коэффициент термистора, К
  4095.0f         // Максимальное значение АЦП
};

extern float g_currentTemperature;  // g_currentTemperature — Текущая температура
