/**************************************************************************************************/
/**
 * @file button.cpp
 * @author  Ryan Jing
 * @brief
 *
 * @version 0.1
 * @date 2026-07-03
 *
 * @copyright Copyright (c) 2026
 *
 */
/**************************************************************************************************/

/*------------------------------------------------------------------------------------------------*/
/* HEADERS                                                                                        */
/*------------------------------------------------------------------------------------------------*/

#include <Arduino.h>
#include "hal/button.h"
#include "app_state.h"

/*------------------------------------------------------------------------------------------------*/
/* MACROS                                                                                         */
/*------------------------------------------------------------------------------------------------*/

#define BUTTON_PIN 0

/*------------------------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES                                                                               */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                                            */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
/* FUNCTION DEFINITIONS                                                                           */
/*------------------------------------------------------------------------------------------------*/

void setup_button() {
    pinMode(BUTTON_PIN, INPUT);
    return;
}

void get_button_state(ButtonState *state) {
    int button_value = digitalRead(BUTTON_PIN);
    *state = (button_value == LOW) ? BUTTON_PRESSED : BUTTON_RELEASED;
    return;
}

void show_mood_button_handle(LampState &s) {
    if (s.button_state == BUTTON_PRESSED) {
        if (s.button_press_time == 0) {
            s.button_press_time = s.current_time;
        }
    }
    else {
        if (s.button_press_time != 0) {
            uint32_t held = s.current_time - s.button_press_time;

            if (held >= BLE_SET_TIMER * 1000) {
                s.application_state = PROVISIONING;
            }
            else if (held >= MOOD_SET_TIMER * 1000) {
                s.application_state = SELECT_MOOD;
            }
        }
        s.button_press_time = 0;
    }
    return;
}

void select_mood_button_handle(LampState &s) {
    if (s.button_state == BUTTON_PRESSED) {
        if (s.button_press_time == 0) {
            s.button_press_time = s.current_time;
            s.current_mood = static_cast<Moods>((s.current_mood + 1) % 10);
        }
        else if ((s.current_time - s.button_press_time) >= MOOD_SET_TIMER * 1000) {
            s.application_state = SHOW_MOOD;
        }
    }
    else {
        s.button_press_time = 0;
    }
    return;
}
