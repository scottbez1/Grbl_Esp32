#include "HuanyangSpindle.h"

/*
    HuanyangSpindle.cpp

    This is for a Huanyang VFD based spindle via RS485 Modbus.
    Sorry for the lengthy comments, but finding the details on this
    VFD was a PITA. I am just trying to help the next person.

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

    ==============================================================================

    If a user changes state or RPM level, the command to do that is sent. If
    the command is not responded to a message is sent to serial that there was
    a timeout. If the Grbl is in a critical state, an alarm will be generated and
    the machine stopped.

    If there are no commands to execute, various status items will be polled. If there
    is no response, it will behave as described above. It will stop any running jobs with
    an alarm.

    ===============================================================================

    Protocol Details

    VFD frequencies are in Hz. Multiply by 60 for RPM

    before using spindle, VFD must be setup for RS485 and match your spindle
    PD001   2   RS485 Control of run commands
    PD002   2   RS485 Control of operating frequency
    PD005   400 Maximum frequency Hz (Typical for spindles)
    PD011   120 Min Speed (Recommend Aircooled=120 Water=100)
    PD014   10  Acceleration time (Test to optimize)
    PD015   10  Deceleration time (Test to optimize)
    PD023   1   Reverse run enabled
    PD142   3.7 Max current Amps (0.8kw=3.7 1.5kw=7.0, 2.2kw=??)
    PD143   2   Poles most are 2 (I think this is only used for RPM calc from Hz)
    PD163   1   RS485 Address: 1 (Typical. OK to change...see below)
    PD164   1   RS485 Baud rate: 9600 (Typical. OK to change...see below)
    PD165   3   RS485 Mode: RTU, 8N1

    The official documentation of the RS485 is horrible. I had to piece it together from
    a lot of different sources

    Manuals: https://github.com/RobertOlechowski/Huanyang_VFD/tree/master/Documentations/pdf
    Reference: https://github.com/Smoothieware/Smoothieware/blob/edge/src/modules/tools/spindle/HuanyangSpindleControl.cpp
    Refernece: https://gist.github.com/Bouni/803492ed0aab3f944066
    VFD settings: https://www.hobbytronics.co.za/Content/external/1159/Spindle_Settings.pdf
    Spindle Talker 2 https://github.com/GilchristT/SpindleTalker2/releases
    Python https://github.com/RobertOlechowski/Huanyang_VFD

    =========================================================================

    Commands
    ADDR    CMD     LEN     DATA    CRC
    0x01    0x03    0x01    0x01    0x31 0x88                   Start spindle clockwise
    0x01    0x03    0x01    0x08    0xF1 0x8E                   Stop spindle
    0x01    0x03    0x01    0x11    0x30 0x44                   Start spindle counter-clockwise

    Return values are
    0 = run
    1 = jog
    2 = r/f
    3 = running
    4 = jogging
    5 = r/f
    6 = Braking
    7 = Track start

    ==========================================================================

    Setting RPM
    ADDR    CMD     LEN     DATA        CRC
    0x01    0x05    0x02    0x09 0xC4   0xBF 0x0F               Write Frequency (0x9C4 = 2500 = 25.00HZ)

    Response is same as data sent

    ==========================================================================

    Status registers
    Addr    Read    Len     Reg     DataH   DataL   CRC     CRC
    0x01    0x04    0x03    0x00    0x00    0x00    CRC     CRC     //  Set Frequency * 100 (25Hz = 2500)
    0x01    0x04    0x03    0x01    0x00    0x00    CRC     CRC     //  Ouput Frequency * 100
    0x01    0x04    0x03    0x02    0x00    0x00    CRC     CRC     //  Ouput Amps * 10
    0x01    0x04    0x03    0x03    0x00    0x00    0xF0    0x4E    //  Read RPM (example CRC shown)
    0x01    0x04    0x03    0x0     0x00    0x00    CRC     CRC     //  DC voltage
    0x01    0x04    0x03    0x05    0x00    0x00    CRC     CRC     //  AC voltage
    0x01    0x04    0x03    0x06    0x00    0x00    CRC     CRC     //  Cont
    0x01    0x04    0x03    0x07    0x00    0x00    CRC     CRC     //  VFD Temp
    Message is returned with requested value = (DataH * 16) + DataL (see decimal offset above)

*/

#include <driver/uart.h>

// Larger than any response (includes address, function code, length, data; excludes CRC)
#define HUANYANG_MAX_RESPONSE_LENGTH 16

