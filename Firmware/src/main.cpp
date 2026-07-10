/**************************************************************************************************/
/**
 * @file main.cpp
 * @author  Ryan Jing
 * @brief Firmware entry point: initialises hardware and spawns the lamp and comms tasks.
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

#include "moods.h"
#include "state.h"
#include "config.h"
#include "hal/led.h"
#include "hal/button.h"
#include "net/wifi.h"
#include "net/ble.h"
#include "net/api.h"

/*------------------------------------------------------------------------------------------------*/
/* MACROS                                                                                         */
/*------------------------------------------------------------------------------------------------*/

#define LAMP_TASK_STACK_SIZE 4096
#define COMMS_TASK_STACK_SIZE 16384

/*------------------------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES                                                                               */
/*------------------------------------------------------------------------------------------------*/



/*----------------------------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                                            */
/*------------------------------------------------------------------------------------------------*/

/**************************************************************************************************/
/**
 * @name
 * @brief Initialise the lamp task's state and its hardware (button and LED).
 *
 *
 *
 */
/**************************************************************************************************/
void lamp_application_init(LampState &lamp_state);

/**************************************************************************************************/
/**
 * @name
 * @brief Initialise the comms task's state and choose the starting connection status.
 *
 *
 * @param net_state
 *
 */
/**************************************************************************************************/
void net_application_init(NetState &net_state);

/**************************************************************************************************/
/**
 * @name
 * @brief FreeRTOS task: run the button/LED state machine and render the current mood.
 *
 *
 * @param pvParameters
 *
 */
/**************************************************************************************************/
void vLampTask(void *pvParameters);

/**************************************************************************************************/
/**
 * @name
 * @brief FreeRTOS task: manage BLE provisioning, Wi-Fi, and server sync.
 *
 *
 * @param pvParameters
 *
 */
/**************************************************************************************************/
void vCommsTask(void *pvParameters);

/*------------------------------------------------------------------------------------------------*/
/* FUNCTION DEFINITIONS                                                                           */
/*------------------------------------------------------------------------------------------------*/

static void set_shared_comms_status(NetState &set_net_state, CommsStatus set_comms_status) {
    if (set_comms_status == BLE_PROVISIONING) {
        set_net_state.ble_provisioning_started_ms = millis();
    }
    else {
        set_net_state.ble_provisioning_started_ms = 0;
    }

    set_net_state.comms_status = set_comms_status;
    shared_set_net_state(set_comms_status);
}

static bool has_saved_wifi_credentials(NetState &net_state) {
    return get_wifi_credentials(net_state.wifi_credentials);
}

static void stop_ble_and_resume_network(NetState &net_state) {
    if (net_state.comms_status == BLE_PROVISIONING ||
        net_state.comms_status == BLE_CONNECTED) {
        ble_provisioning_stop();
    }

    net_state.wifi_retry_count = 0;
    net_state.poll_retry_count = 0;

    if (has_saved_wifi_credentials(net_state)) {
        set_shared_comms_status(net_state, NET_CONNECTING);
    }
    else {
        set_shared_comms_status(net_state, NET_DISCONNECTED);
    }
}

