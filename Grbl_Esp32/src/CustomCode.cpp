// This file loads custom code from the Custom/ subdirectory if
// CUSTOM_CODE_FILENAME is defined.

#include "Grbl.h"

#ifdef CUSTOM_CODE_FILENAME
#    include CUSTOM_CODE_FILENAME
#endif

#include <TFT_eSPI.h>

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
            strcat(status, "|Pn:");
            if (prb_pin_state) {
                strcat(status, "P");
            }
            if (lim_pin_state) {
                auto n_axis = number_axis->get();
                if (n_axis >= 1 && bit_istrue(lim_pin_state, bit(X_AXIS))) {
                    strcat(status, "X");
                }
                if (n_axis >= 2 && bit_istrue(lim_pin_state, bit(Y_AXIS))) {
                    strcat(status, "Y");
                }
                if (n_axis >= 3 && bit_istrue(lim_pin_state, bit(Z_AXIS))) {
                    strcat(status, "Z");
                }
                if (n_axis >= 4 && bit_istrue(lim_pin_state, bit(A_AXIS))) {
                    strcat(status, "A");
                }
                if (n_axis >= 5 && bit_istrue(lim_pin_state, bit(B_AXIS))) {
                    strcat(status, "B");
                }
                if (n_axis >= 6 && bit_istrue(lim_pin_state, bit(C_AXIS))) {
                    strcat(status, "C");
                }
            }
            if (ctrl_pin_state.value) {
                if (ctrl_pin_state.bit.safetyDoor) {
                    strcat(status, "D");
                }
                if (ctrl_pin_state.bit.reset) {
                    strcat(status, "R");
                }
                if (ctrl_pin_state.bit.feedHold) {
                    strcat(status, "H");
                }
                if (ctrl_pin_state.bit.cycleStart) {
                    strcat(status, "S");
                }
                if (ctrl_pin_state.bit.macro0) {
                    strcat(status, "0");
                }
                if (ctrl_pin_state.bit.macro1) {
                    strcat(status, "1");
                }
                if (ctrl_pin_state.bit.macro2) {
                    strcat(status, "2");
                }
                if (ctrl_pin_state.bit.macro3) {
                    strcat(status, "3");
                }
            }
        }
        
        display_sprite.printf("%s", buf);

        display_sprite.pushSprite(0, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
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
}