// Huanyang top level function codes
#define HUANYANG_FUNCTION_CODE_FUNC_READ            0x01
#define HUANYANG_FUNCTION_CODE_FUNC_WRITE           0x02
#define HUANYANG_FUNCTION_CODE_WRITE_CONTROL_DATA   0x03
#define HUANYANG_FUNCTION_CODE_READ_STATUS_VALUE    0x04
#define HUANYANG_FUNCTION_CODE_WRITE_FREQUENCY      0x05
#define HUANYANG_FUNCTION_CODE_RESERVED_1           0x06
#define HUANYANG_FUNCTION_CODE_RESERVED_2           0x07
#define HUANYANG_FUNCTION_CODE_LOOP_TEST            0x08

// Control data (command) bits - sent in function 0x03 Write Control Data
#define HUANYANG_CNTR_BIT_RUN   (1 << 0)
#define HUANYANG_CNTR_BIT_FOR   (1 << 1)
#define HUANYANG_CNTR_BIT_REV   (1 << 2)
#define HUANYANG_CNTR_BIT_STOP  (1 << 3)
#define HUANYANG_CNTR_BIT_RF    (1 << 4)
#define HUANYANG_CNTR_BIT_JOG   (1 << 5)
#define HUANYANG_CNTR_BIT_JOGF  (1 << 6)
#define HUANYANG_CNTR_BIT_JOGR  (1 << 7)


// Control status (response) bits - in response to function 0x03 Write Control Data
#define HUANYANG_CNST_BIT_RUN           (1 << 0)
#define HUANYANG_CNST_BIT_JOG           (1 << 1)
#define HUANYANG_CNST_BIT_REVERSE       (1 << 2)
#define HUANYANG_CNST_BIT_RUNNING       (1 << 3)
#define HUANYANG_CNST_BIT_JOGGING       (1 << 4)
#define HUANYANG_CNST_BIT_RF2           (1 << 5)
#define HUANYANG_CNST_BIT_BRAKING       (1 << 6)
#define HUANYANG_CNST_BIT_TRACK_START   (1 << 7)


// Status value indexes - requested via 0x04 Read Status Value
#define HUANYANG_STATUS_IDX_SET_F   0
#define HUANYANG_STATUS_IDX_OUT_F   1
#define HUANYANG_STATUS_IDX_OUT_A   2
#define HUANYANG_STATUS_IDX_ROTT    3
#define HUANYANG_STATUS_IDX_DCV     4
#define HUANYANG_STATUS_IDX_ACV     5
#define HUANYANG_STATUS_IDX_CONT    6
#define HUANYANG_STATUS_IDX_TMP     7

namespace Spindles {

    void Huanyang::default_modbus_settings(uart_config_t& uart) {
        // sets the uart to 9600 8N1
        VFD::default_modbus_settings(uart);

        // uart.baud_rate = 9600;
        // Baud rate is set in the PD164 setting.
    }

    bool Huanyang::send_control_data(uint8_t control_data, uint8_t* out_control_status) {
        ModbusCommand data;
        uint8_t response[4];

        // NOTE: data length is excluding the CRC16 checksum.
        data.tx_length = 4;
        data.rx_length = 4;

        // data.msg[0] is omitted (modbus address is filled in later)
        data.msg[1] = HUANYANG_FUNCTION_CODE_WRITE_CONTROL_DATA;
        data.msg[2] = 0x01;
        data.msg[3] = control_data;

        if (!send_command(data, response)) {
            return false;
        } else {
            if (response[1] != HUANYANG_FUNCTION_CODE_WRITE_CONTROL_DATA) {
#ifdef VFD_DEBUG_MODE
                grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "Incorrect function in Write Control Data response %d", response[1]);
#endif
                return false;
            } else if (response[2] != 0x01) {
#ifdef VFD_DEBUG_MODE
                grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "Incorrect length in Write Control Data response %d", response[2]);
#endif
                return false;
            }

            *out_control_status = response[3];
            return true;
        }
    }

    bool Huanyang::read_status_register(uint8_t index, uint16_t* out_value) {
        ModbusCommand data;
        uint8_t response[6];

        // NOTE: data length is excluding the CRC16 checksum.
        data.tx_length = 4;
        data.rx_length = 6;

        // data.msg[0] is omitted (modbus address is filled in later)
        data.msg[1] = HUANYANG_FUNCTION_CODE_READ_STATUS_VALUE;
        data.msg[2] = 0x01;
        data.msg[3] = index;

        if (!send_command(data, response)) {
            return false;
        }

        if (response[1] != HUANYANG_FUNCTION_CODE_READ_STATUS_VALUE) {
#ifdef VFD_DEBUG_MODE
            grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "Incorrect function in Read Status response %d", response[1]);
#endif
            return false;
        } else if (response[2] != 0x03) {
#ifdef VFD_DEBUG_MODE
            grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "Incorrect length in Read Status response %d", response[2]);
