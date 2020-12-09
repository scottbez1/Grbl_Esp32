/*
    VFDSpindle.cpp

    This is for a VFD based spindles via RS485 Modbus. The details of the 
    VFD protocol heavily depend on the VFD in question here. We have some 
    implementations, but if yours is not here, the place to start is the 
    manual. This VFD class implements the modbus functionality.

    Part of Grbl_ESP32
    2020 -  Bart Dring
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

                         WARNING!!!!
    VFDs are very dangerous. They have high voltages and are very powerful
    Remove power before changing bits.

    TODO:
      - We can report spindle_state and rpm better with VFD's that support 
        either mode, register RPM or actual RPM.
      - Destructor should break down the task.
      - Move min/max RPM to protected members.

*/
#include "VFDSpindle.h"

const uart_port_t VFD_RS485_UART_PORT  = UART_NUM_2;  // hard coded for this port right now
const int         VFD_RS485_BUF_SIZE   = 127;
const int         VFD_RS485_MODBUS_QUIET_TICKS = pdMS_TO_TICKS(50);  // minimum time between modbus messages
const int         RESPONSE_WAIT_TICKS  = 50;   // how long to wait for a response
const int         VFD_RS485_POLL_RATE  = pdMS_TO_TICKS(100); //200;  // in milliseconds between commands

const int         VFD_RS485_CONFIGURED_RPM_TOLERANCE = 1; // Acceptable error due to potential rounding - TODO: figure out right value

const SpindleState VFD_RS485_DISABLED_STATE = SpindleState::Disable;
const uint32_t     VFD_RS485_DISABLED_RPM = 0;

// OK to change these
// #define them in your machine definition file if you want different values
#ifndef VFD_RS485_ADDR
#    define VFD_RS485_ADDR 0x01
#endif

namespace Spindles {
    QueueHandle_t VFD::vfd_config_queue     = nullptr;
    TaskHandle_t  VFD::vfd_cmdTaskHandle = nullptr;

    // The communications task
    void VFD::vfd_cmd_task(void* pvParameters) {
        static bool unresponsive = false;  // to pop off a message once each time it becomes unresponsive
        static int  pollidx      = 0;

        VFD*          instance = static_cast<VFD*>(pvParameters);
        uint8_t       rx_message[VFD_RS485_MAX_MSG_SIZE];

        bool healthy = true;

        // The desired configuration, as requested via the VFDSpindle public API. This may NOT be what
        // we send to the VFD, because internal logic may take precedence (e.g. unhealthy
        // communications may cause us to request a stop even if we've been asked to move at a nonzero rpm)
        Config desired_config = {
            .state = SpindleState::Disable,
            .rpm = 0,
        };

        const SpindleState* state_request = nullptr;
        const uint32_t* rpm_request = nullptr;

        uint32_t configured_rpm;
        uint32_t actual_rpm;
        SpindleState configured_state;
        SpindleState actual_state;

        while (true) {
            // Check queue for changed config. There is at max 1 item in the queue, as all producers use xQueueOverwrite
            xQueueReceive(vfd_config_queue, &desired_config, 0);

            if (!healthy || sys.state == State::Alarm) {
                // Disable the spindle if comms are unhealthy or system is in alarm. Note that even if the system is in
                // alarm, we will continue to attempt communicating with the spindle to turn it off, for safety.
                state_request = &VFD_RS485_DISABLED_STATE;
                rpm_request = &VFD_RS485_DISABLED_RPM;
            } else {
                if (desired_config.state != configured_state) {
                    state_request = &desired_config.state;
                } else {
                    state_request = nullptr;
                }
                if (desired_config.rpm != configured_rpm) {
                    rpm_request = &desired_config.rpm;
                } else {
                    rpm_request = nullptr;
                }
            }

            if (state_request != nullptr || rpm_request != nullptr) {
                bool config_sent = instance->request_configuration(state_request, rpm_request);
#ifdef VFD_DEBUG_MODE
                if (!config_sent) {
                    char state_buffer[10] = "unchanged";
                    char rpm_buffer[11] = "unchanged";
                    if (state_request != nullptr) {
                        snprintf(state_buffer, sizeof(state_buffer), "%d", (int) *state_request);
                    }
                    if (rpm_request != nullptr) {
                        snprintf(rpm_buffer, sizeof(rpm_buffer), "%d", *rpm_buffer);
                    }
                    grbl_msg_sendf(
                        CLIENT_SERIAL,
                        MsgLevel::Info,
                        "RS485 failed to send config (state=%s, rpm=%s)",
                        state_buffer,
                        rpm_buffer
                        );
                }
#endif
            }

            // Read status - determines healthy or not
            bool new_healthy = instance->read_status(configured_rpm, actual_rpm, configured_state, actual_state);
            if (new_healthy && !healthy) {
                grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "RS485 became healthy");
            } else if (!new_healthy && healthy) {
                grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "RS485 is NOT healthy");
            }
            healthy = new_healthy;

// #ifdef VFD_DEBUG_MODE
//             if (healthy) {
//                 grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "RS485 status: rpm=%d, arpm=%d, st=%d, ast=%d", configured_rpm, actual_rpm, configured_state, actual_state);
//             }
// #endif

