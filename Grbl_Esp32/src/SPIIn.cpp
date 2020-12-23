/*
    SPIIn.cpp
    Part of Grbl_ESP32

    Basic GPIO expander using the ESP32 SPI peripheral (input)

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
#include "Config.h"

// This block of #includes is necessary for Report.h
#include "Error.h"
#include "WebUI/Authentication.h"
#include "WebUI/ESPResponse.h"
#include "Probe.h"
#include "System.h"
#include "Report.h"

#include <FreeRTOS.h>
#include <driver/periph_ctrl.h>
#include <driver/spi_master.h>
#include <rom/lldesc.h>
#include <soc/i2s_struct.h>
#include <freertos/queue.h>

#include <stdatomic.h>

#include "Pins.h"
#include "SPIIn.h"


// Reference: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/spi_master.html

static int spi_in_initialized = 0;
static spi_in_init_t spi_init_params;

// Latest data (written by spi task, readable atomically by any other task via spi_in_read)
static atomic_uint_least32_t spi_in_data = ATOMIC_VAR_INIT(0);

// SPI Task state
static spi_device_handle_t spi_rx;
static spi_transaction_t spi_rx_transaction;


static void spi_in_latch(spi_transaction_t *trans) {
    __digitalWrite(spi_init_params.latch_pin, LOW);
    __digitalWrite(spi_init_params.latch_pin, HIGH);
}

//
// SPI reading task
//
static void spiInTask(void* parameter) {

    __pinMode(spi_init_params.latch_pin, OUTPUT);

    esp_err_t ret;

    //Initialize the SPI bus
    spi_bus_config_t bus_config = {
        .mosi_io_num = -1,
        .miso_io_num = spi_init_params.data_pin,
        .sclk_io_num = spi_init_params.clock_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = sizeof(spi_in_data),
    };
    ret=spi_bus_initialize(spi_init_params.spi_host, &bus_config, spi_init_params.dma_channel);
    ESP_ERROR_CHECK(ret);

    spi_device_interface_config_t rx_device_config = {
        .command_bits=0,
        .address_bits=0,
        .dummy_bits=0,
        .mode=2,
        .duty_cycle_pos=0,
        .cs_ena_pretrans=0,
        .cs_ena_posttrans=0,
        .clock_speed_hz=spi_init_params.clock_speed_hz,
        .input_delay_ns=0,
        .spics_io_num=-1,
        .flags = 0,
        .queue_size=1,
        .pre_cb=&spi_in_latch,  // Latch before reading
        .post_cb=NULL,
    };
    ret=spi_bus_add_device(spi_init_params.spi_host, &rx_device_config, &spi_rx);
    ESP_ERROR_CHECK(ret);

    memset(&spi_rx_transaction, 0, sizeof(spi_rx_transaction));
    spi_rx_transaction.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    spi_rx_transaction.length = sizeof(spi_rx_transaction.rx_data) * 8;
    spi_rx_transaction.rxlength = sizeof(spi_rx_transaction.rx_data) * 8;

    uint32_t last_data = 0;
    while (1) {
        // Read data
        ret=spi_device_polling_transmit(spi_rx, &spi_rx_transaction);
        assert(ret==ESP_OK);

        uint32_t data =
            spi_rx_transaction.rx_data[0]
            | (spi_rx_transaction.rx_data[1] << 8)
            | (spi_rx_transaction.rx_data[2] << 16)
            | (spi_rx_transaction.rx_data[3] << 24);

        atomic_store(&spi_in_data, data);

        if (data != last_data) {
            // In lieu of interrupts, push to the main core via the system notify task if any of the control inputs were set to SPII
#if (\
    (defined(CONTROL_SAFETY_DOOR_PIN)   && (CONTROL_SAFETY_DOOR_PIN >= SPI_IN_PIN_BASE) && (CONTROL_SAFETY_DOOR_PIN < I2S_OUT_PIN_BASE)) || \
    (defined(CONTROL_RESET_PIN)         && (CONTROL_RESET_PIN >= SPI_IN_PIN_BASE)       && (CONTROL_RESET_PIN < I2S_OUT_PIN_BASE)) || \
    (defined(CONTROL_FEED_HOLD_PIN)     && (CONTROL_FEED_HOLD_PIN >= SPI_IN_PIN_BASE)   && (CONTROL_FEED_HOLD_PIN < I2S_OUT_PIN_BASE)) || \
    (defined(CONTROL_CYCLE_START_PIN)   && (CONTROL_CYCLE_START_PIN >= SPI_IN_PIN_BASE) && (CONTROL_CYCLE_START_PIN < I2S_OUT_PIN_BASE)) || \
    (defined(MACRO_BUTTON_0_PIN)        && (MACRO_BUTTON_0_PIN >= SPI_IN_PIN_BASE)      && (MACRO_BUTTON_0_PIN < I2S_OUT_PIN_BASE)) || \
    (defined(MACRO_BUTTON_1_PIN)        && (MACRO_BUTTON_1_PIN >= SPI_IN_PIN_BASE)      && (MACRO_BUTTON_1_PIN < I2S_OUT_PIN_BASE)) || \
    (defined(MACRO_BUTTON_2_PIN)        && (MACRO_BUTTON_2_PIN >= SPI_IN_PIN_BASE)      && (MACRO_BUTTON_2_PIN < I2S_OUT_PIN_BASE)) || \
    (defined(MACRO_BUTTON_3_PIN)        && (MACRO_BUTTON_3_PIN >= SPI_IN_PIN_BASE)      && (MACRO_BUTTON_3_PIN < I2S_OUT_PIN_BASE)) \
    )
            if (system_notify_task != nullptr) {
                xTaskNotify(system_notify_task, SYSTEM_NOTIFY_VALUE_CONTROL_CHANGE, eSetBits);
            }
#endif
            last_data = data;
        }
        static UBaseType_t uxHighWaterMark = 0;
#    ifdef DEBUG_TASK_STACK
        reportTaskStackSize(uxHighWaterMark);
#    endif

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

//
// External funtions
//
uint8_t spi_in_read(uint8_t pin) {
    uint32_t port_data = atomic_load(&spi_in_data);
    return (!!(port_data & bit(pin)));
}

//
// Initialize funtion (external function)
//
int spi_in_init(spi_in_init_t& init_param) {
    if (spi_in_initialized) {
        // already initialized
        return -1;
    }

    atomic_store(&spi_in_data, 0);

    spi_init_params = init_param;

    // Create the task that will periodically read the inputs
    xTaskCreatePinnedToCore(spiInTask,
                            "SPIInTask",
                            2048,
                            NULL,
                            1,
                            nullptr,
                            0  // off the main core to avoid delays to more critical real-time tasks
    );

    return 0;
}

/*
  Initialize SPI in by default parameters.

  return -1 ... already initialized
*/
int spi_in_init() {
    spi_in_init_t default_param = {
        .clock_pin      = SPI_IN_CLOCK,
        .data_pin       = SPI_IN_DATA,
        .latch_pin      = SPI_IN_LATCH,
        .clock_speed_hz = SPI_IN_CLOCK_SPEED_HZ,
        .dma_channel    = SPI_IN_DMA_CHANNEL,
        .spi_host       = SPI_IN_HOST,
    };
    return spi_in_init(default_param);
}