void vLampTask(void *pvParameters) {
    LampState lamp_state;
    lamp_application_init(lamp_state);

    for (;;) {
        Moods mood_to_render = lamp_state.peer_mood;

        lamp_state.current_time = millis();
        get_button_state(&lamp_state.button_state);

        bool button_consumed = handle_user_button_commands(lamp_state);
        CommsStatus comms_status = shared_get_net_state();

        if (lamp_state.application_state == SELECT_MOOD && comms_status != NET_CONNECTED) {
            lamp_state.application_state = SHOW_MOOD;
            lamp_state.button_press_time = 0;
            lamp_state.button_long_press_handled = false;
        }

        if (lamp_state.application_state != SELECT_MOOD) {
            switch (comms_status) {
                case BLE_PROVISIONING:
                    lamp_state.application_state = BLE_STATUS;
                    break;

                case BLE_CONNECTED:
                    lamp_state.application_state = BLE_STATUS;
                    break;

                case NET_DISCONNECTED:
                    lamp_state.application_state = SHOW_MOOD;
                    break;

                case NET_CONNECTING:
                    lamp_state.application_state = SHOW_MOOD;
                    break;

                case NET_CONNECTED:
                    lamp_state.application_state = SHOW_MOOD;
                    break;

                default:
                    lamp_state.application_state = SHOW_MOOD;
                    break;
            }
        }

        switch (lamp_state.application_state) {

            #ifdef PRINT_DEBUG
                Serial.print("Current application state: ");
                Serial.println(lamp_state.application_state);
            #endif

            case BLE_STATUS:
                mood_to_render = BLE;
                break;

            case NET_STATUS:
                mood_to_render = NO_WIFI;
                break;

            case SHOW_MOOD:
                if (!button_consumed) {
                    show_mood_button_handle(lamp_state);
                }

                if (comms_status == NET_CONNECTED) {
                    lamp_state.peer_mood = shared_get_peer_mood();
                }
                else if (comms_status == NET_DISCONNECTED) {
                    lamp_state.peer_mood = IDLE;
                }

                mood_to_render = lamp_state.peer_mood;
                break;

            case SELECT_MOOD:
                if (!button_consumed) {
                    select_mood_button_handle(lamp_state);
                }

                mood_to_render = lamp_state.self_mood;
                break;
            }

        led_render(mood_to_render);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void vCommsTask(void *pvParameters) {
    NetState net_state;
    net_application_init(net_state);

    if (net_state.comms_status == BLE_PROVISIONING) {
        ble_provisioning_start();
    }

    for (;;) {
        UserCommand user_command;
        if (shared_take_user_command(&user_command)) {

            #ifdef PRINT_DEBUG
                Serial.print("User command received: ");
                Serial.println(user_command);
            #endif

            switch (user_command) {
                case USER_COMMAND_START_BLE:
                    if (net_state.comms_status != BLE_PROVISIONING &&
                        net_state.comms_status != BLE_CONNECTED) {
                        wifi_disconnect();
                        ble_provisioning_start();
                        set_shared_comms_status(net_state, BLE_PROVISIONING);
                    }
                    break;

                case USER_COMMAND_STOP_BLE:
                    stop_ble_and_resume_network(net_state);
                    break;

                case USER_COMMAND_CLEAR_WIFI:
                    if (net_state.comms_status == BLE_PROVISIONING ||
                        net_state.comms_status == BLE_CONNECTED) {
                        ble_provisioning_stop();
                    }

                    clear_wifi_credentials();
                    wifi_disconnect();
                    ble_provisioning_start();
                    net_state.wifi_retry_count = 0;
                    net_state.poll_retry_count = 0;
                    set_shared_comms_status(net_state, BLE_PROVISIONING);
                    break;

                default:
                    break;
            }
        }

        switch (net_state.comms_status) {
            case BLE_PROVISIONING:
                if (ble_is_connected()) {
                    set_shared_comms_status(net_state, BLE_CONNECTED);
                }
                else if (millis() - net_state.ble_provisioning_started_ms >=
                         BLE_PROVISIONING_TIMEOUT_MS) {
                    stop_ble_and_resume_network(net_state);
                }
                break;

            case BLE_CONNECTED:
                if (ble_take_credentials(net_state.wifi_credentials)) {
                    save_wifi_credentials(net_state.wifi_credentials);

                    ble_set_status("connecting");
                    vTaskDelay(pdMS_TO_TICKS(300));
                    ble_provisioning_stop();

                    set_shared_comms_status(net_state, NET_CONNECTING);

                    #ifdef PRINT_DEBUG
                        Serial.println("Wi-Fi credentials saved via BLE");
                        Serial.print("SSID: ");
                        Serial.println(net_state.wifi_credentials.ssid);
                        Serial.print("Password: ");
                        Serial.println(net_state.wifi_credentials.password);
                    #endif

                } else if (!ble_is_connected()) {
                    set_shared_comms_status(net_state, BLE_PROVISIONING);
                }
                break;

            case NET_CONNECTING:
                if (wifi_connect(net_state.wifi_credentials, WIFI_CONNECT_TIMEOUT_MS)) {
                    if (!api_wait_for_time(SNTP_TIMEOUT_MS)) {
                        wifi_disconnect();
                        net_state.wifi_retry_count++;

                        if (net_state.wifi_retry_count >= WIFI_MAX_RETRIES_BEFORE_BLE) {
                            set_shared_comms_status(net_state, NET_DISCONNECTED);
                        }

                        else {
                            vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_DELAY_MS));
                        }

                        break;
                    }

                    set_shared_comms_status(net_state, NET_CONNECTED);
                    net_state.last_peer_poll_ms = 0;
                    net_state.poll_retry_count = 0;
                    net_state.wifi_retry_count = 0;

                    #ifdef PRINT_DEBUG
                        Serial.println("Wi-Fi connected");
                    #endif
                }

                else {
                    wifi_disconnect();
                    net_state.wifi_retry_count++;

                    if (net_state.wifi_retry_count >= WIFI_MAX_RETRIES_BEFORE_BLE) {
                        set_shared_comms_status(net_state, NET_DISCONNECTED);
                    }

                    else {
                        vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_DELAY_MS));
                    }
                }
                break;

            case NET_CONNECTED: {
                if (!wifi_is_connected()) {
                    wifi_disconnect();
                    set_shared_comms_status(net_state, NET_CONNECTING);
                    break;
                }

                Moods posted_mood;

                if (shared_get_posted_mood(&posted_mood)) {
                    net_state.mood_to_post = posted_mood;
                    net_state.has_mood_to_post = true;
                }

                if (net_state.has_mood_to_post) {
                    if (api_post_mood(net_state.mood_to_post) == API_OK) {
                        net_state.has_mood_to_post = false;
                        net_state.poll_retry_count = 0;

                        #ifdef PRINT_DEBUG
                            Serial.print("Mood posted successfully: ");
                            Serial.println(net_state.mood_to_post);
                        #endif
                    }

                    else {
                        net_state.poll_retry_count++;
                    }
                }

                uint32_t now = millis();

                if (net_state.last_peer_poll_ms == 0 ||
                    now - net_state.last_peer_poll_ms >= PEER_POLL_INTERVAL_MS) {
                    net_state.last_peer_poll_ms = now;

                    Moods peer_mood;
                    uint32_t new_version = net_state.peer_version;

                    ApiResult result = api_get_peer_mood(net_state.peer_version, peer_mood, new_version);

                    if (result == API_OK) {
                        net_state.peer_mood = peer_mood;
                        net_state.peer_version = new_version;
                        shared_set_peer_mood(peer_mood);
                        net_state.poll_retry_count = 0;

                        #ifdef PRINT_DEBUG
                            Serial.print("Showing new peer mood updated: ");
                            Serial.println(peer_mood);
                        #endif
                    }

                    else if (result == API_NOT_MODIFIED) {
                        net_state.poll_retry_count = 0;
                    }

                    else {
                        net_state.poll_retry_count++;
                    }
                }

                if (net_state.poll_retry_count >= API_RETRY_LIMIT) {
                    wifi_disconnect();
                    set_shared_comms_status(net_state, NET_CONNECTING);
                }
                break;
            }

            case NET_DISCONNECTED:
                break;

            default:
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void lamp_application_init(LampState &lamp_state) {
    setup_button();
    led_init();
    lamp_state.self_mood = MOOD_1;
    lamp_state.peer_mood = shared_get_peer_mood();
    lamp_state.application_state = SHOW_MOOD; // Show default mood pattern on startup
    lamp_state.button_state = BUTTON_RELEASED;
    lamp_state.button_press_time = 0;
    lamp_state.current_time = millis();
    lamp_state.button_long_press_handled = false;
}

void net_application_init(NetState &net_state) {
    net_state.peer_mood = MOOD_1;
    net_state.mood_to_post = MOOD_1;
    net_state.peer_version = 0;
    net_state.last_peer_poll_ms = 0;
    net_state.ble_provisioning_started_ms = 0;
    net_state.wifi_retry_count = 0;
    net_state.poll_retry_count = 0;
    net_state.has_mood_to_post = false;

    if (!get_wifi_credentials(net_state.wifi_credentials)) {
        set_shared_comms_status(net_state, BLE_PROVISIONING);
    }
    else {
        set_shared_comms_status(net_state, NET_CONNECTING);

        #ifdef PRINT_DEBUG
            Serial.println("Wi-Fi credentials found in NVS");
            Serial.print("SSID: ");
            Serial.println(net_state.wifi_credentials.ssid);
            Serial.print("Password: ");
            Serial.println(net_state.wifi_credentials.password);
        #endif
    }
}

void setup() {
    Serial.begin(115200);
    #if defined(PRINT_DEBUG)
        delay(2000);
        Serial.println("Mood Lamp Firmware v0.1");
    #endif
    shared_state_init();

    BaseType_t xLampReturned;
    BaseType_t xCommsTask;

    xLampReturned = xTaskCreate(vLampTask,
                                "Lamp Task",
                                LAMP_TASK_STACK_SIZE,
                                nullptr,
                                1,
                                nullptr);

    xCommsTask = xTaskCreate(vCommsTask,
                             "Comms Task",
                             COMMS_TASK_STACK_SIZE,
                             nullptr,
                             1,
                             nullptr);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
