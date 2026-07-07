/**************************************************************************************************/
/**
 * @file state.h
 * @author  Ryan Jing
 * @brief Lamp and comms state types plus the mutex-guarded interface shared by both tasks.
 *
 * @version 0.1
 * @date 2026-07-03
 *
 * @copyright Copyright (c) 2026
 *
 */
/**************************************************************************************************/

#ifndef STATE_H
#define STATE_H

/*------------------------------------------------------------------------------------------------*/
// HEADERS                                                                                        */
/*------------------------------------------------------------------------------------------------*/

#include "moods.h"
#include "hal/button.h"
#include "net/wifi.h"

/*------------------------------------------------------------------------------------------------*/
// GLOBAL VARIABLES                                                                               */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
// CLASS DECLARATIONS                                                                             */
/*------------------------------------------------------------------------------------------------*/

enum AppState
{
    BLE_STATUS,
    NET_STATUS,
    SHOW_MOOD,
    SELECT_MOOD
};

typedef struct LampState {
    Moods self_mood;
    Moods peer_mood;
    AppState application_state;
    ButtonState button_state;
    uint32_t button_press_time;
    uint32_t current_time;
    bool button_long_press_handled;
} LampState;

enum CommsStatus
{
    BLE_PROVISIONING,
    BLE_CONNECTED,
    NET_CONNECTING,
    NET_CONNECTED,
    NET_DISCONNECTED
};

enum UserCommand
{
    USER_COMMAND_NONE,
    USER_COMMAND_START_BLE,
    USER_COMMAND_STOP_BLE,
    USER_COMMAND_CLEAR_WIFI
};

typedef struct NetState {
    CommsStatus comms_status;
    Moods peer_mood;
    Moods mood_to_post;
    WifiCredentials wifi_credentials;
    uint32_t peer_version;
    uint32_t last_peer_poll_ms;
    uint8_t wifi_retry_count;
    uint8_t poll_retry_count;
    bool has_mood_to_post;
} NetState;

/*------------------------------------------------------------------------------------------------*/
// FUNCTION DECLARATIONS                                                                          */
/*------------------------------------------------------------------------------------------------*/

/**************************************************************************************************/
/**
 * @name
 * @brief Create the mutex and queue backing the shared task interface.
 *
 *
 *
 */
/**************************************************************************************************/
void shared_state_init();

/**************************************************************************************************/
/**
 * @name
 * @brief Publish the current comms status for the lamp task to read.
 *
 *
 * @param comms_status
 *
 */
/**************************************************************************************************/
void shared_set_net_state(CommsStatus comms_status);

/**************************************************************************************************/
/**
 * @name
 * @brief Return the latest comms status.
 *
 *
 *
 * @return CommsStatus
 */
/**************************************************************************************************/
CommsStatus shared_get_net_state();

/**************************************************************************************************/
/**
 * @name
 * @brief Publish the peer's latest mood for the lamp task to display.
 *
 *
 * @param mood
 *
 */
/**************************************************************************************************/
void shared_set_peer_mood(Moods mood);

/**************************************************************************************************/
/**
 * @name
 * @brief Return the peer's latest mood.
 *
 *
 *
 * @return Moods
 */
/**************************************************************************************************/
Moods shared_get_peer_mood();

/**************************************************************************************************/
/**
 * @name
 * @brief Queue a locally-set mood for the comms task to publish.
 *
 *
 * @param mood
 *
 * @return true
 * @return false
 */
/**************************************************************************************************/
bool shared_post_mood(Moods mood);

/**************************************************************************************************/
/**
 * @name
 * @brief Dequeue a locally-set mood awaiting upload, if any.
 *
 *
 * @param mood
 *
 * @return true
 * @return false
 */
/**************************************************************************************************/
bool shared_get_posted_mood(Moods *mood);

/**************************************************************************************************/
/**
 * @name
 * @brief Queue a user-requested command for the comms task.
 *
 *
 * @param command
 *
 * @return true
 * @return false
 */
/**************************************************************************************************/
bool shared_post_user_command(UserCommand command);

/**************************************************************************************************/
/**
 * @name
 * @brief Take a queued user command, if one is pending.
 *
 *
 * @param command
 *
 * @return true
 * @return false
 */
/**************************************************************************************************/
bool shared_take_user_command(UserCommand *command);

#endif // STATE_H
