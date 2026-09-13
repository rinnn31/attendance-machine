#include <Arduino.h>
#include <nvs_flash.h>
#include <rom/rtc.h>
#include <LittleFS.h>

#include "BoardConfig.h"
#include "ConfigService.h"
#include "WebPortal.h"  
#include "MainController.h"

constexpr const char* kTag = "Main";

enum BootMode : uint8_t {
    kNormalMode = 0,
    kWebPortalMode = 1
};

BootMode gCurrentMode = kNormalMode;

bool gBtnPressed = false;
unsigned long gBtnPressStartTime = 0;

BootMode getBootMode() {
    uint32_t val = READ_PERI_REG(RTC_CNTL_STORE0_REG);
    return static_cast<BootMode>(val & 0xFF);
}

void setBootMode(BootMode mode) {
    WRITE_PERI_REG(RTC_CNTL_STORE0_REG, (READ_PERI_REG(RTC_CNTL_STORE0_REG) & 0xFFFFFF00) | static_cast<uint32_t>(mode));
}

void webPortalModeSetup() {
    static WebPortal webPortal;
    webPortal.begin();
}

void normalModeSetup() {
    static MainController mainController;
    mainController.begin();
}
 
void setup() {
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        ESP_LOGE(kTag, "Failed to mount LittleFS");
        return;
    }
    ESP_LOGI(kTag, "LittleFS mounted successfully with total space: %d bytes, used space: %d bytes", LittleFS.totalBytes(), LittleFS.usedBytes());

    // Init non-volatile configuration and configuration service
    ESP_LOGI(kTag, "Initializing NVS and configuration service...");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ConfigService::getInstance()->initializeConfig();

    pinMode(BoardConfig::kModeSwitchButtonPin, INPUT_PULLUP);

    BootMode bootMode = getBootMode();
    if (bootMode == kWebPortalMode) {
        ESP_LOGI(kTag, "Booting in Web Portal Mode");

        gCurrentMode = kWebPortalMode;
        setBootMode(kNormalMode); // Reset to Normal Mode for next boot

        webPortalModeSetup();
    } else {
        ESP_LOGI(kTag, "Booting in Normal Mode");
        gCurrentMode = kNormalMode;
        normalModeSetup();
    }
}

void loop() {
    if (digitalRead(BoardConfig::kModeSwitchButtonPin) == LOW) {
        if (!gBtnPressed) {
            ESP_LOGI(kTag, "Mode switch button pressed.");
            gBtnPressed = true;
            gBtnPressStartTime = millis();
        }
    } else if (gBtnPressed) {
        gBtnPressed = false;
        unsigned long pressDuration = millis() - gBtnPressStartTime;

        if (pressDuration >= BoardConfig::kMinResetPressDurationMs) {
            ESP_LOGI(kTag, "Reset button pressed for %lu ms. Resetting configuration.", pressDuration);
            ConfigService::getInstance()->resetConfig();
            // After resetting the configuration, switch to Web Portal Mode to allow the user to set up the device again
            setBootMode(kWebPortalMode);
            ESP.restart();
        } else if (pressDuration >= BoardConfig::kMinModeSwitchPressDurationMs) {
            if (gCurrentMode == kNormalMode) {
                ESP_LOGI(kTag, "Switching to Web Portal Mode. Button pressed for %lu ms.", pressDuration);
                setBootMode(kWebPortalMode);
            } else {
                ESP_LOGI(kTag, "Switching to Normal Mode. Button pressed for %lu ms.", pressDuration);
                setBootMode(kNormalMode);
            }

            ESP.restart();
        }
    }

    delay(50);
}