#include "Grbl.h"
#include "I2SOut.h"
#include "SPIIn.h"

String pinName(uint8_t pin) {
    if (pin == UNDEFINED_PIN) {
        return "None";
    }
    if (pin >= I2S_OUT_PIN_BASE) {
        return String("I2SO(") + (pin - I2S_OUT_PIN_BASE) + ")";
    } else if (pin >= SPI_IN_PIN_BASE) {
        return String("SPI(") + pin + ")";
    } else {
        return String("GPIO(") + pin + ")";
    }
}

// Even if USE_I2S_OUT is not defined, it is necessary to
// override the following functions, instead of allowing
// the weak aliases in the library to apply, because of
// the UNDEFINED_PIN check.  That UNDEFINED_PIN behavior
// cleans up other code by eliminating ifdefs and checks.
void IRAM_ATTR digitalWrite(uint8_t pin, uint8_t val) {
    if (pin == UNDEFINED_PIN) {
        return;
    }
#ifdef USE_I2S_OUT
    if (pin >= I2S_OUT_PIN_BASE) {
        i2s_out_write(pin - I2S_OUT_PIN_BASE, val);
        return;
    }
#endif
#ifdef USE_SPI_IN
    if (pin >= SPI_IN_PIN_BASE) {
        // TODO: Implement this?
        return;
    }
#endif
    __digitalWrite(pin, val);
}

void IRAM_ATTR pinMode(uint8_t pin, uint8_t mode) {
    if (pin == UNDEFINED_PIN) {
        return;
    }
#ifdef USE_I2S_OUT
    if (pin >= I2S_OUT_PIN_BASE) {
        // I2S out pins cannot be configured, hence there
        // is nothing to do here for them.
        return;
    }
#endif
#ifdef USE_SPI_IN
    if (pin >= SPI_IN_PIN_BASE) {
        // N/A
        return;
    }
#endif
    __pinMode(pin, mode);
}

int IRAM_ATTR digitalRead(uint8_t pin) {
    if (pin == UNDEFINED_PIN) {
        return 0;
    }
#ifdef USE_I2S_OUT
    if (pin >= I2S_OUT_PIN_BASE) {
        return i2s_out_read(pin - I2S_OUT_PIN_BASE);
    }
#endif
#ifdef USE_SPI_IN
    if (pin >= SPI_IN_PIN_BASE) {
        return spi_in_read(pin - SPI_IN_PIN_BASE);
    }
#endif
    return __digitalRead(pin);
}
