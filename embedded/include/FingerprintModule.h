#ifndef FINGERPRINT_MODULE_H
#define FINGERPRINT_MODULE_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <stdint.h>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <Adafruit_Fingerprint.h>

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
    static void sensorStateCheckingTask(void* arg);
    static void IRAM_ATTR onSensorWakeupInterrupt(void* arg);

    bool readExact(uint8_t* buffer, size_t length, uint32_t timeoutMs);
    bool readDataPacket(uint8_t* payload, size_t payloadCapacity, size_t* payloadLength, uint8_t* pid, uint32_t timeoutMs);

    SensorStateChangedCallback mSensorStateChangedCallback;
    TaskHandle_t mSensorStateTaskHandle;

    HardwareSerial mSerial;
    Adafruit_Fingerprint* mFinger;
    bool mIsSensorReady;

    SemaphoreHandle_t mTouchSem;

    uint8_t* mImageBuffer;
};

#endif // FINGERPRINT_MODULE_H