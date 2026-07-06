/**************************************************************************************************/
/**
 * @file main.cpp
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
#include "hal/led.h"
#include "hal/button.h"
#include "moods.h"
#include "app_state.h"

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
 * @brief
 *
 *
 *
 */
/**************************************************************************************************/
void lamp_application_init(LampState &lamp_state);

/**************************************************************************************************/
/**
 * @name
 * @brief
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
 * @brief
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

void vLampTask(void *pvParameters) {
    LampState lamp_state;
    lamp_application_init(lamp_state);

    for (;;) {
        lamp_state.current_time = millis();
        get_button_state(&lamp_state.button_state);

        switch (lamp_state.application_state) {
            case PROVISIONING:
                // Handle provisioning state
                break;

            case CONNECTING:
                // Handle connecting state
                break;

            case SHOW_MOOD:
                    show_mood_button_handle(lamp_state);
                break;

            case SELECT_MOOD:
                    select_mood_button_handle(lamp_state);
                break;

            default:
                // Handle unexpected state
                break;
            }

        led_render(lamp_state.current_mood);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void vCommsTask(void *pvParameters) {
    for (;;) {
        // Handle communication tasks
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void lamp_application_init(LampState &lamp_state) {
    setup_button();
    lamp_state.current_mood = MOOD_1;
    lamp_state.application_state = SHOW_MOOD; // Show default mood pattern on startup
    lamp_state.button_state = BUTTON_RELEASED;
    lamp_state.button_press_time = 0;
    lamp_state.current_time = millis();
}

void setup() {
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
