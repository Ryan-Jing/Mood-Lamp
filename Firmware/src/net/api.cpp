/**************************************************************************************************/
/**
 * @file api.cpp
 * @author  Ryan Jing
 * @brief HTTP mood-server client implementation.
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

#include "net/api.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "config.h"
#include "secrets.h"

/*------------------------------------------------------------------------------------------------*/
/* MACROS                                                                                         */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES                                                                               */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                                            */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
/* FUNCTION DEFINITIONS                                                                           */
/*------------------------------------------------------------------------------------------------*/

static void add_auth(HTTPClient &http) {
    http.addHeader("Authorization", String("Bearer ") + DEVICE_TOKEN);
}

bool api_wait_for_time(uint32_t timeout_ms) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    uint32_t start_time_ms = millis();
    time_t now = 0;

    while ((millis() - start_time_ms) < timeout_ms) {
        time(&now);

        if (now > 1700000000) {
            return true;
        }

        delay(250);
    }

    return false;
}

ApiResult api_post_mood(Moods mood) {
    WiFiClientSecure client;
    HTTPClient http;

    client.setCACert(ROOT_CA);

    String url = String(SERVER_URL) + "/v1/mood";

    if (!http.begin(client, url)) {
        return API_NETWORK_ERROR;
    }

    http.setTimeout(API_TIMEOUT_MS);
    add_auth(http);
    http.addHeader("Content-Type", "application/json");

    JsonDocument json_doc;
    json_doc["mood_id"] = (int)mood;

    String string_body;
    serializeJson(json_doc, string_body);

    int code = http.POST(string_body);
    http.end();

    return code == HTTP_CODE_OK ? API_OK : API_HTTP_ERROR;
}

ApiResult api_get_peer_mood(uint32_t known_version, Moods &peer_mood, uint32_t &new_version) {
    WiFiClientSecure client;
    HTTPClient http;

    client.setCACert(ROOT_CA);

    String url = String(SERVER_URL) + "/v1/peer/mood";

    if (!http.begin(client, url)) {
        return API_NETWORK_ERROR;
    }

    http.setTimeout(API_TIMEOUT_MS);
    add_auth(http);

    if (known_version > 0) {
        http.addHeader("If-None-Match", "\"" + String(known_version) + "\"");
    }

    int code = http.GET();

    if (code == 304) {
        http.end();
        return API_NOT_MODIFIED;
    }

    if (code != HTTP_CODE_OK) {
        http.end();
        return API_HTTP_ERROR;
    }

    String string_body = http.getString();
    http.end();

    JsonDocument json_doc;
    if (deserializeJson(json_doc, string_body)) {
        return API_BAD_RESPONSE;
    }

    if (!json_doc["mood_id"].is<int>() || !json_doc["version"].is<int>()) {
        return API_BAD_RESPONSE;
    }

    int mood_id = json_doc["mood_id"].as<int>();
    int version = json_doc["version"].as<int>();

    if (mood_id < 0 || mood_id >= MOOD_COUNT || version <= 0) {
        return API_BAD_RESPONSE;
    }

    peer_mood = static_cast<Moods>(mood_id);
    new_version = (uint32_t)version;

    return API_OK;
}
