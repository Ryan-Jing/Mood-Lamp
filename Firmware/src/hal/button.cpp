/**************************************************************************************************/
/**
 * @file button.cpp
 * @author  Ryan Jing
 * @brief Debounced dual-stage button input and per-state gesture handling implementation.
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

const int button_position_one = D7;  // Half-press stage (button pin 2), active low
const int button_position_two = D8;  // Full-press stage (button pin 3), active low

/*------------------------------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                                            */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
/* FUNCTION DEFINITIONS                                                                           */
/*------------------------------------------------------------------------------------------------*/

void setup_button() {
    pinMode(button_position_one, INPUT_PULLUP);
    pinMode(button_position_two, INPUT_PULLUP);
}

void get_button_state(ButtonState *state) {
    const uint32_t debounce_delay = 50;

    static int stable_one_value = HIGH;
    static int stable_two_value = HIGH;
    static int last_raw_one_value = HIGH;
    static int last_raw_two_value = HIGH;
    static uint32_t last_debounce_one_time = 0;
    static uint32_t last_debounce_two_time = 0;

    int raw_one_value = digitalRead(button_position_one);
    int raw_two_value = digitalRead(button_position_two);

    if (raw_one_value != last_raw_one_value) {
        last_raw_one_value = raw_one_value;
        last_debounce_one_time = millis();
    }

    if (raw_two_value != last_raw_two_value) {
        last_raw_two_value = raw_two_value;
        last_debounce_two_time = millis();
    }

    if ((millis() - last_debounce_one_time) > debounce_delay) {
        stable_one_value = raw_one_value;
    }

    if ((millis() - last_debounce_two_time) > debounce_delay) {
        stable_two_value = raw_two_value;
    }

    // A full press grounds both stages, so the full stage alone decides FULL_PRESSED.
    if (stable_two_value == LOW) {
        *state = BUTTON_FULL_PRESSED;
    }
    else if (stable_one_value == LOW) {
        *state = BUTTON_HALF_PRESSED;
    }
    else {
        *state = BUTTON_RELEASED;
    }
}

void update_button_event(LampState &s) {
    s.button_event = BUTTON_EVENT_NONE;

    if (s.button_state != BUTTON_RELEASED) {
        if (s.button_press_time == 0) {
            s.button_press_time = s.current_time;
        }

        // A full press passes through the half stage on the way down and back up, so
        // latch when full is first reached; release below uses it to tell a half tap
        // from a full press, and hold time is measured from full engagement.
        if (s.button_state == BUTTON_FULL_PRESSED && s.full_press_time == 0) {
            s.full_press_time = s.current_time;
        }

        return;
    }

    if (s.button_press_time == 0) {
        return;
    }

    if (s.full_press_time != 0) {
        uint32_t held = s.current_time - s.full_press_time;

        if (held >= WIFI_CLEAR_TIMER * 1000) {
            s.button_event = BUTTON_EVENT_FULL_HOLD_WIFI;
        }
        else if (held >= BLE_SET_TIMER * 1000) {
            s.button_event = BUTTON_EVENT_FULL_HOLD_BLE;
        }
        else {
            s.button_event = BUTTON_EVENT_FULL_TAP;
        }
    }
    else {
        s.button_event = BUTTON_EVENT_HALF_TAP;
    }

    s.button_press_time = 0;
    s.full_press_time = 0;
}

bool handle_user_button_commands(LampState &s) {
    switch (s.button_event) {
        case BUTTON_EVENT_FULL_HOLD_WIFI:
            shared_post_user_command(USER_COMMAND_CLEAR_WIFI);
            s.application_state = SHOW_MOOD;

            #ifdef PRINT_DEBUG
                Serial.println("User command: Wi-Fi credentials cleared");
            #endif

            return true;

        case BUTTON_EVENT_FULL_HOLD_BLE: {
            CommsStatus comms_status = shared_get_net_state();

            if (comms_status == BLE_PROVISIONING || comms_status == BLE_CONNECTED) {
                shared_post_user_command(USER_COMMAND_STOP_BLE);

                #ifdef PRINT_DEBUG
                    Serial.println("User command: BLE stopped");
                #endif
            }
            else {
                shared_post_user_command(USER_COMMAND_START_BLE);

                #ifdef PRINT_DEBUG
                    Serial.println("User command: BLE started");
                #endif
            }

            s.application_state = SHOW_MOOD;
            return true;
        }

        case BUTTON_EVENT_FULL_TAP: {
            CommsStatus comms_status = shared_get_net_state();

            // A single full tap is enough to leave BLE provisioning; no hold needed.
            if (comms_status == BLE_PROVISIONING || comms_status == BLE_CONNECTED) {
                shared_post_user_command(USER_COMMAND_STOP_BLE);
                s.application_state = SHOW_MOOD;

                #ifdef PRINT_DEBUG
                    Serial.println("User command: BLE stopped");
                #endif

                return true;
            }

            return false;
        }

        default:
            return false;
    }
}

void show_mood_button_handle(LampState &s) {
    // Half taps do nothing here; they only cycle moods in SELECT_MOOD.
    if (s.button_event == BUTTON_EVENT_FULL_TAP &&
        shared_get_net_state() == NET_CONNECTED) {
        s.application_state = SELECT_MOOD;

        #ifdef PRINT_DEBUG
            Serial.println("User command: Select mood mode");
        #endif
    }
}

void select_mood_button_handle(LampState &s) {
    if (s.button_event == BUTTON_EVENT_HALF_TAP) {
        s.self_mood = static_cast<Moods>((s.self_mood + 1) % MOOD_COUNT);

        #ifdef PRINT_DEBUG
            Serial.print("Self mood currently viewing: ");
            Serial.println(s.self_mood);
        #endif
    }
    else if (s.button_event == BUTTON_EVENT_FULL_TAP) {
        s.application_state = SHOW_MOOD;
        shared_post_mood(s.self_mood);

        #ifdef PRINT_DEBUG
            Serial.print("User command: Mood set to: ");
            Serial.println(s.self_mood);
        #endif
    }
}
