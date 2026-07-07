/**************************************************************************************************/
/**
 * @file wifi.cpp
 * @author  Ryan Jing
 * @brief Wi-Fi credential load/save (NVS) implementation.
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

#include <Preferences.h>
#include <WiFi.h>

#include "net/wifi.h"
#include "config.h"

/*------------------------------------------------------------------------------------------------*/
/* MACROS                                                                                         */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES                                                                               */
/*------------------------------------------------------------------------------------------------*/

static const char *wifi_namespace = "wifi";   // NVS namespace (must be <= 15 chars)

/*------------------------------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                                            */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
/* FUNCTION DEFINITIONS                                                                           */
/*------------------------------------------------------------------------------------------------*/

bool get_wifi_credentials(WifiCredentials &credentials) {
    credentials.ssid[0] = '\0';
    credentials.password[0] = '\0';

    Preferences prefs;

    prefs.begin(wifi_namespace, true);
    prefs.getString("ssid", credentials.ssid, sizeof(credentials.ssid));
    prefs.getString("pass", credentials.password, sizeof(credentials.password));
    prefs.end();

    return strlen(credentials.ssid) > 0;
}

void save_wifi_credentials(const WifiCredentials &credentials) {
    Preferences prefs;

    prefs.begin(wifi_namespace, false);
    prefs.putString("ssid", credentials.ssid);
    prefs.putString("pass", credentials.password);
    prefs.end();
}

void clear_wifi_credentials() {
    Preferences prefs;

    prefs.begin(wifi_namespace, false);
    prefs.clear();
    prefs.end();
}

bool wifi_connect(const WifiCredentials &credentials, uint32_t timeout_ms) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(credentials.ssid, credentials.password);

    uint32_t start_time_ms = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - start_time_ms < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    if (WiFi.status() == WL_CONNECTED) {
        #ifdef PRINT_DEBUG
            Serial.print("Wi-Fi connected, IP: ");
            Serial.println(WiFi.localIP());
        #endif

        return true;
    }

    WiFi.disconnect(true);
    return false;
}

void wifi_disconnect() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    #ifdef PRINT_DEBUG
        Serial.println("Wi-Fi disconnected");
    #endif
}

bool wifi_is_connected() {
    return WiFi.status() == WL_CONNECTED;
}
