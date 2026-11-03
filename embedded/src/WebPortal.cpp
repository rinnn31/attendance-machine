#include "WebPortal.h"

#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_log.h>

#include "ConfigService.h"
#include "BoardConfig.h"
#include "Display.h"

static constexpr const char* kTag = "WebPortal";

void WebPortal::begin() {
    Display::getInstance().showBootMode(Display::BootType::Configuration);

    ESP_LOGI(kTag, "Starting WiFi in AP mode...");
    // Init wifi softAP
    String apPassword = ConfigService::getInstance()->getDeviceConfig().deviceApPassword;
    if (apPassword.isEmpty()) {
        apPassword = BoardConfig::kDefaultSoftApPassword;
        ESP_LOGW(kTag, "AP password is empty. Using default password: %s", BoardConfig::kDefaultSoftApPassword);
    }

    WiFi.mode(WIFI_AP);
    WiFi.softAP(BoardConfig::kDeviceSerial, apPassword.c_str());
    
    mWebServer = std::make_unique<AsyncWebServer>(80);

    ESP_LOGI(kTag, "Starting web server on port 80...");
    // Register callbacks for API endpoints
    mWebServer->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleReadConfig(request);
    });
    mWebServer->on(
        "/api/config",
        HTTP_POST,
        [this](AsyncWebServerRequest *request) {
            String *body = static_cast<String*>(request->_tempObject);
            request->_tempObject = nullptr;
            
            if (body) {
                handleSaveConfig(request, body);
                delete body;
            } else {
                request->send(400, "application/json", "{\"status\":\"error\", \"msg\":\"Dữ liệu không hợp lệ\"}");
            }
        },
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String* body = nullptr;
            if (index == 0) {
                body = new String();
                body->reserve(total);
                request->_tempObject = body;

                // Handle request disconnect to clean up the temporary object if the request is aborted
                request->onDisconnect([request]() {
                    if (request->_tempObject) {
                        String *body = static_cast<String*>(request->_tempObject);
                        delete body;
                        request->_tempObject = nullptr;
                    }
                });
            } else {
                body = static_cast<String*>(request->_tempObject);
            }
    
            body->concat(reinterpret_cast<const char*>(data), len);
        }
    );
    mWebServer->on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleRestartDeviceAction(request);
    });

    // Serve static files from LittleFS
    mWebServer->serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("max-age=9600");
    mWebServer->onNotFound([](AsyncWebServerRequest *request) {
        request->send(404);
    });

    mWebServer->begin();

    Display::getInstance().showConfigMode(BoardConfig::kDeviceSerial, WiFi.softAPIP().toString());
    ESP_LOGI(kTag, "Web portal started. Connect to WiFi SSID '%s' and access the web portal at http://%s", BoardConfig::kDeviceSerial, WiFi.softAPIP().toString().c_str());
}

void WebPortal::handleReadConfig(AsyncWebServerRequest *request) {
    DeviceConfiguration config = ConfigService::getInstance()->getDeviceConfig();

    JsonDocument jsonDoc;
    jsonDoc["deviceApPassword"] = config.deviceApPassword;
    jsonDoc["wifiSsid"] = config.wifiSsid;
    jsonDoc["wifiPassword"] = config.wifiPassword;
    jsonDoc["wsHost"] = config.wsHost;
    jsonDoc["wsPort"] = config.wsPort;
    jsonDoc["authKey"] = config.authKey;

    String jsonResponse;
    serializeJson(jsonDoc, jsonResponse);

    request->send(200, "application/json", jsonResponse);
}

void WebPortal::handleSaveConfig(AsyncWebServerRequest *request, String *jsonString) {
    if (!jsonString || jsonString->isEmpty()) {
        request->send(400, "application/json", "{\"status\":\"error\", \"msg\":\"Dữ liệu không hợp lệ\"}");
        return;
    }

    JsonDocument jsonDoc;
    DeserializationError error = deserializeJson(jsonDoc, *jsonString);
    if (error || !jsonDoc.is<JsonObject>()) {
        request->send(400, "application/json", "{\"status\":\"error\", \"msg\":\"Dữ liệu không hợp lệ\"}");
        return;
    }

    DeviceConfiguration newConfig = ConfigService::getInstance()->getDeviceConfig();
    newConfig.deviceApPassword = jsonDoc["deviceApPassword"].as<String>();
    newConfig.wifiSsid = jsonDoc["wifiSsid"].as<String>();
    newConfig.wifiPassword = jsonDoc["wifiPassword"].as<String>();
    newConfig.wsHost = jsonDoc["wsHost"].as<String>();
    newConfig.wsPort = jsonDoc["wsPort"].as<uint16_t>();
    ConfigService::getInstance()->saveConfig(newConfig);

    request->send(200, "application/json", "{\"status\":\"success\"}");
}

void WebPortal::handleRestartDeviceAction(AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"status\":\"success\"}");
    
    xTaskCreate([](void *param) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP.restart();
        vTaskDelete(NULL);

    }, "RestartTask", 2048, nullptr, 1, nullptr);
}