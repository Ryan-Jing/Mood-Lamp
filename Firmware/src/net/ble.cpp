/**************************************************************************************************/
/**
 * @file ble.cpp
 * @author  Ryan Jing
 * @brief BLE provisioning service implementation.
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
#include <NimBLEDevice.h>
#include "net/ble.h"

/*------------------------------------------------------------------------------------------------*/
/* MACROS                                                                                         */
/*------------------------------------------------------------------------------------------------*/

#define BLE_NAME    "MoodLamp"
#define SVC_UUID    "9a1e0000-8f4a-4b1c-9e2a-1234567890ab"
#define SSID_UUID   "9a1e0001-8f4a-4b1c-9e2a-1234567890ab"
#define PASS_UUID   "9a1e0002-8f4a-4b1c-9e2a-1234567890ab"
#define APPLY_UUID  "9a1e0003-8f4a-4b1c-9e2a-1234567890ab"
#define STATUS_UUID "9a1e0004-8f4a-4b1c-9e2a-1234567890ab"

/*------------------------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES                                                                               */
/*------------------------------------------------------------------------------------------------*/

static volatile bool    s_ble_connected = false;
static volatile bool    s_ble_apply     = false;
static WifiCredentials  s_wifi_creds     = {};
static NimBLECharacteristic *s_ble_status = nullptr;

/*------------------------------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                                            */
/*------------------------------------------------------------------------------------------------*/

class ServerCB : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *ble_server, NimBLEConnInfo &conn_info) override {
        s_ble_connected = true;
    }
    void onDisconnect(NimBLEServer *ble_server, NimBLEConnInfo &conn_info, int reason) override {
        s_ble_connected = false;
        NimBLEDevice::startAdvertising();
    }
};

class WriteCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &conn_info) override {
        NimBLEUUID uuid = c->getUUID();
        std::string val = c->getValue();

        if (uuid == NimBLEUUID(SSID_UUID)) {
            strncpy(s_wifi_creds.ssid, val.c_str(), sizeof(s_wifi_creds.ssid) - 1);
            s_wifi_creds.ssid[sizeof(s_wifi_creds.ssid) - 1] = '\0';
        }

        else if (uuid == NimBLEUUID(PASS_UUID)) {
            strncpy(s_wifi_creds.password, val.c_str(), sizeof(s_wifi_creds.password) - 1);
            s_wifi_creds.password[sizeof(s_wifi_creds.password) - 1] = '\0';
        }

        else if (uuid == NimBLEUUID(APPLY_UUID)) {
            s_ble_apply = true;
        }
    }
};

static ServerCB s_server_callback;
static WriteCB  s_write_callback;

/*------------------------------------------------------------------------------------------------*/
/* FUNCTION DEFINITIONS                                                                           */
/*------------------------------------------------------------------------------------------------*/

void ble_provisioning_start() {
    NimBLEDevice::init(BLE_NAME);
    NimBLEServer *ble_srv = NimBLEDevice::createServer();

    ble_srv->setCallbacks(&s_server_callback);
    NimBLEService *ble_svc = ble_srv->createService(SVC_UUID);
    ble_svc->createCharacteristic(SSID_UUID,  NIMBLE_PROPERTY::WRITE)->setCallbacks(&s_write_callback);
    ble_svc->createCharacteristic(PASS_UUID,  NIMBLE_PROPERTY::WRITE)->setCallbacks(&s_write_callback);
    ble_svc->createCharacteristic(APPLY_UUID, NIMBLE_PROPERTY::WRITE)->setCallbacks(&s_write_callback);
    s_ble_status = ble_svc->createCharacteristic(STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    s_ble_status->setValue("provisioning");
    ble_svc->start();

    NimBLEAdvertising *ble_adv = NimBLEDevice::getAdvertising();
    ble_adv->enableScanResponse(true);
    ble_adv->setName(BLE_NAME);
    ble_adv->addServiceUUID(SVC_UUID);
    ble_adv->start();
}

void ble_provisioning_stop() {
    NimBLEDevice::deinit(true);
    s_ble_connected = s_ble_apply = false;
    s_ble_status = nullptr;
}

bool ble_is_connected() {
    return s_ble_connected;
}

bool ble_take_credentials(WifiCredentials &out) {
    if (!s_ble_apply) {
        return false;
    }
    out = s_wifi_creds;
    s_ble_apply = false;
    return true;
}

void ble_set_status(const char *status) {
    if (s_ble_status) {
        s_ble_status->setValue(status);
        s_ble_status->notify();
    }
}

