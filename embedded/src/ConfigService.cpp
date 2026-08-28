#include "ConfigService.h"

#include <Preferences.h>

static constexpr const char* kConfigNamespace = "device_cfg";

void ConfigService::initializeConfig() {
    Preferences preferences;

    preferences.begin(kConfigNamespace, false);
    mDeviceConfig.deviceApPassword = preferences.getString("ap_password", "");
    mDeviceConfig.wifiSsid = preferences.getString("wifi_ssid", "");
    mDeviceConfig.wifiPassword = preferences.getString("wifi_password", "");
    mDeviceConfig.mqttBroker = preferences.getString("mqtt_broker", "");
    mDeviceConfig.mqttUsername = preferences.getString("mqtt_username", "");
    mDeviceConfig.mqttPassword = preferences.getString("mqtt_password", "");
    mDeviceConfig.mqttPort = preferences.getUInt("mqtt_port", 1883);
    preferences.end();
}

void ConfigService::resetConfig() {
    Preferences preferences;
    preferences.begin(kConfigNamespace, false);
    preferences.clear();
    preferences.end();
}

void ConfigService::saveConfig(const DeviceConfiguration& config) {
    Preferences preferences;
    preferences.begin(kConfigNamespace, false);
    preferences.putString("ap_password", config.deviceApPassword);
    preferences.putString("wifi_ssid", config.wifiSsid);
    preferences.putString("wifi_password", config.wifiPassword);
    preferences.putString("mqtt_broker", config.mqttBroker);
    preferences.putString("mqtt_username", config.mqttUsername);
    preferences.putString("mqtt_password", config.mqttPassword);
    preferences.putUInt("mqtt_port", config.mqttPort);  
    preferences.end();

    mDeviceConfig = config;
}