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

/*------------------------------------------------------------------------------------------------*/
/* MACROS                                                                                         */
/*------------------------------------------------------------------------------------------------*/

#define TASK_STACK_SIZE 4096

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

static void set_comms(NetState &set_net_state, CommsStatus set_comms_status) {
    set_net_state.comms_status = set_comms_status;
    shared_set_net_state(set_comms_status);
}

void vLampTask(void *pvParameters) {
    LampState lamp_state;
    lamp_application_init(lamp_state);

    for (;;) {
        lamp_state.current_time = millis();
        get_button_state(&lamp_state.button_state);

        if (lamp_state.application_state != SELECT_MOOD) {
            switch (shared_get_net_state()) {
                case BLE_PROVISIONING:
                    lamp_state.application_state = BLE_STATUS;
                    break;

                case BLE_CONNECTED:
                    lamp_state.application_state = BLE_STATUS;
                    break;

                case NET_DISCONNECTED:
                    lamp_state.application_state = NET_STATUS;
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
            case BLE_STATUS:
                lamp_state.current_mood = BLE;
                break;

            case NET_STATUS:
                lamp_state.current_mood = NO_WIFI;
                break;

            case SHOW_MOOD:
                show_mood_button_handle(lamp_state);
                lamp_state.current_mood = shared_get_peer_mood();
                break;

            case SELECT_MOOD:
                select_mood_button_handle(lamp_state);
                break;
            }

        led_render(lamp_state.current_mood);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void vCommsTask(void *pvParameters) {
    NetState net_state;
    net_application_init(net_state);

    if (net_state.comms_status == BLE_PROVISIONING) {
        ble_provisioning_start();

        #ifdef PRINT_DEBUG
            Serial.println("BLE provisioning");
        #endif
    }

    for (;;) {
        switch (net_state.comms_status) {
            case BLE_PROVISIONING:
                if (ble_is_connected()) {
                    set_comms(net_state, BLE_CONNECTED);
                }
                break;

            case BLE_CONNECTED:
                if (ble_take_credentials(net_state.wifi_credentials)) {
                    save_wifi_credentials(net_state.wifi_credentials);

                    ble_set_status("connecting");
                    vTaskDelay(pdMS_TO_TICKS(300));
                    ble_provisioning_stop();

                    set_comms(net_state, NET_CONNECTING);

                    #ifdef PRINT_DEBUG
                        Serial.println("Wi-Fi credentials saved via BLE");
                        Serial.print("SSID: ");
                        Serial.println(net_state.wifi_credentials.ssid);
                        Serial.print("Password: ");
                        Serial.println(net_state.wifi_credentials.password);
                    #endif

                } else if (!ble_is_connected()) {
                    set_comms(net_state, BLE_PROVISIONING);
                }
                break;

            case NET_CONNECTING:
                if (wifi_connect(net_state.wifi_credentials, 20000)) {
                    set_comms(net_state, NET_CONNECTED);
                    net_state.last_peer_poll_ms = 0;
                    net_state.poll_retry_count = 0;

                    #ifdef PRINT_DEBUG
                        Serial.println("Wi-Fi connected");
                    #endif
                }

                else {
                    wifi_disconnect();
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    ble_provisioning_start();
                    set_comms(net_state, BLE_PROVISIONING);

                }
                break;

            case NET_CONNECTED:
                break;

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
    lamp_state.current_mood = shared_get_peer_mood();
    lamp_state.application_state = SHOW_MOOD; // Show default mood pattern on startup
    lamp_state.button_state = BUTTON_RELEASED;
    lamp_state.button_press_time = 0;
    lamp_state.current_time = millis();
}

void net_application_init(NetState &net_state) {
    if (!get_wifi_credentials(net_state.wifi_credentials)) {
        shared_set_net_state(BLE_PROVISIONING);
        net_state.comms_status = BLE_PROVISIONING;
    }
    else {
        shared_set_net_state(NET_CONNECTING);
        net_state.comms_status = NET_CONNECTING;

        #ifdef PRINT_DEBUG
            Serial.println("Wi-Fi credentials found in NVS");
            Serial.print("SSID: ");
            Serial.println(net_state.wifi_credentials.ssid);
            Serial.print("Password: ");
            Serial.println(net_state.wifi_credentials.password);
        #endif
    }

    net_state.peer_mood = MOOD_1;
    net_state.mood_to_post = MOOD_1;
    net_state.peer_version = 0;
    net_state.last_peer_poll_ms = 0;
    net_state.poll_retry_count = 0;
    net_state.has_mood_to_post = false;
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
                                TASK_STACK_SIZE,
                                nullptr,
                                1,
                                nullptr);

    xCommsTask = xTaskCreate(vCommsTask,
                             "Comms Task",
                             TASK_STACK_SIZE,
                             nullptr,
                             1,
                             nullptr);
}

void loop() {

}
