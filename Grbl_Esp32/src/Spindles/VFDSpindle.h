#pragma once

/*
    VFDSpindle.h
    
    Part of Grbl_ESP32
    2020 -	Bart Dring
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
#include "Spindle.h"

#include <driver/uart.h>

namespace Spindles {

    class VFD : public Spindle {

    private:
        static const int VFD_RS485_MAX_MSG_SIZE = 16;  // more than enough for a modbus message

    protected:
        struct ModbusCommand {
            uint8_t tx_length;
            uint8_t rx_length;
            uint8_t msg[VFD_RS485_MAX_MSG_SIZE];
        };

        struct Config {
            SpindleState state;
            uint32_t rpm;
        };

    private:
        static const int MAX_RETRIES            = 3;   // otherwise the spindle is marked 'unresponsive'

        bool get_pins_and_settings();

        uint32_t bound_rpm(uint32_t rpm);

        uint8_t _txd_pin;
        uint8_t _rxd_pin;
        uint8_t _rts_pin;

        Config _current_desired_config = {
            .state = SpindleState::Disable,
            .rpm = 0
        };
        bool     _task_running = false;
        bool     vfd_ok        = false;

        static QueueHandle_t vfd_config_queue;
        static TaskHandle_t  vfd_cmdTaskHandle;
        static void          vfd_cmd_task(void* pvParameters);

        static uint16_t ModRTU_CRC(uint8_t* buf, int msg_len);

    protected:
        /**
         * Send a command. Returns whether the command was sent successfully and response received.
         *
         * If successful, the response data (of length cmd.rx_length, i.e. not including CRC, which is already
         * checked as part of this method) will be placed into response_data.
         *
         * Must be called from the VFD task.
         */
        bool send_command(ModbusCommand& cmd, uint8_t* response_data);

        virtual void default_modbus_settings(uart_config_t& uart);

        /**
         * Invoked by the VFD task for a concrete implementation to check the current state of the VFD.
         *
         * Should return true if the current status was read successfully; this means that ALL out params MUST
         * be set before returning true.
         *
         * Out params MAY be set when returning false, but their values are unused in that case.
         * 
         * Implementations will likely use send_command to send one or more messages to the VFD and parse the
         * responses. If the VFD does not support reading a particular value (say, actual_rpm), it still must be
         * set to a reasonable value before returning!
         * 
         * For example, if the actual_rpm cannot actually be read from the VFD, an implementation may choose to
         * return the most recent rpm value requested by request_configuration.
         */
        virtual bool read_status(uint32_t& configured_rpm, uint32_t& actual_rpm, SpindleState& configured_state, SpindleState& actual_state) { return false; };


        /**
         * Invoked by the VFD task to request a configuration from the VFD.
         *
         * Parameters will be NULL if the requested value is unchanged; this allows implementations flexibility
         * to skip some commands if not needed, which will improve the read_status polling rate.
         *
         * Should return true if all non-null configuration parameters were sent and acknowledged successfully.
         * If the implementation requires sending multiple commands to the VFD, any of those commands failed,
         * it should return false.
         *
         * Optionally, if the VFD protocol supports negative acknowledgements (e.g. the command was received but
         * a value was out of range), the implementation should return false upon such negative acknowledgement.
         * If the VFD protocol doesn't support negative acknowledgements, it is OK to return true when the command
         * is acknowledged, even if it is still unknown whether it was *accepted*. The VFD task checks whether the
         * configuration was accepted or not as part of regular read_status polling, so there is no need to
         * duplicate that functionality here if the protocol does not readily provide it.
         */
        virtual bool request_configuration(const SpindleState* state, const uint32_t* rpm) { return false; };

    public:
        VFD()           = default;
        VFD(const VFD&) = delete;
        VFD(VFD&&)      = delete;
        VFD& operator=(const VFD&) = delete;
        VFD& operator=(VFD&&) = delete;

        // TODO FIXME: Have to make these public because of the parsers.
        // Should hide them and use a member function.
        volatile uint32_t _min_rpm;
        volatile uint32_t _max_rpm;

        void         init();
        void         config_message();
        void         set_state(SpindleState state, uint32_t rpm);
        SpindleState get_state();
        uint32_t     set_rpm(uint32_t rpm);
        void         stop();

        virtual ~VFD() {}
    };
}