            // Raise an alarm if current state is not as we requested
            if (sys.state != State::Alarm) {
                if (healthy) {
                    if (desired_config.state != configured_state) {
                        grbl_msg_sendf(
                            CLIENT_SERIAL, 
                            MsgLevel::Info, 
                            "RS485 spindle reported incorrect state configuration (expected %d but got %d)",
                            desired_config.state,
                            configured_state);
                        mc_reset();
                        sys_rt_exec_alarm = ExecAlarm::SpindleControl;
                    } else if (desired_config.state != SpindleState::Disable && abs(desired_config.rpm - configured_rpm) > VFD_RS485_CONFIGURED_RPM_TOLERANCE) {
                        grbl_msg_sendf(
                            CLIENT_SERIAL, 
                            MsgLevel::Info, 
                            "RS485 spindle reported incorrect rpm configuration (expected %d but got %d)",
                            desired_config.rpm,
                            configured_rpm);
                        mc_reset();
                        sys_rt_exec_alarm = ExecAlarm::SpindleControl;
                    }
                } else {
                    if (desired_config.rpm > 0 || desired_config.state != SpindleState::Disable) {
                        grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "RS485 unhealthy but action requested!");
                        mc_reset();
                        sys_rt_exec_alarm = ExecAlarm::SpindleControl;
                    }
                }
            }

            vTaskDelay(VFD_RS485_POLL_RATE);
        }
    }

    bool VFD::send_command(ModbusCommand& cmd, uint8_t* response_data) {
        // Hard assertion that this is only called from the VFD task (anything else indicates *programmer*
        // error in a VFDSpindle implementation, so should fail fast)
        TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
        assert(current_task == vfd_cmdTaskHandle);

        // Set the address
        cmd.msg[0] = VFD_RS485_ADDR;

        // Add the CRC16 checksum:
        auto crc16 = ModRTU_CRC(cmd.msg, cmd.tx_length);
        cmd.tx_length += 2;
        cmd.rx_length += 2;
        cmd.msg[cmd.tx_length - 1] = (crc16 & 0xFF00) >> 8;
        cmd.msg[cmd.tx_length - 2] = (crc16 & 0xFF);

#ifdef VFD_DEBUG_MODE
        // report_hex_msg(cmd.msg, "RS485 Tx: ", cmd.tx_length);
#endif

        // Flush the UART and write the data:
        uart_flush(VFD_RS485_UART_PORT);
        uart_write_bytes(VFD_RS485_UART_PORT, reinterpret_cast<const char*>(cmd.msg), cmd.tx_length);

        // Read the response
        uint8_t rx_message[VFD_RS485_MAX_MSG_SIZE];
        uint16_t read_length = uart_read_bytes(VFD_RS485_UART_PORT, rx_message, cmd.rx_length, RESPONSE_WAIT_TICKS);


#ifdef VFD_DEBUG_MODE
        if (read_length >= 0) {
            // report_hex_msg(rx_message, "RS485 Rx: ", read_length);
        } else {
            grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "RS485 Rx error");
            return false;
        }
