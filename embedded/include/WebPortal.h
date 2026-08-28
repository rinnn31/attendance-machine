#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

#include <ESPAsyncWebServer.h>
#include <memory>

class WebPortal {
public:
    void begin();

private:
    void handleReadConfig(AsyncWebServerRequest *request);
    void handleSaveConfig(AsyncWebServerRequest *request, String *jsonString);
    void handleRestartDeviceAction(AsyncWebServerRequest *request);

    std::unique_ptr<AsyncWebServer> mWebServer;
};

#endif // WEB_PORTAL_H