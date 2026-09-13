#include "ConfigService.h"

#include <Preferences.h>

static constexpr const char* kConfigNamespace = "device_cfg";

void ConfigService::initializeConfig() {
    Preferences preferences;

    preferences.begin(kConfigNamespace, false);
    
    mDeviceConfig.deviceApPassword = preferences.getString("ap_password", "");
    mDeviceConfig.wifiSsid = preferences.getString("wifi_ssid", "");
    mDeviceConfig.wifiPassword = preferences.getString("wifi_password", "");
    mDeviceConfig.wsHost = preferences.getString("ws_host", "");
    mDeviceConfig.wsPort = preferences.getUInt("ws_port", 0);
    mDeviceConfig.authKey = preferences.getString("auth_key", "");

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
    preferences.putString("ws_host", config.wsHost);
    preferences.putUInt("ws_port", config.wsPort);
    preferences.putString("auth_key", config.authKey);
    preferences.end();

    mDeviceConfig = config;
}