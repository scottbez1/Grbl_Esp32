// This file loads custom code from the Custom/ subdirectory if
// CUSTOM_CODE_FILENAME is defined.

#include "Grbl.h"

#ifdef CUSTOM_CODE_FILENAME
#    include CUSTOM_CODE_FILENAME
#endif

#include "driver/pcnt.h"

#include <TFT_eSPI.h>


static uint32_t flow_pulses_per_second = 0;

// Note: Display pinout is specified in platformio.ini
static TFT_eSPI display_tft = TFT_eSPI();

/** Full-size sprite used as a framebuffer */
static TFT_eSprite display_sprite = TFT_eSprite(&display_tft);

void displayTask(void * pvParameters) {
    display_tft.begin();
    display_tft.invertDisplay(1);
    display_tft.setRotation(0);

    display_sprite.setColorDepth(16);
    display_sprite.createSprite(TFT_WIDTH, TFT_HEIGHT);
    display_sprite.setFreeFont(&FreeSans9pt7b);
    display_sprite.setTextColor(0xFFFF, TFT_BLACK);

    char buf[200];
    
    while(1) {
        display_sprite.fillSprite(TFT_BLACK);
        display_sprite.setCursor(0, 40);

        buf[0] = 0;
        switch (sys.state) {
            case State::Idle:
                strncat(buf, "Idle", sizeof(buf));
                break;
            case State::Cycle:
                strncat(buf, "Run", sizeof(buf));
                break;
            case State::Hold:
                if (!(sys.suspend.bit.jogCancel)) {
                    strncat(buf, "Hold", sizeof(buf));
                    strncat(buf, sys.suspend.bit.holdComplete ? "(ready)" : "(not ready)", sizeof(buf));  // Ready to resume
                    break;
                }  // Continues to print jog state during jog cancel.
            case State::Jog:
                strncat(buf, "Jog", sizeof(buf));
                break;
            case State::Homing:
                strncat(buf, "Home", sizeof(buf));
                break;
            case State::Alarm:
                strncat(buf, "Alarm", sizeof(buf));
                break;
            case State::CheckMode:
                strncat(buf, "Check", sizeof(buf));
                break;
            case State::SafetyDoor:
                strncat(buf, "Door", sizeof(buf));
                if (sys.suspend.bit.initiateRestore) {
                    strncat(buf, "(restoring)", sizeof(buf));  // Restoring
                } else {
                    if (sys.suspend.bit.retractComplete) {
                        strncat(buf, sys.suspend.bit.safetyDoorAjar ? "(ajar)" : "(ready)", sizeof(buf));  // Door ajar
                        // Door closed and ready to resume
                    } else {
                        strncat(buf, "(retracting)", sizeof(buf));  // Retracting
                    }
                }
                break;
            case State::Sleep:
                strncat(buf, "Sleep", sizeof(buf));
                break;
        }

        AxisMask    lim_pin_state  = limits_get_state();
        ControlPins ctrl_pin_state = system_control_get_state();
        bool        prb_pin_state  = probe_get_state();
        if (lim_pin_state || ctrl_pin_state.value || prb_pin_state) {
            strncat(buf, "\nPn:", sizeof(buf));
            if (prb_pin_state) {
                strncat(buf, "P", sizeof(buf));
            }
            if (lim_pin_state) {
                auto n_axis = number_axis->get();
                if (n_axis >= 1 && bit_istrue(lim_pin_state, bit(X_AXIS))) {
                    strncat(buf, "X", sizeof(buf));
                }
                if (n_axis >= 2 && bit_istrue(lim_pin_state, bit(Y_AXIS))) {
                    strncat(buf, "Y", sizeof(buf));
                }
                if (n_axis >= 3 && bit_istrue(lim_pin_state, bit(Z_AXIS))) {
                    strncat(buf, "Z", sizeof(buf));
                }
                if (n_axis >= 4 && bit_istrue(lim_pin_state, bit(A_AXIS))) {
                    strncat(buf, "A", sizeof(buf));
                }
                if (n_axis >= 5 && bit_istrue(lim_pin_state, bit(B_AXIS))) {
                    strncat(buf, "B", sizeof(buf));
                }
                if (n_axis >= 6 && bit_istrue(lim_pin_state, bit(C_AXIS))) {
                    strncat(buf, "C", sizeof(buf));
                }
            }
            if (ctrl_pin_state.value) {
                if (ctrl_pin_state.bit.safetyDoor) {
                    strncat(buf, "Door", sizeof(buf));
                }
                if (ctrl_pin_state.bit.reset) {
                    strncat(buf, "Res", sizeof(buf));
                }
                if (ctrl_pin_state.bit.feedHold) {
                    strncat(buf, "Hold", sizeof(buf));
                }
                if (ctrl_pin_state.bit.cycleStart) {
                    strncat(buf, "Sta", sizeof(buf));
                }
                if (ctrl_pin_state.bit.macro0) {
                    strncat(buf, "0", sizeof(buf));
                }
                if (ctrl_pin_state.bit.macro1) {
                    strncat(buf, "1", sizeof(buf));
                }
                if (ctrl_pin_state.bit.macro2) {
                    strncat(buf, "2", sizeof(buf));
                }
                if (ctrl_pin_state.bit.macro3) {
                    strncat(buf, "3", sizeof(buf));
                }
            }
        }

        display_sprite.printf("%s\n%u", buf, flow_pulses_per_second);

        display_sprite.pushSprite(0, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void pulseTask(void * pvParameters) {
    /* Prepare configuration for the PCNT unit */
    pcnt_config_t pcnt_config = {
        // Set PCNT input signal and control GPIOs
        .pulse_gpio_num = SPINDLE_COOLANT_FLOW_PULSE_PIN,
        .ctrl_gpio_num = -1,
        // What to do when control input is low or high?
        .lctrl_mode = PCNT_MODE_KEEP,    // Keep the primary counter mode if low
        .hctrl_mode = PCNT_MODE_KEEP,    // Keep the primary counter mode if high
        // What to do on the positive / negative edge of pulse input?
        .pos_mode = PCNT_COUNT_INC,   // Count up on the positive edge
        .neg_mode = PCNT_COUNT_DIS,   // Keep the counter value on the negative edge
        // Set the maximum and minimum limit values to watch
        .counter_h_lim = INT16_MAX,
        .counter_l_lim = INT16_MIN,
        .unit = SPINDLE_COOLANT_FLOW_PULSE_UNIT,
        .channel = PCNT_CHANNEL_0,
    };
    /* Initialize PCNT unit */
    pcnt_unit_config(&pcnt_config);

    /* Configure and enable the input filter */
    pcnt_set_filter_value(SPINDLE_COOLANT_FLOW_PULSE_UNIT, 100);
    pcnt_filter_enable(SPINDLE_COOLANT_FLOW_PULSE_UNIT);

    /* Initialize PCNT's counter */
    pcnt_counter_pause(SPINDLE_COOLANT_FLOW_PULSE_UNIT);
    pcnt_counter_clear(SPINDLE_COOLANT_FLOW_PULSE_UNIT);

    /* Everything is set up, now go to counting */
    pcnt_counter_resume(SPINDLE_COOLANT_FLOW_PULSE_UNIT);

    unsigned long last_reading = millis();
    int16_t pulse_count = 0;
    while (1) {
        unsigned long now = millis();
        pcnt_get_counter_value(SPINDLE_COOLANT_FLOW_PULSE_UNIT, &pulse_count);
        pcnt_counter_clear(SPINDLE_COOLANT_FLOW_PULSE_UNIT);

        uint32_t delta = now - last_reading;
        last_reading = now;

        if (delta > 0) {
            flow_pulses_per_second = pulse_count * 1000  / delta;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void machine_init() {
    assert(xTaskCreatePinnedToCore(
            displayTask,         // task
            "DisplayTask",  // name for task
            2048,                 // size of task stack
            nullptr,                 // parameters
            1,                    // priority
            nullptr,
            0  // off the main core (1) - see SUPPORT_TASK_CORE
            ) == pdPASS);
    assert(xTaskCreatePinnedToCore(
            pulseTask,         // task
            "PulseTask",  // name for task
            1024,                 // size of task stack
            nullptr,                 // parameters
            1,                    // priority
            nullptr,
            0  // off the main core (1) - see SUPPORT_TASK_CORE
            ) == pdPASS);
}
