/**************************************************************************************************/
/**
 * @file state.cpp
 * @author  Ryan Jing
 * @brief Thread-safe shared state between the lamp and comms tasks (mutex + FreeRTOS queue).
 *
 * @version 0.1
 * @date 2026-07-06
 *
 * @copyright Copyright (c) 2026
 *
 */
/**************************************************************************************************/

/*------------------------------------------------------------------------------------------------*/
/* HEADERS                                                                                        */
/*------------------------------------------------------------------------------------------------*/

#include "state.h"

/*------------------------------------------------------------------------------------------------*/
/* MACROS                                                                                         */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES                                                                               */
/*------------------------------------------------------------------------------------------------*/

static SemaphoreHandle_t mtx;
static QueueHandle_t     outQ;
static CommsStatus       g_net_state = BLE_PROVISIONING;
static Moods             g_peer_mood  = IDLE;

/*------------------------------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                                            */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
/* FUNCTION DEFINITIONS                                                                           */
/*------------------------------------------------------------------------------------------------*/

void shared_state_init() {
    mtx  = xSemaphoreCreateMutex();
    outQ = xQueueCreate(4, sizeof(Moods));
}

void shared_set_net_state(CommsStatus comms_status) {
    xSemaphoreTake(mtx, portMAX_DELAY);
    g_net_state = comms_status;
    xSemaphoreGive(mtx);
}

CommsStatus shared_get_net_state() {
    xSemaphoreTake(mtx, portMAX_DELAY);
    CommsStatus state = g_net_state;
    xSemaphoreGive(mtx);
    return state;
}

void shared_set_peer_mood(Moods mood) {
    xSemaphoreTake(mtx, portMAX_DELAY);
    g_peer_mood = mood;
    xSemaphoreGive(mtx);
}

Moods shared_get_peer_mood() {
    xSemaphoreTake(mtx, portMAX_DELAY);
    Moods mood = g_peer_mood;
    xSemaphoreGive(mtx);
    return mood;
}

bool shared_post_mood(Moods mood) {
    return xQueueSend(outQ ,&mood, 0) == pdTRUE;
}

bool shared_get_posted_mood(Moods *mood) {
    return xQueueReceive(outQ, mood, 0) == pdTRUE;
}