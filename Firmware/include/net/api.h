/**************************************************************************************************/
/**
 * @file api.h
 * @author  Ryan Jing
 * @brief HTTP client for the mood server: publish this device's mood and poll the peer's.
 *
 * @version 0.1
 * @date 2026-07-03
 *
 * @copyright Copyright (c) 2026
 *
 */
/**************************************************************************************************/

#ifndef NET_API_H
#define NET_API_H

/*------------------------------------------------------------------------------------------------*/
// HEADERS                                                                                        */
/*------------------------------------------------------------------------------------------------*/

#include "moods.h"

/*------------------------------------------------------------------------------------------------*/
// GLOBAL VARIABLES                                                                               */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
// CLASS DECLARATIONS                                                                             */
/*------------------------------------------------------------------------------------------------*/

enum ApiResult {
    API_OK,
    API_NOT_MODIFIED,
    API_NETWORK_ERROR,
    API_HTTP_ERROR,
    API_BAD_RESPONSE
};

/*------------------------------------------------------------------------------------------------*/
// FUNCTION DECLARATIONS                                                                          */
/*------------------------------------------------------------------------------------------------*/

/**************************************************************************************************/
/**
 * @name    api_wait_for_time
 * @brief   Wait for the SNTP time to be set, up to a timeout.
 *
 *
 * @param timeout_ms
 *
 * @return true
 * @return false
 */
/**************************************************************************************************/
bool api_wait_for_time(uint32_t timeout_ms);

/**************************************************************************************************/
/**
 * @name    api_post_mood
 * @brief   Send the current mood to the server via HTTP POST.
 *
 *
 * @param mood
 *
 * @return ApiResult
 */
/**************************************************************************************************/
ApiResult api_post_mood(Moods mood);

/**************************************************************************************************/
/**
 * @name    api_get_peer_mood
 * @brief   Poll the server for the peer's latest mood via HTTP GET.
 *          If the peer's mood has not changed since known_version, return API_NOT_MODIFIED.
 *
 *
 * @param known_version
 * @param peer_mood
 * @param new_version
 *
 * @return ApiResult
 */
/**************************************************************************************************/
ApiResult api_get_peer_mood(uint32_t known_version, Moods &peer_mood, uint32_t &new_version);


#endif // NET_API_H