#endif
            return false;
        } else if (response[3] != index) {
#ifdef VFD_DEBUG_MODE
            grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "Incorrect index in Read Status response (expected %d, got %d)", index, response[3]);
#endif
            return false;
        }

        *out_value = (response[4] << 8) | response[5];
        return true;
    }

    bool Huanyang::send_speed(uint32_t rpm) {
        ModbusCommand data;
        uint8_t response[5];

        // NOTE: data length is excluding the CRC16 checksum.
        data.tx_length = 5;
        data.rx_length = 5;

        // data.msg[0] is omitted (modbus address is filled in later)
        data.msg[1] = HUANYANG_FUNCTION_CODE_WRITE_FREQUENCY;
        data.msg[2] = 2;

        // TODO: read RPM conversion factor from VFD and use that
        uint16_t frequency = (uint16_t)(rpm * 100 / 60);  // send Hz * 10  (Ex:1500 RPM = 25Hz .... Send 2500)

        data.msg[3] = (frequency >> 8) & 0xFF;
        data.msg[4] = (frequency & 0xFF);

        if (!send_command(data, response)) {
            return false;
        }

        if (response[1] != HUANYANG_FUNCTION_CODE_WRITE_FREQUENCY) {
#ifdef VFD_DEBUG_MODE
            grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "Incorrect function in Write Frequency response %d", response[1]);
#endif
            return false;
        } else if (response[2] != 2) {
#ifdef VFD_DEBUG_MODE
            grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "Incorrect length in Write Frequency response %d", response[2]);
#endif
            return false;
        }

        // Frequency should be echo'ed back to us
        bool correct = response[3] == ((frequency >> 8) & 0xFF)
            && response[4] == (frequency & 0xFF);

        if (!correct) {
#ifdef VFD_DEBUG_MODE
            grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "Incorrect frequency in Write Frequency response %d %d", response[3], response[4]);
#endif
        }
        return correct;
    }

    bool Huanyang::send_state_command(SpindleState mode) {
        uint8_t control_data;
        switch (mode) {
            case SpindleState::Cw:
                control_data = HUANYANG_CNTR_BIT_RUN;
                break;
            case SpindleState::Ccw:
                control_data = HUANYANG_CNTR_BIT_RUN | HUANYANG_CNTR_BIT_RF;
                break;
            default:  // SpindleState::Disable
                control_data = HUANYANG_CNTR_BIT_STOP;
                break;
        }

        uint8_t control_status;
        bool sent = send_control_data(control_data, &control_status);
        if (!sent) {
            return false;
        }

        // The control status in this response doesn't appear to reflect the change we just made, so we don't
        // validate any status in the response here (they will be checked as part of the regular health checks)
        return true;
    }

    bool Huanyang::read_status(uint32_t& configured_rpm, uint32_t& actual_rpm, SpindleState& configured_state, SpindleState& actual_state) {
        // Step 1) Read status register for RPM
        uint16_t actual_rpm_raw;
        if (!read_status_register(HUANYANG_STATUS_IDX_ROTT, &actual_rpm_raw)) {
            return false;
        }
        actual_rpm = actual_rpm_raw;

        // Step 2) Read status register for set frequency (can be converted to RPM)
        uint16_t configured_frequency_x100;
        if (!read_status_register(HUANYANG_STATUS_IDX_SET_F, &configured_frequency_x100)) {
            return false;
        }
        // TODO: read RPM conversion factor from VFD
        configured_rpm = configured_frequency_x100 * 60 / 100;


        // Step 3) Read control status (write empty control data)
        // TODO: check that sending without reverse bit doesn't cause issues!
        uint8_t control_status;
        if (!send_control_data(0, &control_status)) {
            return false;
        }

        // Configured state is based off the "run" bit and "r/f" bit
        if (control_status & HUANYANG_CNST_BIT_RUN) {
            configured_state = (control_status & HUANYANG_CNST_BIT_REVERSE) ? SpindleState::Ccw : SpindleState::Cw;
        } else {
            configured_state = SpindleState::Disable;
        }

        // Actual state is based off the "running" bit and "r/f" bit
        if (control_status & HUANYANG_CNST_BIT_RUNNING) {
            actual_state = (control_status & HUANYANG_CNST_BIT_REVERSE) ? SpindleState::Ccw : SpindleState::Cw;
        } else {
            actual_state = SpindleState::Disable;
        }
        return true;
    }

    bool Huanyang::request_configuration(const SpindleState* state, const uint32_t* rpm) {
        bool success = true;
        if (state != nullptr) {
            if (!send_state_command(*state)) {
                success = false;
            }
        }

        if (rpm != nullptr) {
            if (!send_speed(*rpm)) {
                success = false;
            }
        }

        return success;
    }

}
