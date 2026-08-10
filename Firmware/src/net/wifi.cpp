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

#include "net/wifi.h"

#include <Preferences.h>
#include <WiFi.h>

#include "esp_netif.h"
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

static void force_public_dns() {
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta) return;
    esp_netif_dns_info_t dns = {};
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = esp_ip4addr_aton("8.8.8.8");
    esp_netif_set_dns_info(sta, ESP_NETIF_DNS_MAIN,   &dns);
    dns.ip.u_addr.ip4.addr = esp_ip4addr_aton("1.1.1.1");
    esp_netif_set_dns_info(sta, ESP_NETIF_DNS_BACKUP, &dns);
}

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

    #ifdef PRINT_DEBUG
        Serial.println("Wi-Fi credentials cleared from NVS");
    #endif
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
        force_public_dns();

        #ifdef PRINT_DEBUG
            Serial.print("Wi-Fi connected, IP: ");
            Serial.println(WiFi.localIP());

            Serial.print("DNS1: ");
            Serial.println(WiFi.dnsIP(0));
            Serial.print("DNS2: ");
            Serial.println(WiFi.dnsIP(1));

            IPAddress ip;
            if (WiFi.hostByName("example.com", ip)) {
                Serial.print("example.com -> ");
                Serial.println(ip);
            } else {
                Serial.println("example.com DNS failed");
            }

            if (WiFi.hostByName("ryans-cm5.tail687bb0.ts.net", ip)) {
                Serial.print("server -> ");
                Serial.println(ip);
            } else {
                Serial.println("server DNS failed");
            }
        #endif

        return true;
    }
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