#endif

        // Wait the minimum RTU quiet interval. This may delay longer than strictly necessary if other
        // processing takes time or delays, but it's simple.
        vTaskDelay(VFD_RS485_MODBUS_QUIET_TICKS);

        // Generate crc16 for the response:
        auto crc16response = ModRTU_CRC(rx_message, cmd.rx_length - 2);

        if (read_length != cmd.rx_length) {
            if (read_length > 0) {
                grbl_msg_sendf(CLIENT_SERIAL,
                                MsgLevel::Info,
                                "RS485 received message of unexpected length; expected %d, got %d",
                                int(cmd.rx_length),
                                int(read_length));
            }
            return false;
        } else if (rx_message[read_length - 1] != (crc16response & 0xFF00) >> 8 ||  // check CRC byte 1
                rx_message[read_length - 2] != (crc16response & 0xFF)) {            // check CRC byte 0
            grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "RS485 CRC check failed %d", crc16response);
            return false;
        } else if (rx_message[0] != VFD_RS485_ADDR) {
            grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "RS485 received message from other modbus device %d");
            return false;
        }

        // Valid response - copy to the output param, without the CRC (not relevant beyond this layer since we already validated it)
        memcpy(response_data, rx_message, read_length - 2);

        // TODO figure out where to put this
        // static UBaseType_t uxHighWaterMark = 0;
        // reportTaskStackSize(uxHighWaterMark);

        return true;
    }


    // ================== Class methods ==================================
    void VFD::default_modbus_settings(uart_config_t& uart) {
        // Default is 9600 8N1, which is sane for most VFD's:
        uart.baud_rate = 9600;
        uart.data_bits = UART_DATA_8_BITS;
        uart.parity    = UART_PARITY_DISABLE;
        uart.stop_bits = UART_STOP_BITS_1;
    }

    void VFD::init() {
        vfd_ok = false;  // initialize

        grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "Initializing RS485 VFD spindle");

        // fail if required items are not defined
        if (!get_pins_and_settings()) {
            vfd_ok = false;
            grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "RS485 VFD spindle errors");
            return;
        }

        // this allows us to init() again later.
        // If you change certain settings, init() gets called agian
        uart_driver_delete(VFD_RS485_UART_PORT);

        uart_config_t uart_config;
        default_modbus_settings(uart_config);

        // Overwrite with user defined defines:
#ifdef VFD_RS485_BAUD_RATE
        uart_config.baud_rate = VFD_RS485_BAUD_RATE;
#endif
#ifdef VFD_RS485_PARITY
        uart_config.parity = VFD_RS485_PARITY;
#endif

        uart_config.flow_ctrl           = UART_HW_FLOWCTRL_DISABLE;
        uart_config.rx_flow_ctrl_thresh = 122;

        if (uart_param_config(VFD_RS485_UART_PORT, &uart_config) != ESP_OK) {
            grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "RS485 VFD uart parameters failed");
            return;
        }

        if (uart_set_pin(VFD_RS485_UART_PORT, _txd_pin, _rxd_pin, _rts_pin, UART_PIN_NO_CHANGE) != ESP_OK) {
            grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "RS485 VFD uart pin config failed");
            return;
        }

        if (uart_driver_install(VFD_RS485_UART_PORT, VFD_RS485_BUF_SIZE * 2, 0, 0, NULL, 0) != ESP_OK) {
            grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "RS485 VFD uart driver install failed");
            return;
        }

        if (uart_set_mode(VFD_RS485_UART_PORT, UART_MODE_RS485_HALF_DUPLEX) != ESP_OK) {
            grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "RS485 VFD uart set half duplex failed");
            return;
        }

        // Initialization is complete, so now it's okay to run the queue task:
        if (!_task_running) {  // init can happen many times, we only want to start one task
            vfd_config_queue = xQueueCreate(1, sizeof(Config));
            xTaskCreatePinnedToCore(vfd_cmd_task,         // task
                                    "vfd_cmdTaskHandle",  // name for task
                                    3000,                 // size of task stack
                                    this,                 // parameters
                                    1,                    // priority
                                    &vfd_cmdTaskHandle,
                                    0  // core
            );
            _task_running = true;
        }

        is_reversable = true;  // these VFDs are always reversable
        use_delays    = true;
        vfd_ok        = true;

        // Initially we initialize this to 0; over time, we might poll better information from the VFD.
        // _current_rpm   = 0;
        _current_state = SpindleState::Disable;

        config_message();
    }

    // Checks for all the required pin definitions
    // It returns a message for each missing pin
    // Returns true if all pins are defined.
    bool VFD::get_pins_and_settings() {
        bool pins_settings_ok = true;

#ifdef VFD_RS485_TXD_PIN
        _txd_pin = VFD_RS485_TXD_PIN;
#else
        grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "Undefined VFD_RS485_TXD_PIN");
        pins_settings_ok = false;
