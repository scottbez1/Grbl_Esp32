#pragma once
// clang-format off

/*
    workbee.h
    Part of Grbl_ESP32

    2018    - Bart Dring
    2020    - Mitch Bradley
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

#define MACHINE_NAME            "Workbee"

// This cannot use homing because there are no switches
#ifdef DEFAULT_HOMING_CYCLE_0
    #undef DEFAULT_HOMING_CYCLE_0
#endif

#ifdef DEFAULT_HOMING_CYCLE_1
    #undef DEFAULT_HOMING_CYCLE_1
#endif

// #define SPINDLE_TYPE    SpindleType::NONE


#define X_STEP_PIN              GPIO_NUM_16
#define X_DIRECTION_PIN         GPIO_NUM_17
#define Y_STEP_PIN              GPIO_NUM_18
#define Y_DIRECTION_PIN         GPIO_NUM_19
#define Y2_STEP_PIN             GPIO_NUM_21
#define Y2_DIRECTION_PIN        GPIO_NUM_22
#define Z_STEP_PIN              GPIO_NUM_23
#define Z_DIRECTION_PIN         GPIO_NUM_25

#define STEPPERS_DISABLE_PIN    GPIO_NUM_13

#define SPINDLE_TYPE            SpindleType::HUANYANG
#define VFD_RS485_TXD_PIN       GPIO_NUM_26
#define VFD_RS485_RXD_PIN       GPIO_NUM_27
#define VFD_RS485_RTS_PIN       GPIO_NUM_32
#define VFD_RS485_ADDR          1

// To debug RS485 spindle comms:
// #define VFD_DEBUG_MODE


#define X_LIMIT_PIN             GPIO_NUM_34
#define Y_LIMIT_PIN             GPIO_NUM_35
#define Z_LIMIT_PIN             GPIO_NUM_36

#define PROBE_PIN               GPIO_NUM_33


// The default value in config.h is wrong for this controller
#ifdef INVERT_CONTROL_PIN_MASK
    #undef INVERT_CONTROL_PIN_MASK
#endif

#define INVERT_CONTROL_PIN_MASK B1110

#define CONTROL_SAFETY_DOOR_PIN GPIO_NUM_39
// #define CONTROL_RESET_PIN       GPIO_NUM_5
// #define CONTROL_FEED_HOLD_PIN   GPIO_NUM_2
// #define CONTROL_CYCLE_START_PIN GPIO_NUM_0
