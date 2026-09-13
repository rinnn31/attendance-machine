#ifndef CONFIG_SERVICE_H
#define CONFIG_SERVICE_H

#include <stdint.h>
#include <WString.h>

struct DeviceConfiguration {
    String deviceApPassword;

    String wifiSsid;
    String wifiPassword;

    String wsHost;
    uint16_t wsPort;
    String authKey;
};

class ConfigService {
public:
    static ConfigService* getInstance() {
        static ConfigService instance;
        return &instance;
    }

    void initializeConfig();
    void resetConfig();
    void saveConfig(const DeviceConfiguration& config);

    const DeviceConfiguration& getDeviceConfig() const {
        return mDeviceConfig;
    }

private:
    DeviceConfiguration mDeviceConfig;
};

#endif // CONFIG_SERVICE_H