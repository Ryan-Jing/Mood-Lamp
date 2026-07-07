/**************************************************************************************************/
/**
 * @file button.cpp
 * @author  Ryan Jing
 * @brief Debounced button input and per-state gesture handling implementation.
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
#include "state.h"

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
}

void get_button_state(ButtonState *state) {
    const uint32_t debounce_delay = 50;

    static ButtonState stable_state = BUTTON_RELEASED;
    static int last_raw_button_value = HIGH;
    static uint32_t last_debounce_time = 0;

    int raw_button_value = digitalRead(BUTTON_PIN);

    if (raw_button_value != last_raw_button_value) {
        last_raw_button_value = raw_button_value;
        last_debounce_time = millis();
    }

    if ((millis( ) - last_debounce_time) > debounce_delay) {
        stable_state = (raw_button_value== LOW) ? BUTTON_PRESSED : BUTTON_RELEASED;
    }

    *state = stable_state;
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
                s.application_state = BLE_STATUS;
            }
            else if (held >= MOOD_SET_TIMER * 1000) {
                s.application_state = SELECT_MOOD;
            }
        }
        s.button_press_time = 0;
    }
}

void select_mood_button_handle(LampState &s) {
    if (s.button_state == BUTTON_PRESSED) {
        if (s.button_press_time == 0) {
            s.button_press_time = s.current_time;
        }
        else if ((s.current_time - s.button_press_time) >= MOOD_SET_TIMER * 1000) {
            s.application_state = SHOW_MOOD;
            s.button_press_time = 0;
        }
    }
    else {
        if (s.button_press_time != 0) {
            s.current_mood = static_cast<Moods>((s.current_mood + 1) % MOOD_COUNT);
        }
        s.button_press_time = 0;
    }
}
