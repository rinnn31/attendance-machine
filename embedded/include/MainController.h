#ifndef MAIN_CONTROLLER_H
#define MAIN_CONTROLLER_H

#include <WiFi.h>
#include <Ticker.h>
#include <atomic>
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "MessageClient.h"
#include "FaceCamModule.h"  
#include "FingerprintModule.h"
#include "Display.h"

enum class DeviceState : uint8_t {
    kNormalState = 0,
    kCollectingFaceDataState = 1,
    kCollectingFingerprintDataState = 2,
};

enum class BiometricType : uint8_t {
    kFace = 0,
    kFingerprint = 1,
};

enum class AttendanceEventType : uint8_t {
    kFaceDetected,
    kFingerprintDetected
};

struct AttendanceEvent {
    AttendanceEventType type;
    uint8_t* imageBuffer;
    size_t imageSize;
    size_t width;
    size_t height;
};

class MainController {
public:
    MainController();
    ~MainController();

    void begin();

private:
    void handleWiFiEvent(WiFiEvent_t event);
    void handleMessageClientStateChange(ClientState state);
    void handleCommandReceived(const uint8_t commandId, const size_t payloadLength, const uint8_t* payload, const uint16_t requestId);

    void handleFaceStable(const uint8_t* image, size_t imageSize, size_t width, size_t height);
    void handleFacePresenceChanged(bool isFacePresent);
    void handleFingerprintSensorStateChange(bool isFingerDetected);

    bool sendAttendanceData(BiometricType type, const uint8_t* data, size_t dataSize);
    void sendDataCollectionResponse(uint16_t requestId, bool success, const uint8_t* data = nullptr, size_t dataSize = 0);

    Display::Status buildStatus();

    static void attendanceWorkerTask(void* pvParameters);
    static void updateTask(void* pvParameters);
    
    MessageClient* mMessageClient;
    FaceCamModule* mFaceCamModule;
    FingerprintModule* mFingerprintModule;

    Ticker mWifiReconnectTicker;

    std::atomic<DeviceState> mRunningMode;
    std::atomic<bool> mReady;
    std::atomic<bool> mBusy;

    std::atomic<uint16_t> mDataCollectionRequestId;

    QueueHandle_t mAttendanceQueue;
    TaskHandle_t mWorkerTaskHandle;
    SemaphoreHandle_t mAttendanceDoneSem;

    uint32_t mStartCollectionTimestamp;

    std::atomic<uint32_t> mResultHoldUntilMs{0};
};

#endif // MAIN_CONTROLLER_H