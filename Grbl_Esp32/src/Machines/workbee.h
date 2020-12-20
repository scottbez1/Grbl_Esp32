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


// ***** IO *****
#define X_STEP_PIN              GPIO_NUM_16
#define X_DIRECTION_PIN         GPIO_NUM_17
#define Y_STEP_PIN              GPIO_NUM_18
#define Y_DIRECTION_PIN         GPIO_NUM_19
#define Y2_STEP_PIN             GPIO_NUM_21
#define Y2_DIRECTION_PIN        GPIO_NUM_22
#define Z_STEP_PIN              GPIO_NUM_23
#define Z_DIRECTION_PIN         GPIO_NUM_25

// #define STEPPERS_DISABLE_PIN    GPIO_NUM_

#define SPINDLE_TYPE            SpindleType::HUANYANG
#define VFD_RS485_TXD_PIN       GPIO_NUM_26
#define VFD_RS485_RXD_PIN       GPIO_NUM_27
#define VFD_RS485_RTS_PIN       GPIO_NUM_32
#define VFD_RS485_ADDR          1
#define DEFAULT_SPINDLE_RPM_MIN 8000.0
#define DEFAULT_SPINDLE_RPM_MAX 24000.0
#define DEFAULT_SPINDLE_DELAY_SPINUP 10000
#define DEFAULT_SPINDLE_DELAY_SPINDOWN 5000

// To debug RS485 spindle comms:
// #define VFD_DEBUG_MODE

#define X_LIMIT_PIN             GPIO_NUM_34
#define Y_LIMIT_PIN             GPIO_NUM_35
#define Y2_LIMIT_PIN            GPIO_NUM_4
#define Z_LIMIT_PIN             GPIO_NUM_36
#define DEFAULT_INVERT_LIMIT_PINS   0

#define ESTOP_PIN               GPIO_NUM_13

// #define PROBE_PIN               GPIO_NUM_33

// #define CONTROL_SAFETY_DOOR_PIN GPIO_NUM_39
// #define CONTROL_RESET_PIN       GPIO_NUM_5
// #define CONTROL_FEED_HOLD_PIN   GPIO_NUM_2
// #define CONTROL_CYCLE_START_PIN GPIO_NUM_0

// The default value in config.h is wrong for this controller
#ifdef INVERT_CONTROL_PIN_MASK
    #undef INVERT_CONTROL_PIN_MASK
#endif
#define INVERT_CONTROL_PIN_MASK B1110


// ***** MACHINE BOUNDS *****
#define DEFAULT_X_MAX_TRAVEL 800.0 // mm
#define DEFAULT_Y_MAX_TRAVEL 1275.0 // mm
#define DEFAULT_Z_MAX_TRAVEL 118.0 // mm

#define DEFAULT_SOFT_LIMIT_ENABLE 1
#define DEFAULT_HARD_LIMIT_ENABLE 1


// ***** MOVEMENT *****
// Invert Y axis
#define DEFAULT_DIRECTION_INVERT_MASK 0b000010

#define DEFAULT_X_STEPS_PER_MM 100.0
#define DEFAULT_Y_STEPS_PER_MM 100.0
#define DEFAULT_Z_STEPS_PER_MM 100.0

#define DEFAULT_X_MAX_RATE 4000.0 // mm/min
#define DEFAULT_Y_MAX_RATE 4000.0 // mm/min
#define DEFAULT_Z_MAX_RATE 3000.0 // mm/min

#define DEFAULT_X_ACCELERATION 500.0 // mm/sec^2
#define DEFAULT_Y_ACCELERATION 500.0 // mm/sec^2
#define DEFAULT_Z_ACCELERATION 100.0 // mm/sec^2


#define PARKING_ENABLE  // Default disabled. Uncomment to enable
#define PARKING_AXIS Z_AXIS                      // Define which axis that performs the parking motion


#define DEFAULT_HOMING_ENABLE           1
#define DEFAULT_HOMING_SQUARED_AXES     (bit(Y_AXIS))
#define DEFAULT_HOMING_DIR_MASK         (bit(X_AXIS))
#define DEFAULT_HOMING_FEED_RATE        100.0 // mm/min
#define DEFAULT_HOMING_SEEK_RATE        2000.0 // mm/min
#define DEFAULT_HOMING_DEBOUNCE_DELAY   25 // msec (0-65k)
#define DEFAULT_HOMING_PULLOFF          1.0 // mm

#define DEFAULT_HOMING_CYCLE_0 (bit(Z_AXIS))  // Raise/home Z before XY motion
#define DEFAULT_HOMING_CYCLE_1 (bit(X_AXIS))  // Home X
#define DEFAULT_HOMING_CYCLE_2 (bit(Y_AXIS))  // Home and sqaure the Y axis (must be done separately from X for squaring)
#define DEFAULT_HOMING_CYCLE_3 0
