/**************************************************************************************************/
/**
 * @file wifi.h
 * @author  Ryan Jing
 * @brief Wi-Fi credentials type and NVS-backed credential accessors.
 *
 * @version 0.1
 * @date 2026-07-03
 *
 * @copyright Copyright (c) 2026
 *
 */
/**************************************************************************************************/

#ifndef NET_WIFI_H
#define NET_WIFI_H

/*------------------------------------------------------------------------------------------------*/
// HEADERS                                                                                        */
/*------------------------------------------------------------------------------------------------*/

#include <Arduino.h>

/*------------------------------------------------------------------------------------------------*/
// GLOBAL VARIABLES                                                                               */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
// CLASS DECLARATIONS                                                                             */
/*------------------------------------------------------------------------------------------------*/

typedef struct {
    char ssid[33];      // 32-byte max SSID + null terminator
    char password[64];  // WPA2 password max is 63 chars + null terminator
} WifiCredentials;

/*------------------------------------------------------------------------------------------------*/
// FUNCTION DECLARATIONS                                                                          */
/*------------------------------------------------------------------------------------------------*/

/**************************************************************************************************/
/**
 * @name get_wifi_credentials
 * @brief Load stored Wi-Fi credentials from NVS; returns false if none are saved.
 *
 * @param credentials
 *
 * @return true
 * @return false
 */
/**************************************************************************************************/
bool get_wifi_credentials(WifiCredentials &credentials);

/**************************************************************************************************/
/**
 * @name save_wifi_credentials
 * @brief Save Wi-Fi credentials to NVS.
 *
 *
 * @param credentials
 *
 */
/**************************************************************************************************/
void save_wifi_credentials(const WifiCredentials &credentials);

/**************************************************************************************************/
/**
 * @name clear_wifi_credentials
 * @brief Clear stored Wi-Fi credentials from NVS.
 *
 *
 *
 */
/**************************************************************************************************/
void clear_wifi_credentials();

/**************************************************************************************************/
/**
 * @name    wifi_connect
 * @brief   Attempt to connect to the Wi-Fi network using the provided credentials,
 *          waiting up to timeout_ms milliseconds for a connection.
 *
 *
 * @param credentials
 * @param timeout_ms
 *
 * @return true
 * @return false
 */
/**************************************************************************************************/
bool wifi_connect(const WifiCredentials &credentials, uint32_t timeout_ms);

/**************************************************************************************************/
/**
 * @name    wifi_disconnect
 * @brief   Disconnect from the current Wi-Fi network.
 *
 *
 *
 */
/**************************************************************************************************/
void wifi_disconnect();

/**************************************************************************************************/
/**
 * @name    wifi_is_connected
 * @brief   Return true if the device is currently connected to a Wi-Fi network.
 *
 *
 *
 * @return true
 * @return false
 */
/**************************************************************************************************/
bool wifi_is_connected();

#endif // NET_WIFI_H
