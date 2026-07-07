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

#include "hal/button.h"

#include <Arduino.h>

#include "state.h"
#include "config.h"

/*------------------------------------------------------------------------------------------------*/
/* MACROS                                                                                         */
/*------------------------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES                                                                               */
/*------------------------------------------------------------------------------------------------*/

const int led = D10;

/*------------------------------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                                            */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
/* FUNCTION DEFINITIONS                                                                           */
/*------------------------------------------------------------------------------------------------*/

void setup_button() {
    pinMode(led, INPUT_PULLUP);
}

void get_button_state(ButtonState *state) {
    const uint32_t debounce_delay = 50;

    static ButtonState stable_state = BUTTON_RELEASED;
    static int last_raw_button_value = HIGH;
    static uint32_t last_debounce_time = 0;

    int raw_button_value = digitalRead(led);

    if (raw_button_value != last_raw_button_value) {
        last_raw_button_value = raw_button_value;
        last_debounce_time = millis();
    }

    if ((millis( ) - last_debounce_time) > debounce_delay) {
        stable_state = (raw_button_value== LOW) ? BUTTON_PRESSED : BUTTON_RELEASED;
    }

    *state = stable_state;
}

bool handle_user_button_commands(LampState &s) {
    if (s.button_state == BUTTON_PRESSED) {
        if (s.button_press_time == 0) {
            s.button_press_time = s.current_time;
        }

        uint32_t held = s.current_time - s.button_press_time;

        if (held >= BLE_SET_TIMER * 1000) {
            s.button_long_press_handled = true;
            return true;
        }

        return false;
    }

    if (s.button_press_time != 0) {
        uint32_t held = s.current_time - s.button_press_time;

        if (held >= WIFI_CLEAR_TIMER * 1000) {
            shared_post_user_command(USER_COMMAND_CLEAR_WIFI);
            s.application_state = SHOW_MOOD;
            s.button_press_time = 0;
            s.button_long_press_handled = false;
            return true;
        }

        if (held >= BLE_SET_TIMER * 1000) {
            CommsStatus comms_status = shared_get_net_state();

            if (comms_status == BLE_PROVISIONING || comms_status == BLE_CONNECTED) {
                shared_post_user_command(USER_COMMAND_STOP_BLE);
            }
            else {
                shared_post_user_command(USER_COMMAND_START_BLE);
            }

            s.application_state = SHOW_MOOD;
            s.button_press_time = 0;
            s.button_long_press_handled = false;
            return true;
        }
    }

    if (s.button_long_press_handled) {
        s.button_press_time = 0;
        s.button_long_press_handled = false;
        return true;
    }

    return false;
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

            if (held >= MOOD_SET_TIMER * 1000 &&
                held < BLE_SET_TIMER * 1000 &&
                shared_get_net_state() == NET_CONNECTED) {
                s.application_state = SELECT_MOOD;

                #ifdef PRINT_DEBUG
                    Serial.println("Entering mood selection mode");
                #endif
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
    }
    else {
        if (s.button_press_time != 0) {
            uint32_t held = s.current_time - s.button_press_time;

            if (held >= MOOD_SET_TIMER * 1000 &&
                held < BLE_SET_TIMER * 1000 &&
                shared_get_net_state() == NET_CONNECTED) {
                s.application_state = SHOW_MOOD;
                shared_post_mood(s.self_mood);

                #ifdef PRINT_DEBUG
                    Serial.print("Mood set to: ");
                    Serial.println(s.self_mood);
                #endif
            }
            else if (held < MOOD_SET_TIMER * 1000) {
                s.self_mood = static_cast<Moods>((s.self_mood + 1) % MOOD_COUNT);

                #ifdef PRINT_DEBUG
                    Serial.print("Self mood currently viewing: ");
                    Serial.println(s.self_mood);
                #endif
            }
        }
        s.button_press_time = 0;
    }
}
