#pragma once

#include <Arduino.h>

#define UNDEFINED_PIN    255  // Can be used to show a pin has no i/O assigned
#define I2S_OUT_PIN_BASE 128
#define SPI_IN_PIN_BASE  64

// Code assumes SPI always comes before I2S
static_assert(I2S_OUT_PIN_BASE >= SPI_IN_PIN_BASE, "I2S base pin must be larger than SPI base pin");

extern "C" int  __digitalRead(uint8_t pin);
extern "C" void __pinMode(uint8_t pin, uint8_t mode);
extern "C" void __digitalWrite(uint8_t pin, uint8_t val);

String pinName(uint8_t pin);
