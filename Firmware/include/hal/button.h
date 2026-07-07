/**************************************************************************************************/
/**
 * @file button.h
 * @author  Ryan Jing
 * @brief Debounced push-button input and the mood-select / provisioning gesture handlers.
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

#define MOOD_SET_TIMER 5
#define BLE_SET_TIMER 10
#define WIFI_CLEAR_TIMER 20

/*------------------------------------------------------------------------------------------------*/
// CLASS DECLARATIONS                                                                             */
/*------------------------------------------------------------------------------------------------*/

enum ButtonState
{
    BUTTON_PRESSED,
    BUTTON_RELEASED
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
 * @brief Configure the button GPIO as an input.
 *
 *
 *
 */
/**************************************************************************************************/
void setup_button();

/**************************************************************************************************/
/**
 * @name
 * @brief Read the debounced button state (pressed or released).
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
 * @brief Handle long-hold user commands that must work across all lamp states.
 *
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
 * @brief Handle the button in SHOW_MOOD: hold to select a mood or enter provisioning.
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
 * @brief Handle the button in SELECT_MOOD: tap to cycle moods, hold to confirm.
 *
 *
 * @param s
 *
 */
/**************************************************************************************************/
void select_mood_button_handle(LampState &s);

#endif // HAL_BUTTON_H
