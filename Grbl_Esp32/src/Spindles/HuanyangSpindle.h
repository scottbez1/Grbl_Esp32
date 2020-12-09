#pragma once

#include "VFDSpindle.h"

/*
    HuanyangSpindle.h

    Part of Grbl_ESP32
    2020 -    Bart Dring
    2020 -  Stefan de Bruijn

    Grbl is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    Grbl is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with Grbl.  If not, see <http://www.gnu.org/licenses/>.

*/

namespace Spindles {
    class Huanyang : public VFD {
    private:
        int reg;
        uint16_t _actual_rpm   = 0;

        SpindleState last_state = SpindleState::Disable;

        // Low level command helpers
        bool send_control_data(uint8_t control_data, uint8_t* out_control_status);
        bool read_status_register(uint8_t index, uint16_t* out_value);
        bool send_speed(uint32_t rpm);

        // Intermediate command helpers
        bool send_state_command(SpindleState state);

    protected:
        void default_modbus_settings(uart_config_t& uart) override;

        // response_parser get_status_ok(ModbusCommand& data) override;
        // response_parser get_current_rpm(ModbusCommand& data) override;
        bool read_status(uint32_t& configured_rpm, uint32_t& actual_rpm, SpindleState& configured_state, SpindleState& actual_state) override;
        bool request_configuration(const SpindleState* state, const uint32_t* rpm) override;
    };
}
