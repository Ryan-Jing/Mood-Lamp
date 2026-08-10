/**************************************************************************************************/
/**
 * @file button.h
 * @author  Ryan Jing
 * @brief Debounced dual-stage (half/full press) button input and the mood-select /
 *        provisioning gesture handlers.
 *
 * @version 0.1
 * @date 2026-07-03
 *
 * @copyright Copyright (c) 2026
 *
 */
/**************************************************************************************************/

#ifndef HAL_BUTTON_H
#define HAL_BUTTON_H

/*------------------------------------------------------------------------------------------------*/
// HEADERS                                                                                        */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
// GLOBAL VARIABLES                                                                               */
/*------------------------------------------------------------------------------------------------*/

#define BLE_SET_TIMER 5
#define WIFI_CLEAR_TIMER 10

/*------------------------------------------------------------------------------------------------*/
// CLASS DECLARATIONS                                                                             */
/*------------------------------------------------------------------------------------------------*/

enum ButtonState
{
    BUTTON_RELEASED,
    BUTTON_HALF_PRESSED,
    BUTTON_FULL_PRESSED
};

enum ButtonEvent
{
    BUTTON_EVENT_NONE,
    BUTTON_EVENT_HALF_TAP,       // Half stage pressed and released without reaching full
    BUTTON_EVENT_FULL_TAP,       // Full stage pressed, released before BLE_SET_TIMER
    BUTTON_EVENT_FULL_HOLD_BLE,  // Full stage held >= BLE_SET_TIMER, < WIFI_CLEAR_TIMER
    BUTTON_EVENT_FULL_HOLD_WIFI  // Full stage held >= WIFI_CLEAR_TIMER
};

// Forward declaration; the full definition lives in state.h. The handlers below
// take LampState by reference, so this incomplete type is enough here -- this is what
// breaks the app_state.h <-> button.h include cycle.
struct LampState;

/*------------------------------------------------------------------------------------------------*/
// FUNCTION DECLARATIONS                                                                          */
/*------------------------------------------------------------------------------------------------*/

/**************************************************************************************************/
/**
 * @name
 * @brief Configure both button stage GPIOs as pulled-up inputs.
 *
 *
 *
 */
/**************************************************************************************************/
void setup_button();

/**************************************************************************************************/
/**
 * @name
 * @brief Read the debounced dual-stage button state (released, half, or full press).
 *
 *
 * @param state
 *
 */
/**************************************************************************************************/
void get_button_state(ButtonState *state);

/**************************************************************************************************/
/**
 * @name
 * @brief Decode the current press cycle into a ButtonEvent, emitted on release.
 *
 *
 * @param s
 *
 */
/**************************************************************************************************/
void update_button_event(LampState &s);

/**************************************************************************************************/
/**
 * @name
 * @brief Handle full-press gestures that work across all lamp states: full tap exits
 *        BLE provisioning, full hold starts/stops BLE or clears Wi-Fi credentials.
 *
 * @param s
 *
 * @return true
 * @return false
 */
/**************************************************************************************************/
bool handle_user_button_commands(LampState &s);

/**************************************************************************************************/
/**
 * @name
 * @brief Handle the button in SHOW_MOOD: full tap enters mood selection.
 *
 *
 * @param s
 *
 */
/**************************************************************************************************/
void show_mood_button_handle(LampState &s);

/**************************************************************************************************/
/**
 * @name
 * @brief Handle the button in SELECT_MOOD: half tap cycles moods, full tap confirms.
 *
 *
 * @param s
 *
 */
/**************************************************************************************************/
void select_mood_button_handle(LampState &s);

#endif // HAL_BUTTON_H
