#ifndef FINGERPRINT_MODULE_H
#define FINGERPRINT_MODULE_H

#include <stdint.h>
#include <functional>
#include <usb/usb_host.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

using SensorStateChangedCallback = std::function<void(bool isFingerDetected)>;

class FingerprintModule {
public:
    static constexpr uint16_t kImageWidth = 256;
    static constexpr uint16_t kImageHeight = 288;

    FingerprintModule();
    ~FingerprintModule();
    
    bool begin();
    void end();

    void onSensorStateChanged(SensorStateChangedCallback callback) { mSensorStateChangedCallback = callback; }

    bool takeImage();
    const uint8_t* getNewestRawImage();
    const uint8_t* getLastRawImage() const { return mImageBuffer; }

private:
    static void usbEventTask(void* arg);
    static void sensorStateCheckingTask(void* arg);
    static IRAM_ATTR void onSensorWakeupInterrupt(void* arg);

    bool checkPacket(const char* ctx, const uint8_t* packet, size_t length, size_t expectedLength);
    void handleClientEvent(const usb_host_client_event_msg_t *event_msg, void *arg);
    bool sendPacket(const uint8_t* data, size_t length);
    bool receivePacket(uint8_t* buffer, size_t bufferSize, size_t* actualLength, uint32_t timeoutMs);

    SensorStateChangedCallback mSensorStateChangedCallback;
    TaskHandle_t mSensorStateTaskHandle;

    usb_host_client_handle_t mClientHandle;
    usb_device_handle_t mDeviceHandle;
    TaskHandle_t mHostTaskHandle;

    bool mIsDeviceConnected;
    uint8_t mBulkOutEp;
    uint8_t mBulkInEp;

    usb_transfer_t* mOutTransfer;
    usb_transfer_t* mInTransfer;
    SemaphoreHandle_t mTransferDoneSem;

    SemaphoreHandle_t mTouchSem;

    uint8_t* mImageBuffer;
};  

#endif // FINGERPRINT_MODULE_H