#pragma once

/*
    SPIIn.h
    Part of Grbl_ESP32
    Header for basic GPIO expander using the ESP32 SPI
    2020    - Scott Bezek
    Grbl_ESP32 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    Grbl is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with Grbl_ESP32.  If not, see <http://www.gnu.org/licenses/>.
*/

// It should be included at the outset to know the machine configuration.
#include "Config.h"

#include <stdint.h>
#include <driver/spi_master.h>

#define SPII(n) (SPI_IN_PIN_BASE + n)

typedef struct {
    uint8_t clock_pin;
    uint8_t data_pin;
    uint8_t latch_pin;
    int clock_speed_hz;
    int dma_channel;
    spi_host_device_t spi_host;
} spi_in_init_t;

/*
  Initialize SPI in by parameters.
  return -1 ... already initialized
*/
int spi_in_init(spi_in_init_t& init_param);

/*
  Initialize SPI in by default parameters.
    spi_in_init_t default_param = {
        .clock_pin = SPI_IN_CLOCK,
        .data_pin = SPI_IN_DATA,
        .latch_pin = SPI_IN_LATCH,
    };
  return -1 ... already initialized
*/
int spi_in_init();

/*
  Read a bit state from the internal pin state var.
  This may be a stale value depending on whether the background task has
  read the device recently or not.
  pin: expanded pin No. (0..31)
*/
uint8_t spi_in_read(uint8_t pin);