#endif

#ifdef VFD_RS485_RXD_PIN
        _rxd_pin = VFD_RS485_RXD_PIN;
#else
        grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "Undefined VFD_RS485_RXD_PIN");
        pins_settings_ok = false;
#endif

#ifdef VFD_RS485_RTS_PIN
        _rts_pin = VFD_RS485_RTS_PIN;
#else
        grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "Undefined VFD_RS485_RTS_PIN");
        pins_settings_ok = false;
#endif

        if (laser_mode->get()) {
            grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "VFD spindle disabled in laser mode. Set $GCode/LaserMode=Off and restart");
            pins_settings_ok = false;
        }

        _min_rpm = rpm_min->get();
        _max_rpm = rpm_max->get();

        return pins_settings_ok;
    }

    uint32_t VFD::bound_rpm(uint32_t rpm) {
        // apply override
        rpm = rpm * sys.spindle_speed_ovr / 100;  // Scale by spindle speed override value (uint8_t percent)

        // apply limits
        if ((_min_rpm >= _max_rpm) || (rpm >= _max_rpm)) {
            rpm = _max_rpm;
        } else if (rpm != 0 && rpm <= _min_rpm) {
            rpm = _min_rpm;
        }
        return rpm;
    }

    void VFD::config_message() {
        grbl_msg_sendf(CLIENT_SERIAL,
                       MsgLevel::Info,
                       "VFD RS485  Tx:%s Rx:%s RTS:%s",
                       pinName(_txd_pin).c_str(),
                       pinName(_rxd_pin).c_str(),
                       pinName(_rts_pin).c_str());
    }

    void VFD::set_state(SpindleState state, uint32_t rpm) {
        if (sys.abort) {
            return;  // Block during abort.
        }
        if (!vfd_ok) {
            return;
        }

        rpm = bound_rpm(rpm);
        sys.spindle_speed = rpm;
        
        _current_desired_config.state = state;
        _current_desired_config.rpm = rpm;
        xQueueOverwrite(vfd_config_queue, &_current_desired_config);

        _current_state = state;  // store locally to track changes. Must be done before mc_dwell to avoid infinite recursion during suspend.

        if (state == SpindleState::Disable) {
            sys.spindle_speed = 0;
            if (_current_state != state) {
                mc_dwell(spindle_delay_spindown->get());
            }
        } else {
            if (_current_state != state) {
                mc_dwell(spindle_delay_spinup->get());
            }
        }

        sys.report_ovr_counter = 0;  // Set to report change immediately

        return;
    }

    uint32_t VFD::set_rpm(uint32_t rpm) {
        if (!vfd_ok) {
            return 0;
        }

#ifdef VFD_DEBUG_MODE
        grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "Setting spindle speed to %d rpm (%d, %d)", int(rpm), int(_min_rpm), int(_max_rpm));
#endif

        rpm = bound_rpm(rpm);
        sys.spindle_speed = rpm;

        // TODO add the speed modifiers override, linearization, etc.

        _current_desired_config.rpm = rpm;
        xQueueOverwrite(vfd_config_queue, &_current_desired_config);

        return rpm;
    }

    void VFD::stop() { set_state(SpindleState::Disable, 0); }

    // state is cached rather than read right now to prevent delays
    SpindleState VFD::get_state() { return _current_state; }

    // Calculate the CRC on all of the byte except the last 2
    // It then added the CRC to those last 2 bytes
    // full_msg_len This is the length of the message including the 2 crc bytes
    // Source: https://ctlsys.com/support/how_to_compute_the_modbus_rtu_message_crc/
    uint16_t VFD::ModRTU_CRC(uint8_t* buf, int msg_len) {
        uint16_t crc = 0xFFFF;
        for (int pos = 0; pos < msg_len; pos++) {
            crc ^= uint16_t(buf[pos]);  // XOR byte into least sig. byte of crc.

            for (int i = 8; i != 0; i--) {  // Loop over each bit
                if ((crc & 0x0001) != 0) {  // If the LSB is set
                    crc >>= 1;              // Shift right and XOR 0xA001
                    crc ^= 0xA001;
                } else {        // Else LSB is not set
                    crc >>= 1;  // Just shift right
                }
            }
        }

        return crc;
    }
}
